/*
 * FileMap.cpp
 *
 *  Created on: Mar 18, 2015
 *      Author: tri
 */

#include "FileMap.h"

#include <sstream>
#include <inttypes.h>
#include <iostream>
#include "util/coding.h"

#include "Disk.h"
#include "Level.h"

using namespace std;

//namespace leveldb {
namespace smr {

ostream& operator<<(ostream& out, FileMap& src) {
    uint64_t totalSize = 0;
    unordered_map<uint64_t, FileInfo*>::iterator it;
    out << endl;
    for (it = src.map_.begin(); it != src.map_.end(); ++it ) {
        cout << *(it->second) << endl;
        totalSize += it->second->getSize();
    }

    out << "\t#Files = " << src.map_.size()
                        << ", Disk space used = " << totalSize;
    return out;
}

FileMap::~FileMap() {
    int i = 0;
    unordered_map<uint64_t, FileInfo*>::iterator it;
    for (it = map_.begin(); it != map_.end(); ++it ) {
        delete it->second;
    }
    map_.clear();
    //delete mu_;
}
void FileMap::clear() {
    MutexLock l(&mu_);
    int i = 0;
    unordered_map<uint64_t, FileInfo*>::iterator it;
    for (it = map_.begin(); it != map_.end(); ++it ) {
        delete it->second;
    }
    map_.clear();
}

char* FileMap::serialize(char* dest) {
    MutexLock l(&mu_);
    char* ptr;
    ptr = dest;
    uint32_t n = map_.size();
    ptr = PutVarint32InBuff(ptr, n);

    unordered_map<uint64_t, FileInfo*>::iterator it;
    for (it = map_.begin(); it != map_.end(); ++it ) {
        FileInfo* finfo = it->second;
        ptr = finfo->serialize(ptr);
    }
    return ptr;
}

void FileMap::serialize(string& dest) {
    MutexLock l(&mu_);
    uint32_t n = map_.size();
    PutVarint32(&dest, n);

    unordered_map<uint64_t, FileInfo*>::iterator it;
    for (it = map_.begin(); it != map_.end(); ++it ) {
        FileInfo* finfo = it->second;
        finfo->serialize(dest);
    }
}

bool FileMap::deserialize(Slice& src, Disk* disk) {
    MutexLock l(&mu_);
    uint32_t n = 0;
    if (!GetVarint32(&src, &n)) {
        return false;
    }
    for (int i = 0; i < n; ++i ){
        FileInfo* finfo = new FileInfo();
        if (!finfo->deserialize(src, disk)) {
            delete finfo;
            return false;
        }
        map_[finfo->getNumber()] = finfo;
    }
    return true;
}

void FileMap::getFiles(vector<string>* result) {
    MutexLock l(&mu_);
    unordered_map<uint64_t, FileInfo*>::iterator it;
    for (it = map_.begin(); it != map_.end(); ++it ) {
        result->push_back(it->second->getFileName());
    }
}

FileInfo* FileMap::pickFileInfoForDefrag(string& dbname) {
  MutexLock l(&mu_);
  int nDefragtableFiles = 0;
  int nNondefragtableFiles = 0;
  FileInfo* picked = NULL;
  uint16_t picked_count = 0;
  for (auto it = map_.begin(); it != map_.end(); it++) {
    FileInfo* fi = it->second;
    if (!fi->isDefragtable(dbname)) {
        nNondefragtableFiles++;
        continue;
    }
    if (obsoleteFiles_.count(fi->getNumber())) {
      continue;
    }
    if (fi->values().getDeleted() == fi->values().getTotal()) {
        obsoleteFiles_.insert(fi->getNumber());
        uint64_t n = fi->getNumber();
        continue;
    }
    nDefragtableFiles++;
    if (fi->values().getDeleted() > picked_count) {
      picked = fi;
      picked_count = fi->values().getDeleted();
    }
  }
  if (picked) {
      privateBlock(picked->getNumber());
  }

  return picked;
}

bool FileMap::isFragmented(string& dbname) {
    MutexLock l(&mu_);
    bool bFragmented = false;
    for (auto it = map_.begin(); !bFragmented && it != map_.end(); it++) {
        FileInfo* fi = it->second;
        if (fi->isDefragtable(dbname) && obsoleteFiles_.count(fi->getNumber()) == 0) {
            bFragmented = true;
        }
    }
    return bFragmented;
}
void FileMap::printInfo() {
    MutexLock l(&mu_);
    if (obsoleteFiles_.size()) {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": #obsoletes = " << obsoleteFiles_.size() << ", obsoletes: ";
        set<uint64_t>::iterator obsit = obsoleteFiles_.begin();
        for (; obsit != obsoleteFiles_.end(); ++obsit) {
            cout << *obsit << " ";
        }
        cout << endl;
   }
}

void FileMap::getInfo(string& sInfo) {
    MutexLock l(&mu_);
    stringstream ss;
    if (blockedByDefragment_ != -1) {
        ss << ">>>>> Blocked by value defragment: " << blockedByDefragment_ << " ";
    }
    if (obsoleteFiles_.size()) {
        ss << ">>>>> #Obsoletes = " << obsoleteFiles_.size() << ": ";
        set<uint64_t>::iterator obsit = obsoleteFiles_.begin();
        for (; obsit != obsoleteFiles_.end(); ++obsit) {
            ss << *obsit << " ";
        }
    }
    sInfo = ss.str();
}

}  // namespace smr
//} /* namespace smrdbdisk */
