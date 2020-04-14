/*
 * FileMap.h
 *
 *  Created on: Mar 18, 2015
 *      Author: tri
 */

#ifndef FILEMAP_H_
#define FILEMAP_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "leveldb/slice.h"

#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "FileInfo.h"
#include "util/coding.h"
#include <queue>
#include <set>

using namespace std;
using namespace leveldb;
using namespace leveldb::port;

//namespace leveldb {
namespace smr {

class Disk;

class FileMap {
    public:
        FileMap() {
            usedBytes_ = 0;
            blockedByDefragment_ = -1;
        }

        virtual ~FileMap();

        friend ostream& operator<<(ostream& out, FileMap& src);

        void getFiles(vector<string>* result);
        bool deserialize(Slice& src, Disk* disk);

        void serialize(string& dest);
        char* serialize(char* dest);

        void addFileInfo(uint64_t key, FileInfo* finfo) {
           MutexLock l(&mu_);
           map_[key] = finfo;
           usedBytes_ += finfo->getSize();
        }

        FileInfo* getFileInfo(uint64_t key, FileType type)  {
            MutexLock l(&mu_);
            unordered_map<uint64_t, FileInfo*>::iterator it = map_.find(key);
            if (it == map_.end() || it->second->getType() != type) {
                return NULL;
            }
            return it->second;
        }

        FileInfo* removeFileInfo(uint64_t key, FileType type) {
            MutexLock l(&mu_);
            unordered_map<uint64_t, FileInfo*>::iterator it = map_.find(key);
            if (it == map_.end() || it->second->getType() != type) {
                return NULL;
            }
            FileInfo* fInfo = it->second;
            map_.erase(key);
            usedBytes_ -= fInfo->getSize();
            return fInfo;
        }

        uint64_t getUsedBytes() {
            return usedBytes_;
        }
        bool isBlocked(uint64_t fnumber) {
            MutexLock l(&mu_);
            return (blockedByDefragment_ == fnumber);
        }
        void unblock(uint64_t fnumber) {
            MutexLock l(&mu_);
            privateUnblock(fnumber);
        }
        void obsoleteFile(uint64_t fnumber) {
            MutexLock l(&mu_);
            obsoleteFiles_.insert(fnumber);
        }
        void getObsoleteFiles(set<uint64_t>& obsoleteFiles) {
            MutexLock l(&mu_);
            for (set<uint64_t>::iterator it = obsoleteFiles_.begin(); it != obsoleteFiles_.end(); ++it) {
                obsoleteFiles.insert(*it);
            }
        }
        void removeObsoleteFile(uint64_t fnumber) {
            MutexLock l(&mu_);
            obsoleteFiles_.erase(fnumber);
        }
        // Iterate over file info structures and pick the (non-blocked) file info containing
        // the fewest valid values.
        FileInfo* pickFileInfoForDefrag(string& dbname);
        void printInfo();
        void getInfo(string& sInfo);
        void clear();
        bool isFragmented(string& dbname);

    private:
        void privateBlock(uint64_t fnumber) {
            blockedByDefragment_ = fnumber;
        }
        void privateUnblock(uint64_t fnumber) {
            if (blockedByDefragment_ == fnumber) {
                blockedByDefragment_ = -1;
            }
        }

    private:
        unordered_map<uint64_t, FileInfo*> map_;
        uint64_t usedBytes_;
        port::Mutex mu_;
        uint64_t blockedByDefragment_;
        set<uint64_t> obsoleteFiles_;
};

}  // namespace smr
//} /* namespace leveldb */

#endif /* FILEMAP_H_ */
