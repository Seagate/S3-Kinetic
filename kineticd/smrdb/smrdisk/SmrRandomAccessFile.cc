/*
 * SmrRandomAccessFile.cc
 *
 *  Created on: Apr 20, 2015
 *      Author: tri
 */

#include "SmrRandomAccessFile.h"

#include <sstream>

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "Util.h"

using namespace leveldb;

//namespace leveldb {
namespace smr {

Status SmrRandomAccessFile::Read(uint64_t offset, size_t n, Slice* result, char* scratch)
{
  MutexLock l(mu_);
  // Validate precondition.  The only thing we can't validate
  // is the size of scratch buffer.
  if (scratch == NULL || offset < 0 || offset > finfo_->getSize() || n < 0 || n > 5* 1024 * 1024 + 264 * 1024 ||
      n > finfo_->getSize() - offset) {
    stringstream ss;
    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_
       << ": scratch = " << (void*) scratch << ", offset = "
       << offset << ", fsize = " << finfo_->getSize() << ", type = " << finfo_->getType()
       << ", n = " << n << endl;
    ss << "File info: " << *finfo_ << endl;
    return Status::InvalidArgument(ss.str());
  }
#ifdef KDEBUG
  uint64_t start, end;
  start = DriveEnv::getInstance()->NowMicros();
  cout << "READ RAND " << n << " OFFSET " << offset << " from " << filename_ << endl;
#endif
  Status status;

  file_offset_ = offset;
  size_t to_read = min(uint64_t(n), finfo_->getSize() - file_offset_);
  size_t left = to_read;
  if (finfo_->getType() == kTableFile || finfo_->getType() == kValueFile) {
    saved_base_ = base_;
    base_ = scratch;
  }
  UnmapCurrentRegion();
  while (left > 0) {
    size_t avail = limit_ - dst_;

    if (avail == 0) {
      uint64_t maxMap = min(left, size_t(5* 1024 * 1024 + 264 * 1024));
      UnmapCurrentRegion();
      status = MapNewRegion(maxMap);
      if (!status.ok()) {
        if (finfo_->getType() == kTableFile || finfo_->getType() == kValueFile) {
          base_ = saved_base_;
        }
        return status;
      }

      avail = limit_ - dst_;
      if (avail <= 0) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": avail = " << avail << endl;
        ss << "File info: " << *finfo_ << endl;
        Status::IOError(ss.str());
      }
    }
    size_t r = (left <= avail) ? left : avail;
    if (finfo_->getType() != kTableFile && finfo_->getType() != kValueFile) {
      memcpy(scratch + (to_read - left), dst_, r);
    } else {
      base_ += r;
    }
    dst_ += r;
    left -= r;
  }
  *result = Slice(scratch, to_read);
  UnmapCurrentRegion();
  if (finfo_->getType() == kTableFile || finfo_->getType() == kValueFile) {
    base_ = saved_base_;
  }
#ifdef KDEBUG
  end = DriveEnv::getInstance()->NowMicros();
  cout << " ----READ RAND TIME " << (end-start)
       << " for " << n << " FROM " << filename_ << endl;
#endif
  return status;

}

bool SmrRandomAccessFile::UnmapCurrentRegion()
{
  if (finfo_->getType() != kTableFile && finfo_->getType() != kValueFile) {
    if (base_) {
      free(base_);
      base_ = NULL;
    }
  }
  limit_ = base_;
  dst_ = base_;
  new_base_ = base_;
  return true;
}

Status SmrRandomAccessFile::MapNewRegion(uint64_t maxMap)
{
  Status status;
  if (file_offset_ == finfo_->getSize()) {
    return status;
  }
  uint64_t regionStartAddr = finfo_->getAddr(file_offset_);
  uint64_t readSize = finfo_->getSizeToSegmentEnd(file_offset_);
  readSize = min(readSize, maxMap);
  uint64_t regionBoundAddr = regionStartAddr + readSize;

  if (base_ == NULL) {
    base_ = (char*) calloc(5* 1024 * 1024 + 264 * 1024, sizeof(char));
    if (base_ == NULL) {
      stringstream ss;
      ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
      return Status::IOError(ss.str());
    }
    dst_ = base_;
    new_base_ = base_;
    limit_ = base_;
  }
  ssize_t nRead = 0;
  uint64_t left = readSize;
  int nTries = 3;
  do {
    if (finfo_->getType() != kTableFile && finfo_->getType() != kValueFile) {
      nRead = ::pread(fd_, base_ + readSize - left, left, regionStartAddr);
     } else {
      nRead = ::pread(fd_, new_base_, left, regionStartAddr);
    }
    if (nRead > 0) {
      left -= nRead;
      new_base_ += nRead;
      limit_ += nRead;
      file_offset_ += nRead;
      regionStartAddr += nRead;
    } else if (nRead < 0) {
      --nTries;
      sleep(1);
    }
  } while (left > 0 && nTries >= 0);

  if (nRead < 0 || left > 0) {
    stringstream ss;
    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno)
       << ", fd = " << fd_ << ", type = " << finfo_->getType() << ", base = " << (void*) base_
       << ", new base = " << (void*) new_base_
       << endl << ", dst = " << (void*) dst_ << ", limit = " << (void*) limit_
       << ", readSize = " << readSize << ", nRead = " << nRead << ", left = " << left << ", regionStartAddr = "
       << regionStartAddr
       << ", nTries = " << nTries << endl;
    ss << "File info: " << *finfo_ << endl;
    return Status::IOError(ss.str());
  } else if (nTries < 3) {
    stringstream ss;
    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_
       << ": successful with #retries = " << 3 - nTries << endl;
    Status::NotSupported(ss.str());
  }
  if (finfo_->getType() != kTableFile && finfo_->getType() != kValueFile) {
    new_base_ = base_;
    dst_ = new_base_;
    limit_ = base_ + readSize;
  }
  return status;
}

}  // namespace smr
//}  // end of namespace leveldb




