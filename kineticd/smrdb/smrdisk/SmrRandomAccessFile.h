/*
 * SmrRandomAccessFile.h
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */

#ifndef SMRRANDOMACCESSFILE_H_
#define SMRRANDOMACCESSFILE_H_

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "SmrSequentialFile.h"
#include "smrdisk/DriveEnv.h"

using namespace leveldb;

//namespace leveldb {
namespace smr {

class SmrRandomAccessFile: public RandomAccessFile , public SmrFile {
    public:
        SmrRandomAccessFile(const std::string& fname, int fd,
                Disk* const& disk, size_t page_size, FileInfo* finfo)
        : SmrFile(fname, fd, disk, page_size, finfo),
          read_(0), file_offset_(0), base_(NULL), limit_(NULL), dst_(NULL), new_base_(NULL) {
            mu_ = new port::Mutex();
        }

        virtual ~SmrRandomAccessFile() {
           // close(fd_);
            if (base_) {
                free(base_);
                base_ = NULL;
            }
            delete mu_;
        }

        virtual Status Read(uint64_t offset, size_t n, Slice* result,
                char* scratch);

    private:
        bool UnmapCurrentRegion();
        Status MapNewRegion(uint64_t maxMap);

    private:
        port::Mutex *mu_;
        off_t read_;
        off_t file_offset_;
        char* base_;
	char* saved_base_;
        char* limit_;
        char* dst_;
        char* new_base_;
};

}  // namespace smr

#endif /* SMRRANDOMACCESSFILE_H_ */
