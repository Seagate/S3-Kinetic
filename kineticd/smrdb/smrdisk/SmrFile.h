/*
 * SmrFile.h
 *
 *  Created on: Apr 19, 2015
 *      Author: tri
 */

#ifndef SMRFILE_H_
#define SMRFILE_H_

#include <string>
#include "db/filename.h"
#include "Disk.h"
#include "Util.h"

using namespace std;
using namespace leveldb;

//namespace leveldb {
namespace smr {

class FileInfo;

class SmrFile {
    public:
        SmrFile(const std::string& fname, int fd,
                Disk* const& disk, size_t page_size, FileInfo* finfo):
                    filename_(fname), fd_(fd),
                    disk_(disk), page_size_(page_size), finfo_(finfo) {
            assert((page_size & (page_size - 1)) == 0);
        }
        virtual ~SmrFile() {
            if (fd_ >= 0 && finfo_) {
                close(fd_);
                fd_ = -1;
            }
        }
        uint64_t getSize() {
        	return finfo_->getSize();
        }
        FileInfo * getFileInfo() const {
            return finfo_;
        }

    protected:
        std::string filename_;
        int fd_;
        Disk *disk_;
        size_t page_size_;
        FileInfo* finfo_;

};

}  // namespace smr
//}  // namespace leveldb

#endif /* SMRFILE_H_ */
