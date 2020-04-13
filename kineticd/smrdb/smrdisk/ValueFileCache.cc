#include "ValueFileCache.h"
#include "smrdisk/DriveEnv.h"
#include "smrdisk/SmrWritableFile.h"
#include <unistd.h>

namespace smr {

ValueFileCache::ValueFileCache(const std::string& db_name) : db_name_(db_name), max_readable_(10)
{
}

ValueFileCache::~ValueFileCache()
{
  for (auto it = wmap_.begin(); it != wmap_.end(); it++) {
    it->second->Close();
    it->second.reset();
  }
  wmap_.clear();

  for (auto it = rmmap_.begin(); it != rmmap_.end(); it++) {
    it->second.reset();
  }
  rmmap_.clear();
  rmmap_access_.clear();
}

// some helper methods for handling multimap to keep code readable
namespace {

typedef std::multimap<uint64_t, std::shared_ptr<RandomAccessFile>> rmmap_t;

// return true if all pointers for the element are unique
bool unique_all(const rmmap_t& mmap, uint64_t element)
{
  for (auto it = mmap.find(element); it != mmap.end(); it++) {
    if (!it->second.unique()) {
      return false;
    }
  }
  return true;
}

// if a unique pointer for the element exist, sets result and returns true
bool get_unique(const rmmap_t& mmap, uint64_t element, std::shared_ptr<RandomAccessFile>& result)
{
  auto it = mmap.find(element);
  if (it != mmap.end()) {
      result = it->second;
      return true;
  }
  return false;

/*
  for (auto it = mmap.find(element); it != mmap.end(); it++) {
    if (it->second.unique()) {
      result = it->second;
      return true;
    }
  }
  return false;
*/
}

// discards the first entry in mmap defined by the supplied order that is unique and returns
void discard_one(rmmap_t& mmap, std::list<uint64_t>& list)
{
  for(auto lit = list.begin(); lit != list.end(); lit++) {
   for(auto mit = mmap.find(*lit); mit != mmap.end(); mit ++) {
      if(mit->second.unique()) {
        //delete mit->second.get();
        mit->second.reset();
        mmap.erase(mit);
        list.erase(lit);
        return;
      }
    }
  }
}
} // namespace

Status ValueFileCache::getReadable(uint64_t file_number, std::shared_ptr<RandomAccessFile>& result)
{
  MutexLock lock(&mu_);
  map<uint64_t, shared_ptr<WritableFile>>::iterator wit = wmap_.find(file_number);
  if (wit != wmap_.end()) {
      wit->second->Sync();
  }

  // ensure that there is no write object for the same file in use at this time
  // if there is, wait until it gets released
/*    while(wmap_.count(file_number) && !wmap_.at(file_number).unique()) {
      mu_.Unlock();
      usleep(1000);
      mu_.Lock();
    } */

  // if we have a cached readable object that is not in use, we're done
  if(get_unique(rmmap_, file_number, result)){
    return Status::OK();
  }

  // generate a new readable object
  RandomAccessFile* raw_ptr;
  std::string file_name = ValueFileName(db_name_, file_number);
  Status s = smr::DriveEnv::getInstance()->NewRandomAccessFile(file_name, &raw_ptr);
  if (s.ok()) {
    result.reset(raw_ptr);
    /*
    rmmap_.insert(std::make_pair(file_number, result));
    rmmap_access_.push_back(file_number);
    if(rmmap_.size() > max_readable_) {
      discard_one(rmmap_, rmmap_access_);
    }
    */
  }
  return s;
}

Status ValueFileCache::getWritable(uint64_t file_number_hint, std::shared_ptr<WritableFile>& result)
{
  MutexLock lock(&mu_);
  // if any writable objects are cached, and the associated file is not in use for
  // reading or writing, use cached object
  for (auto it = wmap_.begin(); it != wmap_.end(); it++) {
    if(!unique_all(rmmap_, it->first)) {
      continue;
    }
    if (it->second.unique()) {
      result = it->second;
      return Status::OK();
    }
  }

  // no cached object or all files that have cached writables are in use... generate a new file
  WritableFile* raw_ptr;
  std::string file_name = ValueFileName(db_name_, file_number_hint);
  Status s = smr::DriveEnv::getInstance()->NewWritableFile(file_name, &raw_ptr);
  if (s.ok()) {
    result.reset(raw_ptr);
    wmap_[file_number_hint] = result;
  }
  return s;
}

void ValueFileCache::removeReadable(uint64_t file_number)
{
  MutexLock lock(&mu_);
  auto it = rmmap_.find(file_number);
  if (it != rmmap_.end()) {
     it->second.reset();
     rmmap_.erase(file_number);
     rmmap_access_.remove(file_number);
  }
}
void ValueFileCache::removeWritable(uint64_t file_number)
{
  MutexLock lock(&mu_);
  auto it = wmap_.find(file_number);
  if (it != wmap_.end()) {
     SmrWritableFile* file = (SmrWritableFile*)it->second.get();
     file->getFileInfo()->setAsNotWritable();
     it->second.reset();
     wmap_.erase(file_number);
  }
}

// define static class members
port::Mutex CacheManager::mu_;


std::map<std::string, std::unique_ptr<ValueFileCache>> CacheManager::caches_;

ValueFileCache* CacheManager::cache(const std::string& db_name)
{
  //std::lock_guard<std::mutex> lock(mutex_);
  MutexLock lock(&mu_);
  if(!caches_.count(db_name)) {
    caches_[db_name].reset(new ValueFileCache(db_name));
  }
  assert(caches_.find(db_name) != caches_.end());
  return caches_.at(db_name).get();
}

void CacheManager::clear()
{
  MutexLock lock(&mu_);
  for (auto it = caches_.begin(); it != caches_.end(); ++it) {
      it->second.reset();
  }
  caches_.clear();
}


}
