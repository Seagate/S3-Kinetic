/*
 * SmrDirectWritableFile.h
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */

#ifndef SMRDIRECTWRITABLEFILE_H_
#define SMRDIRECTWRITABLEFILE_H_

#include <iostream>

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "smrdisk/DriveEnv.h"
#include <iostream>
using namespace leveldb;
using namespace std;

namespace smr {

class SmrDirectWritableFile : public WritableFile, public SmrFile {
public:
  SmrDirectWritableFile(const std::string& fname, int fd, Disk* const& disk, size_t page_size, FileInfo* finfo)
      : SmrFile(fname, fd, disk, page_size, finfo) {
    fd_ = fd;
    synchronized_ = false;
    lastSyncOffset_ = finfo_->getAddr();
  }

  virtual ~SmrDirectWritableFile() {
    if (fd_ >= 0) {
      Close();
    }
  }

  virtual Status Append(const Slice& data);

  virtual Status Close();

  virtual Status Sync();

  virtual uint64_t GetCurrentFileSize();

  int GetFd() { return fd_; };

  virtual Status Flush() { return Status::OK(); }

private:
  uint64_t lastSyncOffset_;
  bool synchronized_;
  port::Mutex mu_;
};

}  // namespace smr
#endif /* SmrDirectWritableFile_H_ */
