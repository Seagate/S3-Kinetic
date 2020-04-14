/*
 * Segment.h
 *
 *  Created on: Apr 30, 2015
 *      Author: tri
 */

#ifndef SEGMENT_H_
#define SEGMENT_H_

#include "leveldb/slice.h"
#include "util/coding.h"

#include "Util.h"
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <stdlib.h>

using namespace std;
using namespace leveldb;
//namespace leveldb {
namespace smr {

class FileInfo;
class Zone;
class Level;
class FileMap;

class Segment {
    public:
        Segment() : addr_(-1), size_(0), zone_(NULL), finfo_(NULL) {
        }
        Segment(off_t addr, off_t size, FileInfo* finfo, Zone* zone) :
            addr_(addr), size_(size), zone_(zone), finfo_(finfo) {
        }
        Segment(Segment& src): addr_(src.addr_), size_(src.size_),
                zone_(src.zone_), finfo_(src.finfo_) {
        }
        virtual ~Segment() {
            // finfo_ and zone_ are not owned by segment.
        }
        Segment& operator=(const Segment& src) {
            addr_ = src.addr_;
            size_ = src.size_;
            finfo_ = src.finfo_;
            zone_ = src.zone_;
            return *this;
        }
        virtual off_t getSize() const {
            return size_;
        }
        virtual void setSize(off_t size) {
            size_ = size;
        }
        virtual off_t getAddr() const {
            return addr_;
        }
        void setAddr(off_t addr) {
            addr_ = addr;
        }

        FileInfo* getFileInfo() {
            return finfo_;
        }

        void setFileInfo(FileInfo* finfo) {
            finfo_ = finfo;
        }

        Zone* getZone() {
            return zone_;
        }
        void setZone(Zone* zone) {
            zone_ = zone;
        }

        friend ostream& operator<<(ostream& out, Segment& src);

        int getZoneNumber();

        virtual void addSize(off_t n);
        virtual void reduceSize(off_t n);

        bool deserialize(Slice& src, Level* level );

	    char* serialize(char* dest) const {
	        char* ptr;
	        ptr = dest;
            ptr = PutVarint64InBuff(ptr, addr_);
            ptr = PutVarint64InBuff(ptr, (uint64_t)size_);
	        return ptr;
        }


        void serialize(string& dest) const {
            PutVarint64(&dest, addr_);
            PutVarint64(&dest, (uint64_t)size_);
        }
        uint32_t getSpaceLeft();

        int read(int fd, uint32_t offset, char* buffer, uint32_t bufferSize) {
            int toRead = min(bufferSize, uint32_t(size_ - offset));
            if (lseek(fd, addr_ + offset, SEEK_SET) == -1) {
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << strerror(errno) << endl;
                return -1;
            }
            int n = ::read(fd, buffer, toRead);
            return n;
        }
        int write(int fd, char* buffer, uint32_t size) {
            size = min(size, getSpaceLeft());
            if (size == 0) {
                return 0;
            }
            assert((addr_ + size_) % 4096 == 0);
            if (lseek(fd, addr_ + size_, SEEK_SET) == -1) {
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << strerror(errno) << endl;
                return -1;
            }
            int written = ::write(fd, buffer, ROUNDUP(size, 4096));
            if (written > 0) {
                written = min(int(size), written);
                this->addSize(written);
            }
            return written;
        }
        void complete(int rIdx = -1);
        void update(int idx);

        void log(int rIdx = -1);
        void logUpdate(int rIdx);
        off_t size() const {
            return size_;
        }
        void size(off_t size) {
            size_ = size;
        }
        off_t getSizeIn4KAlignedBytes() {
            return ROUNDUP(getSize(), 4096);
        }
    protected:
        off_t addr_;
        off_t size_;
        Zone* zone_;
        FileInfo* finfo_;
};

}
//}



#endif /* SEGMENT_H_ */
