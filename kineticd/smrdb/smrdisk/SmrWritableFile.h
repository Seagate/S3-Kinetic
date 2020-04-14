/*
 * SmrWritableFile.h
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */

#ifndef SMRWRITABLEFILE_H_
#define SMRWRITABLEFILE_H_

#include <iostream>

#include "util/env_posix.h"
#include "Disk.h"
#include "SmrFile.h"
#include "smrdisk/DriveEnv.h"
#include "mem/DynamicMemory.h"
#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "common/Listenable.h"

using namespace com::seagate::common;
using namespace leveldb;
using namespace std;

//namespace leveldb {
namespace smr {

class SmrWritableFile : public WritableFile, public SmrFile, public Listenable {
    public:
        SmrWritableFile(const std::string& fname, int fd, Disk* const& disk,
                size_t page_size, FileInfo* finfo)
    : SmrFile(fname, fd, disk, page_size, finfo),
      base_(NULL), new_base_(NULL), limit_(NULL), dst_(NULL), file_offset_(0) {
        nNewSegments_ = 0;
        nPhysicalSize_ = 0;
        if (finfo_ && finfo_->getLastSegment()) {
            ++nNewSegments_;
            nPhysicalSize_ = finfo_->getLastSegment()->getSizeIn4KAlignedBytes();
        }	
        nNewSegmentsAfterLastSync_ = nNewSegments_;
        base_ = NULL;
        int s = posix_memalign((void**)&base_, 4096, ALIGNED_MEM_SIZE_5M);

        if(base_ == NULL || s !=0) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__
               << ":CAN NOT ALLOCATE MEMORY IN SMRWRITABLE";
            Status::IOError(ss.str());
#ifdef KDEBUG
    	    cout << " CAN NOT ALLOCATE MEMORY IN WRITABLE" << endl;
    	    abort();
#endif
    	}
        dst_ = base_;
        new_base_ = base_;
        limit_ = base_;
        cur_file_size_ = 0;
        available_ = 0;
        synchronized_ = true;
//#ifndef NDEBUGW
        start_time_ = DriveEnv::getInstance()->NowMicros();
	    totalcopytime_ = 0;
	    totaltimewritetodisk_ = 0;
//#endif
        }

        virtual ~SmrWritableFile() {
            if (fd_ > 0) {
                Close();
            }
            if (base_) {
                free(base_);
	        base_ = NULL;
            }
        }
        virtual Status Append(const Slice& data);
        virtual Status Close();
        virtual Status Sync();
	virtual off_t GetAddr();
	virtual uint64_t GetCurrentFileSize();
	int GetFd() {return fd_;};
        virtual Status Flush() {
            return Status::OK();
        }
        virtual uint64_t GetSize() {
        	return SmrFile::getSize();
        }
        uint64_t getPhysicalSize() {
            return nPhysicalSize_;
        }
        int numNewSegments() const {
            return nNewSegments_;
        }
        void numNewSegments(int nSegments) {
            nNewSegments_ = nSegments;
        }

    private:
        Status UnmapCurrentRegion(bool sync=false);
        Status MapNewRegion();

    protected:
        char* base_;
        char* new_base_;
        char* limit_;
        char* dst_;
    	uint64_t totalcopytime_;
    	uint64_t totaltimewritetodisk_;
    	uint64_t cur_file_size_;
        uint64_t file_offset_;
        uint64_t available_;
        uint64_t start_time_;
        bool synchronized_;
        port::Mutex mu_;
        int nNewSegments_;
        int nNewSegmentsAfterLastSync_;
        uint64_t nPhysicalSize_;
        Segment* lastSegAfterLastSync_;
        off_t lastSegAfterLastSyncSize_;

};
}  // namespace smr
//}  // namespace leveldb

#endif /* SMRWRITABLEFILE_H_ */
