#ifndef SMRDB_VALUEFILEMANAGER_H
#define SMRDB_VALUEFILEMANAGER_H

#include <map>
#include <mutex>
#include <list>
#include <memory>
#include <string>
#include "leveldb/env.h"

#include "util/mutexlock.h"

using namespace leveldb;

namespace smr {

// Keeps references to open value files. This allows growing value files to zone
// size (reducing the number of generated files) and improves sequential read
// performance.
// Obtaining a file guarantees unique access to the object (reused after released) as
// well as no interference from other file objects (no need to worry about concurrency)
class ValueFileCache
{
  friend class CacheManager;
public:
  // Obtain the a random access file object for the requested file number.
  // Guarantee: unique user of object and no concurrent writable object for the same file.
  Status getReadable(uint64_t file_number, std::shared_ptr<RandomAccessFile>& result);

  // Obtain a writable file object. Returned object may have any file number, hint is
  // only used in case no cached objects are available to generate a new file.
  // Guarantee: unique user of object, no concurrent writable or readable objects for the same file.
  Status getWritable(uint64_t file_number_hint, std::shared_ptr<WritableFile>& result);

  // The writable file has been closed by the client, it is no longer useful to keep around.
  void removeWritable(uint64_t file_number);
  void removeReadable(uint64_t file_number);

  bool isWritable(uint64_t fnumber) {
      MutexLock lock(&mu_);
      auto wit = wmap_.find(fnumber);
      return (wit != wmap_.end());
  }

  // Destructor. Closes all cached writable files.
  ~ValueFileCache();
private:
  // Private. Use CacheManager to obtain a cache instance.
  explicit ValueFileCache(const std::string& db_name);

private:
  // database name used to generate new files
  const std::string db_name_;
  // maximum number of cached readable files
  const int max_readable_;
  // map of cached writable files
  std::map<uint64_t, std::shared_ptr<WritableFile>> wmap_;
  // map of cached readable files
  std::multimap<uint64_t, std::shared_ptr<RandomAccessFile>> rmmap_;
  // remember access order of readable files so we can discard in fifo order
  std::list<uint64_t> rmmap_access_;
  // thread safety
//  std::mutex mutex_;
  port::Mutex mu_;
};

// Static cache manager to access individual caches. This is not strictly needed as we only have a single
// database instance in kineticd... simplify?
class CacheManager
{
public:
  // obtain a cache reference for supplied dbname.
  static ValueFileCache* cache(const std::string& db_name);
  // delete all caches
  static void clear();

private:
  static port::Mutex mu_;

//  static std::mutex mutex_;
  static std::map<std::string, std::unique_ptr<ValueFileCache>> caches_;
};


}

#endif //SMRDB_VALUEFILEMANAGER_H
