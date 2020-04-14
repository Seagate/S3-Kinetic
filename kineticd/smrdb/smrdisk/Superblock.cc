/*
 * Superblock.cc
 *
 *  Created on: Nov 18, 2016
 *      Author: tri
 */

#include "Superblock.h"
#include "Disk.h"
#include "ReplayObj.h"

namespace smr {

Superblock::Superblock(Disk* disk, int superblockNumber): disk_(disk) {
    number_ = superblockNumber;
    destAddr_ = -1;
    bGood_ = false;
    seqNum_ = 0;
    current_ = NULL;
    fileMap_ = NULL;
    valueFileMap_ = NULL;
    zoneUseMap_ = NULL;
    manifestInfo_ = NULL;
    for (int i = 0; i < 10; ++i) {
        levels_[i] = NULL;
    }
    lastAllocatedZoneMapIdx_ = 0;
    lastAllocatedZone_ = 0;
    lastAllocatedConvZoneMapIdx_ = 0;
    lastAllocatedConvZone_ = 0;
    currentLogZone_ = 0;
    zoneUseMapSize_ = 0;
    zoneStartAddr_ = 0;
    numZones_ = 0;
    nBads_ = 10;
}

Superblock::~Superblock() {
    disk_ = NULL;
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Super block #" << number_
         << ": #Replay objects = " << replayObjs_.size() << endl;
    clearUpdates();
    if (current_) {
        delete current_;
    }
    if (fileMap_) {
        fileMap_->clear();
        delete fileMap_;
    }
    if (valueFileMap_) {
        valueFileMap_->clear();
        delete valueFileMap_;
    }
    if (zoneUseMap_) {
        free(zoneUseMap_);
        zoneUseMap_ = NULL;
    }
    manifestInfo_ = NULL;
    for (int i = 0; i < 10; ++i) {
        delete levels_[i];
    }
}

void Superblock::clearUpdates() {
    for (vector<ReplayObj*>::iterator it = replayObjs_.begin(); it != replayObjs_.end(); ++it) {
        (*it)->unref();
    }
    replayObjs_.clear();
}

Status Superblock::create() {
    return persistSnapshot();
}
Status Superblock::load() {
    char *buf = disk_->buff_;
    Status s;
    int decodeResult = 0;
    uint64_t destAddr_ = address();
    int r = lseek(disk_->fd_, destAddr_, SEEK_SET);
    if (r == -1) {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to seek to " << destAddr_ << endl;
        s = Status::IOError("Failed to seek");
        return s;
    }
    size_t read_in = Disk::SUPER_ZONE_SIZE;
    r = read(disk_->fd_, buf, read_in);

    if (r != -1) {  // Successful read
        char* ptr = buf;
        destAddr_ += r;
        uint64_t magicNumber = DecodeFixed64(ptr);  // Decode begin of superblock file info
        if (magicNumber != Disk::MAGIC_NUMBER) {
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Invalid superblock# " << number_ << endl;
            return Status::InvalidArgument("Invalid superblock");
        }
        ptr = buf + ELEMENT_ALIGNMENT;  // Move pointer to begin of the disk info

        Slice input(ptr, r - ELEMENT_ALIGNMENT);
        int m = 0;
        do {
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Loop " << ++m << endl;
            decodeResult = decode(input);
            if (decodeResult == 1) {
                r = lseek(disk_->fd_, destAddr_, SEEK_SET);
                if (r == -1) {
                    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to seek to " << destAddr_ << endl;
                    s = Status::IOError("Failed to seek");
                    decodeResult = -1;
                    break;
                }
                size_t read_in = min(uint64_t(Disk::SUPER_ZONE_SIZE), uint64_t(Disk::ZONE_SIZE - (destAddr_ - address())));
                r = read(disk_->fd_, buf, read_in);
                if (r == -1) {
                    s = Status::IOError("Failed to read superblock");
                    decodeResult = -1;
                    break;
                }
                input = Slice(buf, r);
                destAddr_ += r;
            } else if (decodeResult == -1) {
                s = Status::InvalidArgument("Failed to parse superblock");
            }
        } while (decodeResult == 1);
    } else {
        decodeResult = -1;
        s = Status::IOError("Failed to read superblock");
    }
    if (s.ok()) {
        bGood_ = true;
    } else {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": >>> BAD "<< endl;
    }
    return s;
}
int Superblock::decode(Slice &src) {
    Slice input = src;
    uint32_t tag;
    char* elementStartPtr = (char*)src.data();
    char* nextElementStartPtr;
    int bytesRemain = src.size();
    int decodeResult = 1;
    do {
        uint64_t magicNumber = DecodeFixed64(input.data());
        if (magicNumber == Disk::MAGIC_NUMBER) {
           decodeResult = 0;
           cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": End of superblock " << endl;
           break;
        }

        if (GetVarint32(&input, &tag)) {
        switch (tag) {
            case ReplayObj::Tag::kCurLogZone:
                if (!GetVarint32(&input, &disk_->currentLogZone_)) {
                    decodeResult = -1;
                }
                break;
            case ReplayObj::Tag::kZoneStartAddr:
            {
                uint64_t addr = -1;
                if (!GetVarint64(&input, &addr) || addr != disk_->zoneStartAddr_) {
                    decodeResult = -1;
                }
                break;
            }
            case ReplayObj::Tag::kNumZones:
            {
                uint32_t n = 0;
                if (!GetVarint32(&input, &n) || n != disk_->numZones_) {
                    decodeResult = -1;
                }
                break;
            }
            case ReplayObj::Tag::kZoneSize:
                if (!GetVarint64(&input, &disk_->zoneSize_)) {
                    if (disk_->zoneSize_ != Disk::ZONE_SIZE) {
                        decodeResult = -1;
                    }
                }
                break;
            case ReplayObj::Tag::kCurrent:
            {
                Slice tmp;
                if (GetLengthPrefixedSlice(&input, &tmp)) {
                    delete disk_->current_;
                    disk_->current_ = new std::string(tmp.ToString());
                    uint64_t number;
                    FileType type;
                    if (!disk_->current_->empty()) {
                        string manifestName = disk_->current_->substr(0, disk_->current_->size() - 1);
                        if (ParseFileName(manifestName, &number, &type)){
                            disk_->manifestInfo_ = disk_->fileMap_->getFileInfo(number, type);
                        } else {
                            decodeResult = -1;
                            Status::InvalidArgument("Failed to parse manifest name from CURRENT");
                        }
                    }
                } else {
                    decodeResult = -1;
                }
                break;
            }
            case ReplayObj::Tag::kZoneInventory:
            {
                Slice tmp;
                if (GetLengthPrefixedSlice(&input, &tmp)) {
                    disk_->zoneUseMapSize_ = tmp.size();
                    if (disk_->zoneUseMap_) {
                        free(disk_->zoneUseMap_);
                        disk_->zoneUseMap_ = NULL;
                    }
                    disk_->zoneUseMap_ = (unsigned char *) calloc(disk_->zoneUseMapSize_, sizeof(char));
                    memcpy(disk_->zoneUseMap_, tmp.ToString().c_str(), disk_->zoneUseMapSize_);
                } else {
                    decodeResult = -1;
                }
                break;
            }
            case ReplayObj::Tag::kFileMap:
                if (!disk_->fileMap_->deserialize(input, disk_)) {
                    decodeResult = -1;
                }
                break;
            case ReplayObj::Tag::kNewFile:
            {
                ReplayFileInfo replayFileInfo(ReplayObj::Tag::kNewFile);
                if (!replayFileInfo.deserialize(input, disk_)) {
                    decodeResult = -1;
                } else {
                    FileInfo* finfo = replayFileInfo.fileInfo();
                    if (finfo->getType() == kValueFile) {
                        disk_->valueFileMap_->addFileInfo(finfo->getNumber(), finfo);
                    } else {
                        disk_->fileMap_->addFileInfo(finfo->getNumber(), finfo);
                    }
                }
                break;
            }
            case ReplayObj::Tag::kNewSegment:
            {
                ReplaySegment replaySegment(ReplayObj::Tag::kNewSegment);
                if (!replaySegment.deserialize(input, disk_)) {
                    decodeResult = -1;
                    break;
                }
                Segment* newSegment = replaySegment.segment();
                FileInfo* finfo = disk_->getFileInfo(replaySegment.getFileNumber(), replaySegment.getFileType());
                if (!finfo) {
                    decodeResult = -1;
                    break;
                }
                Level* level = finfo->getLevel();
                if (!level) {
                    decodeResult = -1;
                    break;
                }
                Zone* zone = level->addZone(newSegment->getZoneNumber());
                if (replaySegment.getIdx() == -1) {
                    Segment* existSegment = finfo->getLastSegment();
                    if (existSegment && existSegment->getAddr() == newSegment->getAddr()) {

                        Segment* duplicateSegment = finfo->replaceLastSegment(newSegment);
                        duplicateSegment->getZone()->deallocateSegment(duplicateSegment);
                        zone->addSegment(newSegment);
                        delete duplicateSegment;
                    } else {
                        zone->addSegment(newSegment);
                        finfo->addSegment(newSegment);
                    }
                } else {
                    zone->addSegment(newSegment);
                    finfo->insert(newSegment, replaySegment.getIdx());
                }
                replaySegment.segment(NULL);
                break;
            }
            case ReplayObj::Tag::kUpdateSegment:
            {
                ReplaySegment replaySegment(ReplayObj::Tag::kUpdateSegment);
                if (!replaySegment.deserialize(input, disk_)) {
                    decodeResult = -1;
                    break;
                }
                Segment* updateSegment = replaySegment.segment();
                FileInfo* finfo = disk_->getFileInfo(replaySegment.getFileNumber(), replaySegment.getFileType());
                if (!finfo) {
                    decodeResult = -1;
                    break;
                }
                if (replaySegment.getIdx() == -1) {
                    Status::InvalidArgument("Invalid segment idx");
                    decodeResult = -1;
                    break;
                }
                Segment* existSegment = finfo->getSegment(replaySegment.getIdx());
                if (existSegment == NULL) {
                    decodeResult = -1;
                    break;
                }
                off_t newDataSize = updateSegment->size() - existSegment->size();
                finfo->addSize(newDataSize);
                //existSegment->size(updateSegment->size());
                break;
            }
            case ReplayObj::Tag::kDeallocatedSegment:  // created by defragment only
            {
                ReplaySegment replaySegment(ReplayObj::Tag::kDeallocatedSegment);
                if (!replaySegment.deserialize(input, disk_)) {
                    decodeResult = -1;
                    break;
                }
                Segment* newSegment = replaySegment.segment();
                if (!newSegment) {
                    decodeResult = -1;
                    break;
                }
                FileInfo* finfo = disk_->getFileInfo(replaySegment.getFileNumber(), replaySegment.getFileType());
                if (!finfo) {
                    decodeResult = -1;
                    break;
                }
                Level* level = finfo->getLevel();
                if (!level) {
                    decodeResult = -1;
                    break;
                }
                Segment* removeSeg = finfo->removeSegment(newSegment);
                if (!removeSeg) {
                    decodeResult = -1;
                    break;
                }
                if (level) {
                   level->deallocateSegment(newSegment);
                } else {
                   Zone* zone = level->addZone(newSegment->getZoneNumber());
                   zone->deallocateSegment(newSegment);
                }
                delete removeSeg;
                break;
            }
            case ReplayObj::Tag::kDeletedFile:
            {
                ReplayFileInfo replayFileInfo(ReplayObj::Tag::kDeletedFile);
                if (!replayFileInfo.deserialize(input, disk_)) {
                    decodeResult = -1;
                    break;
                }
                string fname(disk_->name_);
                fname += replayFileInfo.fileInfo()->getFileName();
                uint64_t fnumber = replayFileInfo.fileInfo()->getNumber();
                FileType ftype = replayFileInfo.fileInfo()->getType();
                FileInfo* finfo = NULL;
                if (ftype == kValueFile) {
                    finfo = disk_->valueFileMap_->getFileInfo(fnumber, ftype);
                } else {
                    finfo = disk_->fileMap_->getFileInfo(fnumber, ftype);
                }
                if (finfo == NULL) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
                    Status::NotFound(context.str());
                    return -1;
                }
                FileType type = finfo->getType();
                uint64_t number = finfo->getNumber();
                if (finfo->getType() == kValueFile) {
                    disk_->valueFileMap_->removeFileInfo(number, type);
                } else {
                    disk_->fileMap_->removeFileInfo(number, type);
                }
                uint32_t level = finfo->getLevel()->getNumber();
                disk_->levels_[level]->deallocateFile(finfo);
                delete finfo;
                break;
            }
            case ReplayObj::Tag::kValueFileMap:
                if (!disk_->valueFileMap_->deserialize(input, disk_)) {
                    decodeResult = -1;
                }
                break;
            case ReplayObj::Tag::kSeqNum:
            {
                decodeResult = -1;
                uint64_t nSeqNum = 0;
                if (GetVarint64(&input, &nSeqNum)) {
                    if (nSeqNum > 0) {
                        if (seqNum_ == 0) {
                            seqNum_ = nSeqNum;
                            decodeResult = 1;
                        } else if (nSeqNum == seqNum_ + 1) {
                            seqNum_ = nSeqNum;
                            decodeResult = 1;
                        }
                    }
                }
                break;
            }
            case ReplayObj::kValueFileInfo:
            {
                ReplayValueFileInfo replayValFileInfo;
                if (!replayValFileInfo.deserialize(input, disk_)) {
                    decodeResult = -1;
                    break;
                }
                FileInfo* finfo = disk_->getFileInfo(replayValFileInfo.getFileNumber(), FileType::kValueFile);
                if (!finfo) {
                    decodeResult = -1;
                    break;
                }
                finfo->values(replayValFileInfo.fileValueInfo());
                if (finfo->values().getTotal() < finfo->values().getDeleted()) {
                }

                break;
            }
            default:
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": unknown tag: " << tag << endl;
                decodeResult = -1;
                break;
        }
        } else {
            decodeResult = -1;
        }
        if (decodeResult == 1) {
            int bytesConsumed = ROUNDUP(input.data() - elementStartPtr, ELEMENT_ALIGNMENT);
            nextElementStartPtr = elementStartPtr + bytesConsumed;
            bytesRemain -= bytesConsumed;
            if (bytesRemain > 0) {
                input = Slice(nextElementStartPtr, bytesRemain);
                elementStartPtr = nextElementStartPtr;
            }
        }
    } while (decodeResult == 1 && bytesRemain > 0);
    return decodeResult;
}

void Superblock::fileCreated(FileInfo* finfo) {
        MutexLock l(&mu_);
        ReplayFileInfo* replayFileInfo = new ReplayFileInfo();
        replayFileInfo->init(ReplayObj::Tag::kNewFile, finfo->getNumber(),
                                                   finfo->getType(), finfo->getLevel()->getNumber());
        replayObjs_.push_back(replayFileInfo);
        if (finfo->getType() == FileType::kValueFile) {
            // Add the first segment
            ReplaySegment* replaySegment = new ReplaySegment(ReplayObj::Tag::kNewSegment, finfo->getLastSegment(), 0);
            replayObjs_.push_back(replaySegment);
        }
}
Status Superblock::markStatus(bool status) {
    Status s;
    bGood_ = status;
    char *buf = disk_->buff_;
    memset(buf, 0, ELEMENT_ALIGNMENT);
    EncodeFixed64(buf, Disk::MAGIC_NUMBER);
    EncodeFixed32(buf + sizeof(uint64_t), status);
    int result = lseek(disk_->fd_, address(), SEEK_SET);
    if (result == -1) {
        s = Status::IOError("Failed to seek", strerror(errno));
    } else {
        int written = write(disk_->fd_, buf, ELEMENT_ALIGNMENT);
        if (written < 0) {
            s = Status::IOError("Failed to mark superblock", strerror(errno));
        } else {
            int nResult = sync_file_range(disk_->fd_, address(), ELEMENT_ALIGNMENT, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
            if (nResult == -1) {
                stringstream ss;
                ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to sync file range for marking superblock status: " << strerror(errno);
                s = Status::IOError(ss.str());
            } else {
                nResult = fdatasync(disk_->fd_);
                if (nResult == -1) {
                    stringstream ss;
                    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to fdatasync for marking superblock status: " << strerror(errno);
                    s = Status::IOError(ss.str());
                } else {
                    destAddr_ = address() + ELEMENT_ALIGNMENT;
                }
            }
        }
    }
    return s;
}

Status Superblock::persistProgress() {
    MutexLock l(&mu_);
    Status s;
    if (!shouldBeGood()) {
        good(false);
        ++nBads_;
        clearUpdates();
        s = Status::IOError("Forcing bad superblock");
        return s;
    }
    uint64_t limitSize = Disk::ZONE_SIZE/4;
    uint64_t memoryNeeded = (replayObjs_.size() + 10)*ELEMENT_ALIGNMENT + ROUNDUP(disk_->zoneUseMapSize_, ELEMENT_ALIGNMENT);
    if (memoryNeeded > Disk::SUPER_ZONE_SIZE || destAddr_  + memoryNeeded > address() + limitSize) {
        s = persistSnapshot();
        return s;
    }
    char* buff = disk_->buff_;
    char *ptr = buff;
    for (vector<ReplayObj*>::iterator it = this->replayObjs_.begin(); it != replayObjs_.end(); ++it) {
        ptr = (*it)->serialize(ptr);
        ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    }
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kCurLogZone);
    ptr = PutVarint32InBuff(ptr, disk_->currentLogZone_);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kZoneInventory);
    Slice bandmap(reinterpret_cast<char*>(disk_->zoneUseMap_), disk_->zoneUseMapSize_);
    ptr = PutLengthPrefixedSliceInBuff(ptr, bandmap);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kCurrent);
    ptr = PutLengthPrefixedSliceInBuff(ptr, *(disk_->current_));
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kSeqNum);
    ptr = PutVarint64InBuff(ptr, this->seqNum_ + 1);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    EncodeFixed64(ptr, Disk::MAGIC_NUMBER);  // Mark end of superblock info
    ptr += sizeof(uint64_t);
    int nResult = lseek(disk_->fd_, this->destAddr_, SEEK_SET);
    if (nResult == -1) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to seek to addr " << destAddr_ << ": " << strerror(errno);
        s = Status::IOError(ss.str());
    } else {
        int nWrittenBytes = write(disk_->fd_, buff, ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT));
        if (nWrittenBytes == -1) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to write superblock fileinfo : " << strerror(errno);
            s = Status::IOError(ss.str());
        } else {
            nResult = sync_file_range(disk_->fd_, destAddr_, ptr - buff, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
            if (nResult == -1) {
                stringstream ss;
                ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to sync file range: " << strerror(errno);
                s = Status::IOError(ss.str());
            } else {
                nResult = fdatasync(disk_->fd_);
                if (nResult == -1) {
                    stringstream ss;
                    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to fdatasync: " << strerror(errno);
                    s = Status::IOError(ss.str());
                } else {
                    destAddr_ += ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT) - ELEMENT_ALIGNMENT;
                    ++seqNum_;
                }
            }
        }
    }
    bGood_ = s.ok();
    if (!bGood_) {
        this->nBads_ = 0;
    }
    clearUpdates();
    return s;
}

Status Superblock::persistSnapshot() {
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": ===== Creating superblock snapshot #" << this->number() << endl;
    Status s;
    if (!shouldBeGood()) {
        good(false);
        ++nBads_;
        clearUpdates();
        s = Status::IOError("Forcing bad superblock");
        return s;
    }

    char* buff = disk_->buff_;
    memset(buff, 0, Disk::SUPER_ZONE_SIZE);
    char* ptr = buff;
    EncodeFixed64(ptr, Disk::MAGIC_NUMBER);  // Mark begin of superblock info
    ptr += sizeof(uint64_t);
    EncodeFixed32(ptr, true);  // Mark good
    ptr = buff + ELEMENT_ALIGNMENT;
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kZoneSize);
    ptr = PutVarint64InBuff(ptr, disk_->zoneSize_);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kNumZones);
    ptr = PutVarint32InBuff(ptr, disk_->numZones_);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kCurLogZone);
    ptr = PutVarint32InBuff(ptr, disk_->currentLogZone_);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kZoneStartAddr);
    ptr = PutVarint64InBuff(ptr, disk_->zoneStartAddr_);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kZoneInventory);
    Slice bandmap(reinterpret_cast<char*>(disk_->zoneUseMap_), disk_->zoneUseMapSize_);
    ptr = PutLengthPrefixedSliceInBuff(ptr, bandmap);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kFileMap);
    ptr = disk_->fileMap_->serialize(ptr);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kCurrent);
    ptr = PutLengthPrefixedSliceInBuff(ptr, *disk_->current_);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kSeqNum);
    ptr = PutVarint64InBuff(ptr, seqNum_ + 1);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    ptr = PutVarint32InBuff(ptr, ReplayObj::Tag::kValueFileMap);
    ptr = disk_->valueFileMap_->serialize(ptr);
    ptr = buff + ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT);
    EncodeFixed64(ptr, Disk::MAGIC_NUMBER);  // Mark end of superblock info
    ptr += sizeof(uint64_t);
    destAddr_ = address();
    int nResult = lseek(disk_->fd_, destAddr_, SEEK_SET);
    if (nResult == -1) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to seek to addr " << destAddr_ << ": " << strerror(errno);
        s = Status::IOError(ss.str());
    } else {
        int nWrittenBytes = write(disk_->fd_, buff, ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT));
        if (nWrittenBytes == -1) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to write superblock fileinfo : " << strerror(errno);
            s = Status::IOError(ss.str());
        } else {
            nResult = sync_file_range(disk_->fd_, destAddr_, ptr - buff, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
            if (nResult == -1) {
                stringstream ss;
                ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to sync file range: " << strerror(errno);
                s = Status::IOError(ss.str());
            } else {
                nResult = fdatasync(disk_->fd_);
                if (nResult == -1) {
                    stringstream ss;
                    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to fdatasync: " << strerror(errno);
                    s = Status::IOError(ss.str());
                } else {
                    destAddr_ += ROUNDUP(ptr - buff, ELEMENT_ALIGNMENT) - ELEMENT_ALIGNMENT;
                    ++seqNum_;
                }
            }
        }
    }
    bGood_ = s.ok();
    clearUpdates();
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": ===== Complete creating superblock snapshot: " << s.ToString() << endl;
    return s;
}

uint64_t Superblock::address() {
    return disk_->superBlockAddress(number_);
}

void Superblock::transferDiskInfoFromDisk(Disk* disk) {
    numZones_ = disk->numZones_;
    zoneStartAddr_ = disk->zoneStartAddr_;
    delete current_;
    current_ = disk->current_;
    disk->current_ = NULL;
    delete fileMap_;
    fileMap_ = disk->fileMap_;
    disk->fileMap_ = NULL;
    delete valueFileMap_;
    valueFileMap_ = disk->valueFileMap_;
    disk->valueFileMap_ = NULL;
    if (zoneUseMap_) {
        free(zoneUseMap_);
    }
    zoneUseMap_ = disk->zoneUseMap_;
    disk->zoneUseMap_ = NULL;
    zoneUseMapSize_ = disk->zoneUseMapSize_;
    manifestInfo_ = disk->manifestInfo_;
    disk->manifestInfo_ = NULL;
    for (int i = 0; i < 10; ++i) {
        delete levels_[i];
        levels_[i] = disk->levels_[i];
        disk->levels_[i] = NULL;
    }
    lastAllocatedZoneMapIdx_ = disk->lastAllocatedZoneMapIdx_;
    lastAllocatedZone_ = disk->lastAllocatedZone_;
    lastAllocatedConvZoneMapIdx_ = disk->lastAllocatedConvZoneMapIdx_;
    lastAllocatedConvZone_ = disk->lastAllocatedConvZone_;
    currentLogZone_ = disk->currentLogZone_;
}

void Superblock::transferDiskInfoToDisk(Disk* disk) {
    disk->numZones_ = numZones_;
    disk->zoneStartAddr_ = zoneStartAddr_;
    delete disk->current_;
    disk->current_ = current_;
    current_ = NULL;
    delete disk_->fileMap_;
    disk->fileMap_ = fileMap_;
    fileMap_ = NULL;
    delete disk->valueFileMap_;
    disk->valueFileMap_ = valueFileMap_;
    valueFileMap_ = NULL;
    if(disk->zoneUseMap_) {
        free(disk->zoneUseMap_);
    }
    disk->zoneUseMap_ = zoneUseMap_;
    zoneUseMap_ = NULL;
    disk->zoneUseMapSize_ = zoneUseMapSize_;
    disk->manifestInfo_ = manifestInfo_;
    manifestInfo_ = NULL;
    for (int i = 0; i < 10; ++i) {
        delete disk->levels_[i];
        disk->levels_[i] = levels_[i];
        levels_[i] = NULL;
    }
    disk->lastAllocatedZoneMapIdx_ = lastAllocatedZoneMapIdx_;
    disk->lastAllocatedZone_ = lastAllocatedZone_;
    disk->lastAllocatedConvZoneMapIdx_ = lastAllocatedConvZoneMapIdx_;
    disk->lastAllocatedConvZone_ = lastAllocatedConvZone_;
    disk->currentLogZone_ = currentLogZone_;
}
void Superblock::clear() {
    numZones_ = 0;
    zoneStartAddr_ = 0;
    delete current_;
    current_ = NULL;
    delete fileMap_;
    fileMap_ = NULL;
    delete valueFileMap_;
    valueFileMap_ = NULL;
    if (zoneUseMap_) {
        free(zoneUseMap_);
    }
    zoneUseMap_ = NULL;
    zoneUseMapSize_ = 0;
    manifestInfo_ = NULL;
    for (int i = 0; i < 10; ++i) {
        delete levels_[i];
        levels_[i] = NULL;
    }
    lastAllocatedZoneMapIdx_ = -1;
    lastAllocatedZone_ = -1;
    lastAllocatedConvZoneMapIdx_ = -1;
    lastAllocatedConvZone_ = -1;
    currentLogZone_ = 0;
}
bool Superblock::shouldBeGood() {
    return Disk::superblockStatus[number()];
}
} /* namespace smr */
