/*
 * Disk.cc
 *
 *  Created on: Apr 19, 2015
 *      Author: tri
 */


#include "Disk.h"

#include <iostream>
#include <tuple>
#include <deque>
#include <map>
#include <sys/syscall.h>
#include <sstream>
#include <set>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <numeric>
#include <limits.h>
#if defined(LEVELDB_PLATFORM_ANDROID)
#include <sys/stat.h>
#endif

#include "leveldb/slice.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/posix_logger.h"
#include "db/filename.h"
#include "util/coding.h"
#include "util/env_posix.h"
#include "SmrWritableFile.h"
#include "SmrSequentialFile.h"
#include "ValueMover.h"
//#include "ValueDeleter.h"
#include "ValueFileCache.h"

#include <linux/fs.h>

using namespace std;

using namespace leveldb;

//namespace leveldb {
namespace smr {

pthread_t bgasync_thread_;
pthread_mutex_t a_mu_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t a_signal_ = PTHREAD_COND_INITIALIZER;
std::deque<void*> a_queue_;
bool started_async_thread_ = false;

void* BGAsyncSync(void *arg);
void BGAsyncQueue(void* arg) {
    pthread_mutex_lock(&a_mu_);
    if(!started_async_thread_){
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        size_t STK_SIZE = 2 * 1024 * 1024;
        pthread_attr_setstacksize(&attr, STK_SIZE);
        started_async_thread_ = true;
        pthread_create(&bgasync_thread_, &attr, &BGAsyncSync, NULL);
        pthread_attr_destroy(&attr);
    }
    if ( a_queue_.empty()){
        pthread_cond_signal(&a_signal_);
    }
    a_queue_.push_back(arg);
    //  printf("Queue ");
    pthread_mutex_unlock(&a_mu_);
}

void* BGAsyncSync(void *arg){
    pid_t tid;
    tid = syscall(SYS_gettid);
    int ret = setpriority(PRIO_PROCESS, tid, 5);
    if (ret < 0)
        fprintf(stderr,"Prio %d\n", ret);
    while(true) {
        pthread_mutex_lock(&a_mu_);
        while (a_queue_.empty()) {
            pthread_cond_wait(&a_signal_, &a_mu_);
        }
        AsyncState* state = reinterpret_cast<AsyncState*>(a_queue_.front());
        a_queue_.pop_front();
        pthread_mutex_unlock(&a_mu_);
        if (state->options == 1)
            sync_file_range(state->fd, state->addr, state->size, SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE);
        else if(state->options == 2)
            sync_file_range(state->fd, state->addr, state->size, SYNC_FILE_RANGE_WRITE);
        else if(state->options == 3)
            sync_file_range(state->fd, state->addr, state->size, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
        if (state->closeFD && ::close(state->fd) != 0) {
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to close fd " << state->fd << ", " << strerror(errno) << endl;
        }
        delete state;
    }
}

pthread_t bg_comp_async_thread_;
pthread_mutex_t a_c_mu_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t a_c_signal_ = PTHREAD_COND_INITIALIZER;
std::deque<void*> a_c_queue_;
bool started_async_compact_thread_ = false;

void* BGCompAsyncSync(void *arg);
void BGCompAsyncQueue(void* arg) {
    pthread_mutex_lock(&a_c_mu_);
    if(!started_async_compact_thread_){
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        size_t STK_SIZE = 2 * 1024 * 1024;
        pthread_attr_setstacksize(&attr, STK_SIZE);
        started_async_compact_thread_ = true;
        pthread_create(&bg_comp_async_thread_, &attr, &BGCompAsyncSync, NULL);
        pthread_attr_destroy(&attr);
    }
    if ( a_c_queue_.empty()){
        pthread_cond_signal(&a_c_signal_);
    }
    a_c_queue_.push_back(arg);
    pthread_mutex_unlock(&a_c_mu_);
}

void* BGCompAsyncSync(void *arg){
    while(true) {
        pthread_mutex_lock(&a_c_mu_);
        while (a_c_queue_.empty()) {
            pthread_cond_wait(&a_c_signal_, &a_c_mu_);
        }
        AsyncState* state = reinterpret_cast<AsyncState*>(a_c_queue_.front());
        a_c_queue_.pop_front();

        pthread_mutex_unlock(&a_c_mu_);
        if (state->options == 1)
            sync_file_range(state->fd, state->addr, state->size, SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE);
        else if(state->options == 2)
            sync_file_range(state->fd, state->addr, state->size, SYNC_FILE_RANGE_WRITE);
        else if(state->options == 3)
            sync_file_range(state->fd, state->addr, state->size, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
        delete state;
    }
}

const string Disk::LEVEL_INFO_DIR = "/mnt/util/LevelDBInfo";
uint64_t Disk::SUPERBLOCK_0_ADDR = -1;
uint64_t Disk::SUPERBLOCK_1_ADDR = -1;
uint64_t Disk::SUPERBLOCK_2_ADDR = -1;
bool Disk::superblockStatus[NUMBER_SUPERBLOCKS] = {true, true, true};

ostream& operator<<(ostream& out, Disk& disk) {
    out << "\t=== DISK INFORMATION:" << endl;
    out << "\tDatabase name = " << disk.name_
	    << "\tSize = " << disk.zoneSize_ << endl
            << "\tBand start address = " << disk.zoneStartAddr_ << endl
            << "\tNumber of zones = " << disk.numZones_
            << ", usable capacity = " << disk.usableCapacity_
            << ", #Usable zones = " << disk.usableCapacity_/Disk::ZONE_SIZE << endl
            << "\tPage size = " << disk.pageSize_ << endl
            << "\tZone uses size = " << disk.zoneUseMapSize_ << endl
            << "\tFree zones = " << disk.nFreeZones_ << endl
            << "\tUsed zones = " << disk.nUsedZones_ << endl
            << *(disk.fileMap_) << endl << "VALUE FILE MAP: " << endl
            << *(disk.valueFileMap_);
    if (disk.current_) {
        cout << endl << "\tCURRENT = " << *(disk.current_);
        if (*(disk.current_) == "") {
        	cout << endl;
        }
    } else {
    	cout << endl;
    }

    if (false) { //disk.zoneUseMap_) { // Save for extra disk info
       cout << "Zone use bit map: " << endl;
       unsigned int* zoneBitMap = (unsigned int*)disk.zoneUseMap_;
       unsigned char* charPtr = disk.zoneUseMap_;
       int arrSize = (disk.zoneUseMapSize_/sizeof(unsigned int));
       cout << " ARRAY SIZE: " << arrSize << endl;
       int j = 0;
       for (int i = 0; i < sizeof(unsigned int)*arrSize; ++i) {
          printf(" i  %d  %02x ", i, *charPtr);
          ++charPtr;
          ++j;
          if (j % 20 == 0) {
             printf("\n");
             j = 0;
          }
       }
    cout << dec << endl;
    }
    if (false) {  // Save for extra disk info
       for (int i = 0; i < 10; ++i) {
          int nDesignate;
          int nNonDesignate;
          int nMismatch;
          disk.levels_[i]->getZoneCounts(nDesignate, nNonDesignate, nMismatch);
          cout << "Level " << i << ", nDesignate = " << nDesignate << ", nNoneDesignate = " << nNonDesignate << ", nMismatch = " << nMismatch << endl;
       }
    }
    out << "\t#Good superblocks = " << disk.numGoodSuperblocks_ << endl;
    vector<Superblock*>::iterator it;
    for (it = disk.superblocks_.begin(); it != disk.superblocks_.end(); ++it) {
    	out << "\t\tSuperblock #" << (*it)->number() << " at zone #" << (*it)->address()/Disk::ZONE_SIZE << ": ";
    	if ((*it)->isGood()) {
    	    cout << "good";
    	} else {
    	    cout << "bad";
    	}
    	cout << endl;
    }

    cout << "=== LEVELS:" << endl;
    for (int i = 0; i < 10; ++i) {
        cout << *(disk.levels_[i]) << endl;
    }

    cout << endl;


    return out;
}

Disk::DiskStatus Disk::_status = Disk::DiskStatus::NORMAL;
port::Mutex Disk::_mu;

void Disk::initializeSuperBlockAddr(string& diskName) {
    const int fd = ::open(diskName.c_str(), O_RDWR);
    if (fd < 0) {
    	 throw DiskError();
    }
	uint64_t capacity = 0;
    if(ioctl(fd, BLKGETSIZE64, &capacity)<0) {
        close(fd);
        throw DiskError();
    }
    else if (capacity < 5*ZONE_SIZE) {
        close(fd);
        throw DiskError();
    }
    close(fd);
    int numZones = (capacity / ZONE_SIZE) - 1;
    int superZone = numZones/2;
    superZone = START_SUPER_BLOCK_ZONE; //Start from OD
    Disk::SUPERBLOCK_0_ADDR = superZone*ZONE_SIZE;
    Disk::SUPERBLOCK_1_ADDR = (superZone + 1)*ZONE_SIZE;
    Disk::SUPERBLOCK_2_ADDR = (superZone + 2)*ZONE_SIZE;
}

Status Disk::moveSegment(Segment* srcSeg, Level* level, FileType type) {
    Status status;
    vector<Segment*> dstSegs;
    Segment* dstSeg = NULL;
    uint32_t srcOffset = 0;
    uint32_t srcSize = srcSeg->getSize();
    int sFd = open(O_RDONLY);
    int dFd = open(O_RDWR | O_DIRECT);
    const int BUFFER_SIZE = 1024*1024;
    char* buffer = NULL;
    int s = posix_memalign((void**)&buffer, 4096, BUFFER_SIZE);
    if (buffer == NULL || s != 0) {
        status = Status::IOError("Failed to allocate memory for move segment");
        return status;
    }
    bool success = true;
    FileInfo* finfo = srcSeg->getFileInfo();
    int srcSegIdx = finfo->getIdx(srcSeg);
    int idx = srcSegIdx;
    while (srcOffset < srcSize && success) {
        int nRead = srcSeg->read(sFd, srcOffset, buffer, BUFFER_SIZE);
        if (nRead > 0) {
            uint32_t nWrite = nRead;
            uint32_t nWrote = 0;
            while (nWrite > 0 && success) {
                if (dstSeg == NULL) {
                    dstSeg = level->allocateSegment(type);
                    // Check dstSeg, if NULL could not allocateSegment not enough space
                    if (dstSeg == NULL) {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to allocateSegment";
                        status = Status::NoSpaceAvailable(context.str());
                        success = false;
                        break;
                    }
                    assert(dstSeg);
                    dstSeg->setFileInfo(srcSeg->getFileInfo());
                    dstSegs.push_back(dstSeg);
                }
                int n = dstSeg->write(dFd, buffer + nWrote, nWrite);
                if (n == 0) {
                    assert(dstSeg->getSize() > 0);
                    dstSeg->setFileInfo(srcSeg->getFileInfo());
                    dstSeg->complete(++idx);
                    bgSync(dFd, dstSeg->getAddr(), dstSeg->getSize(), false);
                    dstSeg = NULL;
                } else if (n < 0) {
                    status = Status::IOError("Move segment", "Failed to write");
                    success = false;
                } else {
                    nWrote += n;
                    nWrite -= n;
                }
            }
            srcOffset += nRead;
        } else if (nRead < 0) {
            status = Status::IOError("Move segment", "Failed to read");
            success = false;
        }
    }
    close(sFd);
    if (dstSeg) {
        dstSeg->setFileInfo(srcSeg->getFileInfo());
        dstSeg->complete(++idx);
        bgSync(dFd, dstSeg->getAddr(), dstSeg->getSize(), true);
    }
    if (success) {
        FileInfo* finfo = srcSeg->getFileInfo();
        status = finfo->replaceSegment(srcSeg, dstSegs);
        if (status.ok()) {
            srcSeg->getZone()->deallocateSegment(srcSeg);
            ReplaySegment* replaySegment = new ReplaySegment(ReplayObj::Tag::kDeallocatedSegment, srcSeg, srcSegIdx);
            segmentDeallocated(replaySegment);
            replaySegment->unref();
            delete srcSeg;
        } else {
            success = false;
        }
    }
    if (!success) {
        vector<Segment*>::iterator it;
        idx = srcSegIdx;
        for (it = dstSegs.begin(); it != dstSegs.end(); ++it) {
            Segment* seg = *it;
            seg->getFileInfo()->removeSegment(seg);
            Level* level = seg->getZone()->getLevel();
            level->deallocateSegment(*it);
            ReplaySegment* replaySegment = new ReplaySegment(ReplayObj::Tag::kDeallocatedSegment, *it, ++idx);
            this->segmentDeallocated(replaySegment);
            replaySegment->unref();
            delete seg;
        }
    }
    free(buffer);
    return status;
}
Status Disk::defragment(int level) {
    cout << dec;
    if (this->numGoodSuperblocks_ == 0) {
        return Status::Corruption("There is no good superblock");
    } else if (numGoodSuperblocks_ == 1) {
        return Status::Corruption("Superblock is not writable");
    }
    vector<Zone*> fragmentedZones;
     levels_[level]->getFragmentedZones(fragmentedZones);
     if (fragmentedZones.size() < 2) {
         return Status::NotAttempted("Disk::defragment", "There is only 1 fragmented zone");
     }
     Status status;
     int nDefragmented = 0;

     for (vector<Zone*>::iterator it = fragmentedZones.begin();
             status.ok() && ++nDefragmented <= 2 && it != fragmentedZones.end(); ++it) {
         Zone* srcZone = *it;
         uint64_t number = 0;
         FileInfo* finfo = NULL;
         map<uint64_t, Segment*>& srcSegMap = srcZone->getSegmentMap();
         map<uint64_t, Segment*>::iterator srcSegMapIt = srcSegMap.begin();
         int zoneNum = srcZone->getNumber();
         int fd = open(O_RDWR);
         while (srcSegMapIt != srcSegMap.end() && status.ok()) {
             status = moveSegment(srcSegMapIt->second, levels_[level], kTableFile);
             srcSegMapIt = srcSegMap.begin();
         }
         if (status.ok()) {
             levels_[level]->deallocateZone(srcZone);
             delete srcZone;
             status = this->sync(fd);
         }
         ::close(fd);
     }
     return status;
}
Status Disk::defragmentExternal(const Options& options, Env::put_func_t putFunc)
{
    FileInfo* info = valueFileMap_->pickFileInfoForDefrag(name_);
    if(!info) {
        //Log(options.info_log, 5, "No suitable value file for defragmentation.");
        return Status::NotAttempted("No suitable value file for defragmentation.");
    }
    uint64_t fnumber = info->getNumber();
    ValueMover mover(name_, options, info, putFunc);
    Status status = mover.Finish();

    // two options: value file may be either de-allocated directly or we can just
    // block the file for defragmentation and wait for compaction to trigger value
    // deleter. if de-allocation is done directly, races with concurrent gets have
    // to be considered.

    if (status.ok()) {
        valueFileMap_->obsoleteFile(fnumber); //info->getNumber());
    }
    valueFileMap_->unblock(fnumber);
/*
    //Save for debug.  Will be removed when code is stable
    string valFileMapInfo;
    valueFileMap_->getInfo(valFileMapInfo);
    if (!valFileMapInfo.empty()) {
        Log(options.info_log, 0, " %s", valFileMapInfo.c_str());
    }
*/
    if (status.ok()) {
        Log(options.info_log, 5, "Defragmented value file %llu. status=%s", (unsigned long long) fnumber, status.ToString().c_str());
    } else {
        Log(options.info_log, 5, "Failed to defragment value file %llu. status=%s", (unsigned long long) fnumber, status.ToString().c_str());
    }
    return status;
}

bool Disk::isSuperBlockGood(int fd, uint64_t addr) {
    char *buf;
    if (posix_memalign((void**)&buf, pageSize_, pageSize_)) {
        return false;
    }
    if (lseek(fd, addr, SEEK_SET) == -1) {
           stringstream ss;
           ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << ": " << strerror(errno);
           Status::IOError(ss.str());
	   cout << ss.str() << endl;
    }

    int r = read(fd, buf, pageSize_);
    if ( r < 0 ){
        free(buf);
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << ": " << strerror(errno);
        Status::IOError(ss.str());
	     cout << ss.str() << endl;
        return false;
    }
    uint64_t magic_num;
    magic_num = DecodeFixed64(buf);
    if (magic_num != MAGIC_NUMBER){
        free(buf);
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__<< ": Bad superblock: #" << addr/Disk::ZONE_SIZE << endl;
        cout << "MAGIC NUMBER WAS NOT CORRECT  " << endl;
        return false;
    }
    bool status = DecodeFixed32(buf + sizeof(uint64_t));
    free(buf);
    return status;

}

Status Disk::getDiskInfo(const int fd) {
    Status s;
    numGoodSuperblocks_ = 0;
    Superblock* latestSuperblock = NULL;
    // Load all superblocks and determine the latest one
    for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
        zoneStartAddr_ = ZONE_SIZE;
        zoneSize_ = ZONE_SIZE;
        numZones_ = (capacity_ / ZONE_SIZE);
        nUsedZones_ = 0;
        nFreeZones_ = numZones_;
        Superblock* superblock = *it;
        superblock->clear();
        s = superblock->load();
        superblock->transferDiskInfoFromDisk(this);
        this->clear();
        if (superblock->isGood()) {
            if (latestSuperblock) {
                if (superblock->seqNum() > latestSuperblock->seqNum()) {
                    latestSuperblock->clear();
                    latestSuperblock = superblock;
                } else if (superblock->seqNum() < latestSuperblock->seqNum()) {
                    superblock->clear();
                    superblock->good(false);
                }
            } else {
                latestSuperblock = superblock;
            }
        } else {
            if (current_) {
               delete current_;
               current_ = NULL;
            }
            if(zoneUseMap_) {
                free(zoneUseMap_);
                zoneUseMap_ = NULL;
            }
            superblock->clear();
        }
    }
    if (latestSuperblock) {
        latestSuperblock->transferDiskInfoToDisk(this);
        numGoodSuperblocks_ = 1;
        // Persist snapshot onto other superblocks using the loaded latest superblock.
        for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
            Superblock* superblock = *it;
            if (superblock != latestSuperblock) {
                if (numGoodSuperblocks_ < 2) {
                    superblock->seqNum(latestSuperblock->seqNum());
                    s = superblock->persistSnapshot();
                    if (superblock->isGood()) {
                        ++numGoodSuperblocks_;
                    }
                } else {
                    superblock->good(false);
                }
            }
        }
        // Persist snapshot to the latest good snapshot
        if (numGoodSuperblocks_ > 1) {
            s = latestSuperblock->persistSnapshot();
            if (!s.ok()) {
                --numGoodSuperblocks_;
                // Persist snapshot to the spare superblock
                for (vector<Superblock*>::iterator it = superblocks_.begin(); numGoodSuperblocks_ < 2 && it != superblocks_.end(); ++it) {
                    Superblock* superblock = *it;
                    if (superblock != latestSuperblock && !superblock->isGood()) {  // Try with the previous bad/spare superblock
                        superblock->seqNum(latestSuperblock->seqNum());
                        s = superblock->persistSnapshot();
                        if (s.ok()) {
                           ++numGoodSuperblocks_;
                        }
                    }
                }
            } else {
                ++numGoodSuperblocks_;
            }
        }
    }
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": status = " << s.ToString() << endl;
    if (numGoodSuperblocks_ > 0) {
        s = Status::OK();
        if (numGoodSuperblocks_ > 2) {
            numGoodSuperblocks_ = 2;
        }
    }
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": status = " << status_.code() << ", numGoodSuperblocks_ = " << numGoodSuperblocks_ << ": ";
    for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
        Superblock* superblock = *it;
        if (superblock->isGood()) {
            cout << superblock->number() << " ";
        }
    }
    cout << endl;
    status_ = s;
    return s;
}

void Disk::clearZoneUseMap() {
    for (int i = 0; i < Disk::NUMBER_SUPERBLOCKS; ++i) {
        Disk::superblockStatus[i] = true;
    }
    memset(zoneUseMap_, 0, zoneUseMapSize_);
    // The first 3 zones (0, 1, and 2) are not used by SMR host aware.  Our used map structure kept usage status
    // from zone 1 and up only.  So, there is no need to set zone 0 to used.
    setUsedZone(0);
    setUsedZone(1);
    setUsedZone(2);
    setUsedZone(3); // Meta Data Partition (SDA5) within Linux

    // Set zones > total number of zones to used.
    if (zoneUseMapSize_ * CHAR_BIT > numZones_) {
        int lastIntIdx = (zoneUseMapSize_/sizeof(unsigned int) - 1);
        unsigned int* uintPtr = (reinterpret_cast<unsigned int*>(zoneUseMap_)); // + lastIntIdx;
        for (int i = 0; i < lastIntIdx; ++i) {
            ++uintPtr;
        }
        *uintPtr = ~(*uintPtr);
        int nExcess = zoneUseMapSize_*CHAR_BIT - numZones_ + 1;
        *uintPtr <<= (sizeof(unsigned int)*CHAR_BIT - nExcess);
    }
    nUsedZones_ = 0;
    nFreeZones_ = numZones_;
}

void Disk::clear() {
#ifdef KDEBUG
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Disk before clear:" << endl;
    cout << *this << endl;
#endif
    this->numGoodSuperblocks_ = 0;
    for (int i = 0; i < 10; ++i) {
        delete levels_[i];
        levels_[i] = new Level(i, this);
    }
    if (fileMap_) {
       fileMap_->clear();
    } else {
        fileMap_ = new FileMap();
    }
    if (valueFileMap_) {
       valueFileMap_->clear();
    } else {
        valueFileMap_ = new FileMap();
    }
    manifestInfo_= NULL;
    delete current_;
    current_ = new std::string("");
    if (zoneUseMap_) {
        if (numZones_ != (capacity_ / ZONE_SIZE)) {
            numZones_ = (capacity_ / ZONE_SIZE);
            int nIntegers = (numZones_ + sizeof(unsigned int)*CHAR_BIT - 1)/(sizeof(unsigned int)*CHAR_BIT);
            zoneUseMapSize_ = nIntegers*sizeof(unsigned int);
            free(zoneUseMap_);
            zoneUseMap_ = (unsigned char *) calloc(zoneUseMapSize_, sizeof(char));
        }
    } else {
        numZones_ = (capacity_ / ZONE_SIZE);
        int nIntegers = (numZones_ + sizeof(unsigned int)*CHAR_BIT - 1)/(sizeof(unsigned int)*CHAR_BIT);
        zoneUseMapSize_ = nIntegers*sizeof(unsigned int);
        zoneUseMap_ = (unsigned char *) calloc(zoneUseMapSize_, sizeof(char));
    }
    const std::string lockFileName = name_ + "/LOCK";
    DriveEnv::getInstance()->DeleteFile(lockFileName);
    clearZoneUseMap();
    int superZone = SUPERBLOCK_0_ADDR/ZONE_SIZE;;
    setUsedZone(superZone);
    setUsedZone(superZone + 1);
    setUsedZone(superZone + 2);
    setUsedZone(superZone + 3);  //PAD zone
    setUsedZone(superZone + 4);  //LOG zone
    setUsedZone(superZone + 5);  //LOG zone
    setUsedZone(superZone + 6);  //Pad zone
    currentLogZone_ = superZone + 4;
    for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
        (*it)->clearUpdates();
    }
#ifdef KDEBUG
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Disk after clear:" << endl;
    cout << *this << endl;
#endif
}

bool Disk::checkDiskInfoValid() {
    if (db_status_.ok()){
#ifdef KDEBUG
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Superblocks valid" << endl;
#endif
        return true;
    } else if (superblocks_.size() == 0 && db_status_.IsInvalidArgument()) {
#ifdef KDEBUG
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Both superblocks invalid" << endl;
#endif
        return false;
    }
    return false;
}

void Disk::FillZoneMap() {
    for (int i=4; i < numZones_; i++) {
        setUsedZone(i);
    }
}

bool Disk::ISE() {
/* Cannot use because execute_command not avail
    stringstream command;
    uint64_t seek_to;
    seek_to = Disk::SUPERBLOCK_0_ADDR/1048576;
    command << "dd if=/dev/zero of=" << store_partition_ << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1";

    std::string system_command = command.str();
    if (!execute_command(system_command)) {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to ISE" << endl;
        return false;
    }
    command.clear();
    seek_to = Disk::SUPERBLOCK_1_ADDR/1048576;
    command << "dd if=/dev/zero of=" << store_partition_ << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1";

    system_command = command.str();
    if (!execute_command(system_command)) {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to ISE" << endl;
        return false;
    }
*/
    return true;
}

bool Disk::zoneUsageLogBuilder(std::string &usage_str) {
    int wholeBuckets = 6;
    int partialBuckets = 3;
    uint32_t totalSstCount = 0;
    uint16_t totalZoneCount = 0;
    Level *pLvlZero = levels_[0];
    if (pLvlZero == nullptr) {
        std::cerr << "Error lvl0 pointer is null." << std::endl;
        return false;
    }
    std::set<unsigned int> snapshot_conditions{kZAC_RO_EMPTY, kZAC_RO_IMP_OPEN,
                                               kZAC_RO_EXP_OPEN, kZAC_RO_FULL, kZAC_RO_NON_SEQ};
    std::vector<vector<uint16_t>> zoneHistList;
    zoneHistList.reserve(wholeBuckets);
    pLvlZero->getZoneHistogram(zoneHistList);

    std::string zone_condition_snapshot{"ATA Zone Snapshot Init"};
    zone_condition_snapshot = zac_kin_->GetZoneConditionSnapShot(snapshot_conditions);

    std::stringstream ss(std::ios_base::out | std::ios_base::ate);
    ss << "\n ---------------------------------\n"
        << "  ZONE CONDITION SNAPSHOT\n"
        << " ---------------------------------\n";
        ss << zone_condition_snapshot;

    ss << "\n ---------------------------------\n"
       << " ------   ZONE SST HISTO   -------\n"
       << "  WholeSST  ->    Partial SST"
       << "\n ---------------------------------\n";

    int row_counter = 0;
    int col = 0;
    char stringSeperator = ' ';
    uint32_t currZoneCount = 0;
    uint32_t pairedPartialCount = 0;
    for (vector<uint16_t> &partial_list : zoneHistList) {
        currZoneCount = std::accumulate(partial_list.begin(), partial_list.end(),0);
        totalZoneCount += currZoneCount;
        pairedPartialCount += partial_list[1];
        totalSstCount += ((row_counter * currZoneCount) + partial_list[2]);
        ss << "    " << row_counter << "   ->   [";
        for (uint16_t &part : partial_list) {
            stringSeperator = (col < partialBuckets-1) ? (',') : ('\0');
            ss << std::dec << part << stringSeperator;
            col++;
        }
        ss << "]\n";
        row_counter++;
        col = 0;
    }

    totalSstCount += (pairedPartialCount / 2);
    ss << " ---------------------------------\n"
       << " -Effective SSTs: " << totalSstCount << "\n -Total Zones: " << totalZoneCount << "\n"
       << " ---------------------------------" << std::endl;

    usage_str = ss.str();
    pLvlZero = nullptr;
    return true;
}

Zone* Disk::allocateZone(FileType type) {
    MutexLock l(mu_);
    uint64_t lba = 0;
    int band = getFreeZone(type);
    Zone* zone = NULL;
    if (band == -1) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": No free zone available";
        Status::IOError(ss.str());
        return NULL;
    }
    int status = zac_kin_->AllocateZone(band, &lba);
    if (status != 0) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to allocate zone OR reset WP.  zone = "
           << band << ", lba = " << lba << ", status = " << status << endl;
        ZacZone zacZone;
        zac_kin_->GetZoneInfo(&zacZone, band);
        ss << zacZone << endl;
        Status::IOError(ss.str());
        return NULL;
    }
    if((uint64_t)band*256*1024*1024/512 != lba) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__
           << ":MISMATCHED " <<(uint64_t)band*256*1024*1024/512 << " LBA " << lba;
        Status::IOError(ss.str());
    } else {
        setUsedZone(band);
        zone = new Zone(band);
    }
    return zone;
}

}  // namespace smr
//}  // namespace leveldb




