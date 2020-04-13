/*
 * SmrDirectWritableFile.cc
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */
#include "SmrDirectWritableFile.h"
#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "Util.h"

using namespace leveldb;

//namespace leveldb {
namespace smr {

uint64_t SmrDirectWritableFile::GetCurrentFileSize()
{
  return (finfo_->getSize());
}

Status SmrDirectWritableFile::Append(const Slice& data)
{
    MutexLock lock(&mu_);
    if (fd_ < 0) {
        return Status::IOError(this->filename_, "File is not open");
    }

    Segment* segment = finfo_->getLastSegment();
    const char* write_pointer = data.data();

    /* We always write page sized. Since buffers used by smrdb are page sized as well, we can
    * assume nothing bad will happen. */
    uint32_t left = ROUNDUP(data.size(), page_size_);
    if(finfo_->getType() == kValueFile && segment->getSpaceLeft() < left) {
      return Status::NoSpaceAvailable(filename_, "ValueFile is single-segment only.");
    }
    int nTries = 3;
    size_t written = 0;
    Status status;
    synchronized_ = false;

    while (left && nTries) {
        off_t addr = ROUNDUP(segment->getAddr() + segment->getSize(), page_size_);
        written = ::pwrite(fd_, write_pointer, left, addr);
        if (written > 0) {
            if (written % page_size_ != 0) {
               stringstream ss;
               ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
               ss << ": Not written in block";
               ss << endl << "File info: " << *finfo_ << endl;
               status = Status::IOError(ss.str());
               break;
            }
            write_pointer += written;
            left -= written;
            finfo_->addSize(written);
        } else if (written < 0) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
            ss << "RETRY IN DIRECTW: Not written in block";
            ss << endl << "File info: " << *finfo_ << endl;
            Status::IOError(ss.str());
            --nTries;
            sleep(1);
        }
    }
    if (written == -1) {
               stringstream ss;
               ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << filename_ << ": " << strerror(errno);
               ss << endl << "File info: " << *finfo_ << endl;
               status = Status::IOError(ss.str());
    }
    sync_file_range(fd_, finfo_->getAddr() + lastSyncOffset_, finfo_->getSize() - lastSyncOffset_,
                  SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER);
    lastSyncOffset_ += (finfo_->getSize() - lastSyncOffset_);

    return status;
}

Status SmrDirectWritableFile::Close()
{  
  MutexLock lock(&mu_);
  Status s;
  if (fd_ < 0) {
    return s;
  }
  if (finfo_) {
    Segment* lastSegment = finfo_->getLastSegment();
    lastSegment->update(0);
  }
  int opResult = 0;
  fdatasync(fd_);
/*
  if (!synchronized_) {
      opResult = sync_file_range(fd_, finfo_->getAddr(), finfo_->getSize(),
                      SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER);
  }
*/
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

  return s;
}

Status SmrDirectWritableFile::Sync()
{
  MutexLock lock(&mu_);
  if (fd_ < 0 || synchronized_) {
    return Status::OK();
  }
/*
  sync_file_range(fd_, finfo_->getAddr() + lastSyncOffset_, finfo_->getSize() - lastSyncOffset_,
                  SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER);
  lastSyncOffset_ += (finfo_->getSize() - lastSyncOffset_);
*/
  fdatasync(fd_);
  synchronized_ = true;
  Segment* lastSegment = finfo_->getLastSegment();
  lastSegment->update(0);
  return Status::OK();
}
}  // namespace smr
//}  // namespace leveldb

