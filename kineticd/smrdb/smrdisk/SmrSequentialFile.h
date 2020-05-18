/*
 * SmrSequentialFile.h
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */

#ifndef SMRSEQUENTIALFILE_H_
#define SMRSEQUENTIALFILE_H_

#include <string>

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "mem/DynamicMemory.h"

using namespace std;
using namespace leveldb;

//namespace leveldb {
namespace smr {

class SmrSequentialFile: public SequentialFile, public SmrFile {
    public:
        SmrSequentialFile(const std::string& fname, int fd,
                Disk* const& disk, size_t page_size, FileInfo* finfo)
    : SmrFile(fname, fd, disk, page_size, finfo),
      read_(0), file_offset_(0), base_(NULL), limit_(NULL), dst_(NULL), new_base_(NULL) {
            base_ = NULL;
            int s = posix_memalign((void**)&base_, 4096, ALIGNED_MEM_SIZE);

            if(base_ == NULL || s !=0) {
                stringstream ss;
                ss << __FILE__ << ":" << __LINE__ << ":" << __func__
                   << ":CAN NOT ALLOCATE MEMORY IN SMRSEQ";
                 leveldb::Status::IOError(ss.str());
#ifdef KDEBUG
            cout << " CAN NOT ALLOCATE MEMORY IN WRITABLE" << endl;
            abort();
#endif
            }
            dst_ = base_;
            new_base_ = base_;
            limit_ = base_;
        }

        virtual ~SmrSequentialFile() {
            if (base_) {
                free(base_);
                base_ = NULL;
            }
        }
        virtual Status Read(size_t n, Slice* result, char* scratch);
        virtual Status Skip(uint64_t n);
        virtual uint64_t GetSize() {
        	return SmrFile::getSize();
        }

    protected:
        bool UnmapCurrentRegion();
        virtual bool MapNewRegion();

    protected:
        off_t read_;
        off_t file_offset_;
        char* base_;
        char* limit_;
        char* dst_;
        char* new_base_;
        port::Mutex mu_;

};
}  // namespace smr
//}  // namespace leveldb

#endif /* SMRSEQUENTIALFILE_H_ */
