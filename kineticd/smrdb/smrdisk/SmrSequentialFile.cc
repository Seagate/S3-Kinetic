/*
 * SmrSequentialFile.cc
 *
 *  Created on: Apr 20, 2015
 *      Author: tri
 */
#include "SmrSequentialFile.h"

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "Util.h"

using namespace leveldb;

//namespace leveldb {
namespace smr {

bool SmrSequentialFile::UnmapCurrentRegion() {
    limit_ = base_;
    dst_ = base_;
    new_base_ = base_;
    return true;
}

bool SmrSequentialFile::MapNewRegion() {
    if (file_offset_ == finfo_->getSize()) {
        return true;
    }
    uint64_t regionStartAddr = finfo_->getAddr(file_offset_);
    uint64_t readSize = finfo_->getSizeToSegmentEnd(file_offset_);
    readSize = min(readSize, uint64_t(1024*1024));

    uint64_t regionBoundAddr = regionStartAddr + readSize;
    ssize_t n = ::pread(fd_, base_, readSize, regionStartAddr);
    if (n < 0) {
        return false;
    }
    assert(n == readSize);
    file_offset_ += readSize;
    new_base_ = base_;
    dst_ = new_base_;
    limit_ = base_ + readSize;
    return true;
}

Status SmrSequentialFile::Read(size_t n, Slice* result, char* scratch) {
    MutexLock lock(&mu_);
    Status s;
    if (finfo_ == NULL) { // CURRENT file has no finfo
        if (file_offset_ == 0) {
            disk_->getCURRENT(n, result, scratch);
            if ( result->size() == 0)
                return Status::IOError(filename_);
            file_offset_ += result->size();
        }
        return s;
    }
    size_t to_read = min(uint64_t(n), finfo_->getSize() - read_);
    size_t left = to_read;
    while (left > 0) {
        size_t avail = limit_ - dst_;
        if (avail == 0) {
            if (!UnmapCurrentRegion() ||
                    !MapNewRegion()) {
                return Status::IOError(filename_, strerror(errno));
            }
            avail = limit_ - dst_;
        }

        size_t r = (left <= avail) ? left : avail;
        memcpy(scratch+(to_read-left), dst_, r);
        dst_ += r;
        left -= r;
        read_ += r;
    }
    *result = Slice(scratch, to_read);
    return s;
}

Status SmrSequentialFile::Skip(uint64_t n) {
    if (finfo_ == NULL) { // CURRENT file has no file info
        return Status::OK();
    }
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": SKIPING............... " << n << endl;
    n = min(n, uint64_t(finfo_->getSize() - read_));
    read_ += n;
    uint64_t avail = limit_ - dst_;

    if (n <= avail) {
        dst_ += n;
    } else {
        UnmapCurrentRegion();
        file_offset_ = read_;
    }
    return Status::OK();

    /*
    if (finfo_ == NULL) { // CURRENT fill has no file info
        return Status::OK();
    }
    n = min(n, uint64_t(finfo_->getSize() - read_));
    uint64_t left = n;
    while (left > 0) {
        uint64_t avail = limit_ - dst_;
        if (avail == 0) {
            if (!UnmapCurrentRegion()) {
                return Status::IOError(filename_, "Failed to unmap");
            }
            avail = left;
        }
        uint64_t r = min(left, avail);
        read_ += r;
        left -= r;
        dst_ += r;
    }
    return Status::OK();
    */
}

}  // namespace smr
//}  // namespace leveldb

