/*
 * ManifestSequentialFile.cc
 *
 *  Created on: Nov 3, 2015
 *      Author: tri
 */

#include "ManifestSequentialFile.h"
#include <vector>
#include "Segment.h"
#include "leveldb/status.h"

using namespace std;
using namespace leveldb;

namespace smr {

uint64_t ManifestSequentialFile::GetMaxContiguousSize() {
    uint64_t capSize = 0;
    if (currentIt_ == finfo_->getSegments()->end()) {
        return capSize;
    }
    int nZone = (*currentIt_)->getZoneNumber();
    uint64_t lastBlockEnd = (*currentIt_)->getAddr();
    vector<Segment*>::iterator currentIt = currentIt_;
    vector<Segment*>::iterator lastCurrentIt = currentIt_;
    while (currentIt != finfo_->getSegments()->end()) {
        if (nZone != (*currentIt)->getZoneNumber()) {
            break;
        }
        if ((*currentIt)->getAddr() != lastBlockEnd) {
            break;
        }
        capSize += (*currentIt)->getSizeIn4KAlignedBytes();
        lastBlockEnd = (*currentIt)->getAddr() + (*currentIt)->getSizeIn4KAlignedBytes();
        ++currentIt;
    }
    return capSize;
}

bool ManifestSequentialFile::MapNewRegion() {
    if (file_offset_ == finfo_->getSize()) {
        return true;
    }
    //  Collect all contiguous segments whose sum of their occupied block sizes is <=
    //  our buffer size (1M) and they are in the same zone.
    if (contiguousSize_ == 0) {
        contiguousSize_ = this->GetMaxContiguousSize();
    }
    uint64_t readSize = min(contiguousSize_, uint64_t(512*1024));
    uint64_t regionStartAddr = (*currentIt_)->getAddr() + currentSegmentOff_;
    assert(regionStartAddr % 4096 == 0);
    uint64_t regionBoundAddr = regionStartAddr + readSize;
    assert(regionBoundAddr % 4096 == 0);
    if (lseek(fd_, regionStartAddr, SEEK_SET) == -1) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": Failed to seek, " << strerror(errno);
        Status::IOError(ss.str()); // Log error
        return false;
    }
    int n = ::read(fd_, base_, readSize);
    if (n < 0) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": Failed to read, " << strerror(errno);
        Status::IOError(ss.str()); // Log error
        return false;
    }
    assert(n == readSize);
    contiguousSize_ -= n;
    new_base_ = base_;
    dst_ = base_;
    limit_ = base_ + readSize;
    return true;
}
Status ManifestSequentialFile::Read(size_t n, Slice* result, char* scratch) {
    MutexLock lock(&mu_);
    Status s;
    size_t to_read = min(uint64_t(n), finfo_->getSize() - read_);
    size_t left = to_read;

    while (left > 0 && currentIt_ != finfo_->getSegments()->end()) {
        size_t avail = limit_ - dst_;
        if (avail == 0) {
            if (!UnmapCurrentRegion() || !MapNewRegion()) {
                stringstream ss;
                ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
                return Status::IOError(ss.str());
            }
            avail = limit_ - dst_;
        }

        // Loop through each segment to copy data excluding its roundup portion
        while (avail > 0 && left > 0 && currentIt_ != finfo_->getSegments()->end()) {
             size_t n = min(left, size_t((*currentIt_)->getSize() - currentSegmentOff_));
             n = min(n, avail);
            int r = ExtractData(*currentIt_, n, scratch + to_read - left);
            assert( r == n);
            if (currentSegmentOff_ == (*currentIt_)->getSize()) {
                dst_ += (*currentIt_)->getSizeIn4KAlignedBytes() - (currentSegmentOff_ - r);
                ++currentIt_;
                currentSegmentOff_ = 0;
            } else if (currentSegmentOff_ < (*currentIt_)->getSize()){
                dst_ += r;
            } else {
                assert(currentSegmentOff_ <= (*currentIt_)->getSize());
            }
            file_offset_ += r;
            left -= r;
            read_ += r;
            avail = limit_ - dst_;
        }
    }
    *result = Slice(scratch, to_read);
    return s;
}
Status ManifestSequentialFile::Skip(uint64_t n) {
    MutexLock lock(&mu_);
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": SKIPING............... " << n << endl;
    n = min(n, uint64_t(finfo_->getSize() - read_));
    while (n > 0) {
        int skipped = SkipInCurrent(n);
        n -= skipped;
        read_ += skipped;
    }


    read_ += n;
    uint64_t avail = limit_ - dst_;

    if (n <= avail) {
        dst_ += n;
    } else {
        UnmapCurrentRegion();
        file_offset_ = read_;
    }
    return Status::OK();
}

} /* namespace smr */
