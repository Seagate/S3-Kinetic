/*
 * SmrWritableFile.cc
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */
#include "SmrWritableFile.h"

#include <sstream>

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "Util.h"

using namespace leveldb;

namespace smr {

off_t  SmrWritableFile::GetAddr() {

    return(off_t (finfo_->getAddr()));
}

uint64_t  SmrWritableFile::GetCurrentFileSize() {

    return(cur_file_size_);
}

Status SmrWritableFile::UnmapCurrentRegion(bool sync) {

    Status status;
    if (base_ == NULL || dst_ - new_base_ == 0) {
        return status;
    }
    Segment* lastSegment = finfo_->getLastSegment();
    off_t segSize = lastSegment->getSize();

    if (segSize % 4096 > 0 || lastSegment->getSpaceLeft() == 0) {
        lastSegment->complete();
        nPhysicalSize_ += lastSegment->getSizeIn4KAlignedBytes();
        lastSegment = finfo_->getLevel()->allocateSegment(finfo_->getType());
        if (lastSegment == NULL) {
            #ifndef NDEBUGW
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << ": Failed to allocateSegment" << endl;
            #endif // NDEBUGW
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": No free zones available";
            return Status::NoSpaceAvailable(ss.str());
        }
        finfo_->addSegment(lastSegment);
        ++nNewSegments_;
        segSize = lastSegment->getSize();
        notify();   // DISABLE MANIFEST BACKGROUND COMPACTION: Comment out this line
    }

    uint64_t segAddr = lastSegment->getAddr();
    uint64_t regionStartAddr = segAddr + segSize;
    ssize_t written = 0;
    uint64_t needToWrite = dst_ - new_base_;
    uint64_t left = needToWrite;
    int nTries = 3;
    do {
        assert(regionStartAddr % 4096 == 0);
        if (regionStartAddr % 4096 != 0) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
            ss << endl << ": regionStartAddr not block align = " << regionStartAddr
               << ", need to write = " << needToWrite << ", left = " << left << endl;
            ss << "Last Segment: " << *lastSegment << endl;
            status = Status::IOError(ss.str());
            break;
        }
#ifndef NDEBUGW
        uint64_t start, end;
        start = DriveEnv::getInstance()->NowMicros();
#endif
        written = ::pwrite(fd_, new_base_, ROUNDUP(left, 4096), regionStartAddr);
        if (written > 0) {
            written = min((size_t)written, (size_t)left);
            finfo_->addSize(written);
            left -= written;
            new_base_ += written;
            regionStartAddr += written;
        } else if (written < 0) {
            --nTries;
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
            ss << endl << "RETRY: regionStartAddr not block align = " << regionStartAddr
               << ", need to write = " << needToWrite << ", left = " << left << endl;
            ss << "Last Segment: " << *lastSegment << endl;
            Status::IOError(ss.str());
            sleep(1);
        }
#ifndef NDEBUGW
        end = DriveEnv::getInstance()->NowMicros();
        totaltimewritetodisk_ += (end-start);
        if ((end-start) > 2000000) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
            ss << endl << "WRITE TIME : regionStartAddr not block align = " << regionStartAddr
               << ", need to write = " << needToWrite << ", left = " << left << endl;
            ss << "Last Segment: " << *lastSegment << (end-start) << endl;
            Status::IOError(ss.str());
        }
        cout << "  WRITE TO DISK  " << filename_ << " " << written << " " << (end-start) << endl;
#endif
    } while (left > 0 && nTries >= 0);

    if (written == -1) {
           cout << "WRITTEN == -1 " << endl;
               stringstream ss;
               ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno)
                  << ", regionStart = " << regionStartAddr
                  << endl << ", written = " << written << ", base_ = " << (void*)base_
                  << ", new_base_ = " << (void*)(new_base_) << ", should be: " << ROUNDUP(left, 4096)
                  << endl << ", dst = " << (void*)dst_ << ", limit = " << (void*)limit_
                  << ", nTries = " << nTries << endl;
               ss << "space left = " << lastSegment->getSpaceLeft() << ", needToWrite = " << needToWrite << endl;
               ss << endl << "File info: " << *finfo_ << endl;
               status = Status::IOError(ss.str());
    } else if (nTries < 3) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ 
           << ": successful with #retries = " << 3 - nTries << endl;
        Status::NotSupported(ss.str());
    } 
    if (left == 0) {
        if (sync) {
           lastSegment->log();
        }
        dst_ = base_;
        new_base_ = base_;
    }
    return status;
}

Status SmrWritableFile::MapNewRegion() {
    Segment* lastSegment = finfo_->getLastSegment();
    uint64_t segAddr = lastSegment->getAddr();
    off_t segSize = lastSegment->getSize();
    Status status;
    if (segSize % 4096 > 0 || lastSegment->getSpaceLeft() == 0) {
        // Cross zone boundary.  Allocate new segment
        lastSegment->complete();
        nPhysicalSize_ += lastSegment->getSizeIn4KAlignedBytes();
        lastSegment = finfo_->getLevel()->allocateSegment(finfo_->getType());
        if (lastSegment == NULL) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": Failed to allocate segment";
            return Status::NoSpaceAvailable(ss.str());
        }
        segAddr = lastSegment->getAddr();
        segSize = lastSegment->getSize();
        finfo_->addSegment(lastSegment);
        ++nNewSegments_;
        notify();   // DISABLE MANIFEST BACKGROUND COMPACTION: Comment out this line
    }

    uint64_t regionStartAddr = ROUNDUP(segAddr + segSize, 4096);
    uint64_t regionBoundAddr;
    regionBoundAddr = regionStartAddr + 5*1024*1024;
    uint64_t zoneBoundAddr = TRUNCATE(regionStartAddr + Zone::ZONE_SIZE, Zone::ZONE_SIZE);
    regionBoundAddr = min(regionBoundAddr, zoneBoundAddr);
    available_ = regionBoundAddr - regionStartAddr;
    return status;
}

Status SmrWritableFile::Append(const Slice& data) {
    MutexLock lock(&mu_);
    Status status;
#ifndef NDEBUGW
    uint64_t start, end;
    start = DriveEnv::getInstance()->NowMicros();
    cout << "       APPEND " << data.size() << " FILE " << filename_ << endl;
#endif

    if (fd_ < 0) {
        return Status::IOError(this->filename_, "File is not open");
    }
    if (synchronized_ && finfo_->getType() == FileType::kDescriptorFile) {
        nNewSegmentsAfterLastSync_ = 0;
        lastSegAfterLastSync_ = finfo_->getLastSegment();
        lastSegAfterLastSyncSize_ = lastSegAfterLastSync_->getSize();
    }
    synchronized_ = false;
    const char* src = data.data();
    size_t left = data.size();

    while (left > 0) {
        if (available_ == 0) {
    	    status = UnmapCurrentRegion(false);
            if (!status.ok()) {
                return status;
            }
            status = MapNewRegion();
            if (!status.ok()) {
                return status;
            }
        }
        size_t n = min((uint64_t)left, available_);
#ifndef NDEBUGW
        uint64_t start1, end1;
        start1 = DriveEnv::getInstance()->NowMicros();
#endif        
        memcpy(dst_, src, n);
#ifndef NDEBUGW
        end1 = DriveEnv::getInstance()->NowMicros();
        cout << "       MEM COPY  " << n << " " << (end1-start1) << endl;
        totalcopytime_ += (end1-start1);
#endif

        dst_ += n;
	cur_file_size_ += n;
	available_ -= n;
        src += n;
        left -= n;
    }
#ifndef NDEBUGW
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----APPEND TIME  " << (end-start)
         << " for " << filename_ << endl;
#endif
    return status;
}

Status SmrWritableFile::Close() {
    MutexLock lock(&mu_);
#ifndef NDEBUGW
    cout << "CLOSE FILE " << filename_ << endl;
    uint64_t start, end;
    start = DriveEnv::getInstance()->NowMicros();
#endif

    Status s;
    if (fd_ < 0) {
        return s;
    }
    s = UnmapCurrentRegion(false);
    if(!s.ok()) {
        if (s.IsNoSpaceAvailable()) {
            return s;
        }
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
        return Status::IOError(ss.str());
    }
    available_ = 0;
    if (s.ok()) {
        if (finfo_) {
            Segment* lastSegment = finfo_->getLastSegment();
            lastSegment->complete();
            nPhysicalSize_ += lastSegment->getSizeIn4KAlignedBytes();
        }

        int opResult = 0;
        if (!synchronized_) {
            vector<Segment*>* segments = finfo_->getSegments();


            for (int i = segments->size() - nNewSegments_; opResult == 0 && i < segments->size(); ++i) {
    	        opResult = sync_file_range(fd_, (*segments)[i]->getAddr(), (*segments)[i]->getSize(),
	    		SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE);

                if (opResult == 0) {
                   --nNewSegments_;
                }
            }
        }
        if (opResult == 0) {
            ::close(fd_);
            fd_ = -1;
            synchronized_ = true;
        } else {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
            ss << endl << "File info: " << *finfo_ << endl;
            s = Status::IOError(ss.str());
        }
    } else {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
        ss << endl << "File info: " << *finfo_ << endl;
        s = Status::IOError(ss.str());
    }
#ifndef NDEBUGW
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----CLOSE TIME  " << (end-start)
         << " for " << filename_ << endl;
    cout << " ----FROM START TO CLOSE TIME  " << (end-start_time_)
         << " for " << filename_ << endl;
    cout << "TOTAL COPY TIME " << totalcopytime_ << endl;
    cout << "TOTAL TIME WRITE TO DISK " << totaltimewritetodisk_ << endl;
    cout << "TOTAL FILE SIZE " << cur_file_size_ << endl;

#endif

    return s;
}

Status SmrWritableFile::Sync() {
    MutexLock lock(&mu_);

#ifndef NDEBUGW
    cout << "SYNC FILE " << filename_ << endl;
    uint64_t start, end;
    start = DriveEnv::getInstance()->NowMicros();
#endif
    Status status;
    if (fd_ < 0 || synchronized_) {
        return Status::OK();
    }
    status = UnmapCurrentRegion(true);
    if(!status.ok()) {
        if (status.IsNoSpaceAvailable()) {
            return status;
        }
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
        ss << endl << "File info: " << *finfo_ << endl;
        return Status::IOError(ss.str());
    }
    available_ = 0;
    int opResult = 0;
    vector<Segment*>* segments = finfo_->getSegments();
    nNewSegmentsAfterLastSync_ = nNewSegments_;
    int newSegments = nNewSegments_;
    for (int i = segments->size() - newSegments; opResult == 0 && i < segments->size(); ++i) {
        opResult = sync_file_range(fd_, (*segments)[i]->getAddr(), (*segments)[i]->getSize(),
                   SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
        if (opResult == 0) {
            --newSegments;
        } else {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno)
               << ", fd = " << fd_ << ", addr = " << (*segments)[i]->getAddr() << ", size = " << (*segments)[i]->getSize() << endl;
            ss << endl << "File info: " << *finfo_ << endl;
            nNewSegments_ = newSegments;
            return Status::IOError(ss.str());
        }
    }
    nNewSegments_ = newSegments;
    if (opResult == 0) {
        this->synchronized_ = true;
    } else {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
        ss << endl << "File info: " << *finfo_ << endl;
        return Status::IOError(ss.str());
    }
#ifndef NDEBUGW
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----SYNC TIME  " << (end-start)
         << " for " << filename_
         << " size " << cur_file_size_ << endl;
#endif

    return status;
}
}  // namespace smr

