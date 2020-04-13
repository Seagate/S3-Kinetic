#ifndef FILE_INFO_H_
#define FILE_INFO_H_

#include <vector>
#include <iostream>
#include <sstream>

#include "db/filename.h"
#include "util/coding.h"
#include "Segment.h"
#include "Util.h"
#include <mutex>

using namespace std;
using namespace leveldb;

namespace smr {

class Level;
class Disk;

class FileValueInfo {
  // enable direct access for serialization
  friend class FileInfo;
public:
  friend ostream& operator<<(ostream& out, const FileValueInfo& src);

  // return total number of values in the file
  uint16_t getTotal() const;
  // return total number of deleted values in the file
  uint16_t getDeleted() const;
  // increment total by size
  void incrTotal(uint16_t size);
  // increment deleted by size
  void incrDeleted(uint16_t size);

  // Constructor
  FileValueInfo() : num_total(0), num_deleted(0) {}
  FileValueInfo(const FileValueInfo& src) {
      num_total = src.num_total;
      num_deleted = src.num_deleted;
  }
  char* serialize(char* dest) const {
      std::lock_guard<std::mutex> lock(mutex_);
      memcpy(dest, &num_total, sizeof(num_total));
      dest += sizeof(num_total);
      memcpy(dest, &num_deleted, sizeof(num_deleted));
      dest += sizeof(num_deleted);
      return dest;
  }
  void serialize(string& dest) const {
      std::lock_guard<std::mutex> lock(mutex_);
      dest.append((char*) &num_total, sizeof(num_total));
      dest.append((char*) &num_deleted, sizeof(num_deleted));
  }

  bool deserialize(Slice& src) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* ptr = src.data();
    memcpy((char*)&num_total, ptr, sizeof(num_total));
    ptr += sizeof(num_total);
    memcpy((char*)&num_deleted, ptr, sizeof(num_deleted));
    ptr += sizeof(num_deleted);
    src = Slice(ptr, src.size() - (ptr - src.data()));
    return true;
}
  void totalKeys(uint16_t nTotal) {
      std::lock_guard<std::mutex> lock(mutex_);
      num_total = nTotal;
  }
  void deletedKeys(uint16_t nDeleteds) {
      std::lock_guard<std::mutex> lock(mutex_);
      num_deleted = nDeleteds;
  }
  FileValueInfo& operator=(const FileValueInfo& rhs) {
      num_total = rhs.getTotal();
      num_deleted = rhs.getDeleted();
      return *this;
  }

private:
  // total number of values in value file
  uint16_t num_total;
  // number of deleted values in value file
  uint16_t num_deleted;
  // static mutex shared among all objects... there is (almost) zero contention
  static std::mutex mutex_;
};

class FileInfo {
    public:
        friend ostream& operator<<(ostream& out, FileInfo& src);

        FileInfo() : number_(0), type_(kTempFile), level_(NULL) {
        }

        FileInfo(uint64_t number, FileType type, Segment* segment) :
            number_(number), type_(type), level_(NULL) {
            if (segment) {
                addSegment(segment);
            }
        }

        virtual ~FileInfo() {
            for (uint i = 0; i < segments_.size(); ++i) {
                delete segments_[i];
            }
        }

        FileType getType() {
            return type_;
        }

        void setType(FileType type) {
            type_ = type;
        }

        uint64_t getSize() const {
            uint64_t size = 0;
            for (uint i = 0; i < segments_.size(); ++i) {
                size += segments_[i]->getSize();
            }
            return size;
        }
        uint64_t getAddr(uint64_t offset = 0) {
        	if (segments_.size() == 0) {
        		return -1;
        	}
            if (offset == 0) {
                return segments_[0]->getAddr();
            }
            uint64_t fsize = getSize();
            if (offset > fsize) {
                offset = fsize;
            }
            uint i = 0;
            while (i < segments_.size() - 1) {
                if (offset < (uint64_t) segments_[i]->getSize()) {
                    break;
                }
                offset -= segments_[i]->getSize();
                ++i;
            }
            uint64_t offAddr = segments_[i]->getAddr() + offset;
            return offAddr;
        }

        Level* getLevel() const {
            return level_;
        }

        void setLevel(Level* level) {
            level_ = level;
        }

        uint64_t getNumber() const {
            return number_;
        }

        string getFileName() const;

        virtual void addSize(off_t n) {
            segments_[segments_.size() - 1]->addSize(n);
        }
        void addSegment(Segment* segment) {
            assert(segment);
            segment->setFileInfo(this);
            segments_.push_back(segment);
        }
        bool deserialize(Slice& src, Disk* disk, bool withSegment = true);
        void deserializeDeletedFile(Slice& src);
        void serialize(string& dest, bool withSegment = true);
        char* serialize(char* dest, bool withSegment = true);
        char* serializeDeletedFile(char* src);

        uint64_t getSizeToSegmentEnd(uint64_t offset) {
            uint64_t fsize = getSize();
            if (offset > fsize) {
                offset = fsize;
            }
            uint i = 0;
            while (i < segments_.size() - 1) {
                if (offset < (uint64_t)segments_[i]->getSize()) {
                    break;
                }
                offset -= segments_[i]->getSize();
                ++i;
            }
            uint64_t readSize = segments_[i]->getSize() - offset;
            return readSize;
        }

        void setNumber(uint64_t number) {
            number_ = number;
        }
        Segment* getLastSegment() {
            if (segments_.size() > 0) {
                return segments_[segments_.size() - 1];
            }
            return NULL;
        }
        Segment* replaceLastSegment(Segment* newSeg);
        vector<Segment*>* getSegments() {
            return &segments_;
        }
        int getNumSegments() {
            return segments_.size();
        }
        Status replaceSegment(Segment* oldSeg, vector<Segment*>& newSegs); 
        unsigned int getNumZones();
        void insert(Segment* seg, int idx) {
            seg->setFileInfo(this);
            if (idx == -1) {
                segments_.push_back(seg);
                return;
            }
            vector<Segment*>::iterator it = segments_.begin();
            for (; it != segments_.end() && idx > 0; ++it) {
                --idx;
            }
            segments_.insert(it, seg);
        }
        bool isDefragtable(string& dbname);
        void setAsNotWritable();
        Segment* removeSegment(Segment* seg);
        Segment* remove(int idx) {
            Segment* segment = NULL;
            if (idx >= 0 && (uint32_t)idx < segments_.size()) {
                segment = segments_[idx];
                segments_.erase(segments_.begin() + idx);
            }
            return segment;
        }
        int getIdx(Segment* seg) {
            int idx = -1;
            for (vector<Segment*>::iterator it = segments_.begin();
                    it != segments_.end(); ++it) {
                ++idx;
                if (seg->getAddr() == (*it)->getAddr()) {
                    break;
                }
            }
            return idx;
        }
        Segment* getSegment(int idx) {
            if (idx < 0 || (size_t)idx >= segments_.size()) {
                return NULL;
            }
            return segments_[idx];
        }

        FileValueInfo& values() {
          return value_info_;
        }
        void values(const FileValueInfo& info) {
            value_info_ = info;
        }
    protected:
        uint64_t number_;
        FileType type_;
        Level* level_;
        FileValueInfo value_info_; // keep member regardless of file type (4 byte)
        vector<Segment*> segments_;
};  // class FileInfo

}  // namespace smr

#endif // FILE_INFO_H_

