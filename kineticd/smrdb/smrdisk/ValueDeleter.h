#ifndef SMRDB_VALUEDELTER_H
#define SMRDB_VALUEDELTER_H

#include "leveldb/env.h"
#include "Disk.h"
#include <list>
#include <map>

#include "util/mutexlock.h"

using namespace leveldb;
namespace smr {

class ValueDeleter : public ExternalValueDeleter
{
public:
  // Add deletion. Nothing will actually be done until a call to Finalize()
  void Add(const char* descriptor);
  void Add(uint64_t fnumber) {
     MutexLock lock(&mu_);
     if (deletions_.count(fnumber) == 0) {
         //disk_->blockValueFile(fnumber);
         deletions_[fnumber] = 0;
     }
     deletions_[fnumber]++;
  }


  // Applies all added deletions to the corresponding file info structures.
  // If all records of a file are deleted, deallocate the file.
  // The super block will have to be flushed after a successful call to Finalize
  // in order to persist the changes.
  Status Finalize(VersionEdit* versionEdit);

  // Constructor
  explicit ValueDeleter(Logger* logger, const string& dbName, Disk* disk);
  // Destructor
  ~ValueDeleter();
  
private:
  Logger* logger_;
  Disk* disk_;
  string dbName_;
  port::Mutex mu_;
  std::map<uint64_t, uint16_t> deletions_;
};

}
#endif //SMRDB_VALUEDELTER_H
