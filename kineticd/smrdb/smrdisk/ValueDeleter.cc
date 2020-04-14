#include "ValueDeleter.h"
#include "db/version_edit.h"
#include <iostream>

#include "ValueFileCache.h"

using namespace std;

namespace smr {

ValueDeleter::ValueDeleter(Logger* logger, const string& dbName, Disk* disk) :
    ExternalValueDeleter(), logger_(logger), disk_(disk), dbName_(dbName)
{
}

ValueDeleter::~ValueDeleter()
{
}

void ValueDeleter::Add(const char* descriptor)
{
  MutexLock lock(&mu_);
  ExternalValueInfo external;
  if (external.deserialize(descriptor)) {
    if (deletions_.count(external.file_number) == 0) {
       deletions_[external.file_number] = 0;
    }
    deletions_[external.file_number]++;
  } else {
    Status::InvalidArgument("Provided descriptor is not a valid ExternalValueInfo structure.");
  }
}

Status ValueDeleter::Finalize(VersionEdit* versionEdit)
{
  MutexLock lock(&mu_);
  for (auto file_it = deletions_.begin(); file_it != deletions_.end(); file_it++) {

    FileInfo* file_info = disk_->getFileInfo(file_it->first, kValueFile);
    if (!file_info) {
      Log(logger_, 5, "No file info for value file %lu available. File may have been deallocated "
                      "by defragmentation. Skipping %d scheduled deletes.", file_it->first, file_it->second);
      continue;
    }

    file_info->values().incrDeleted(file_it->second);
    file_info->getLevel()->disk()->updateFileValueInfo(file_info);

    bool file_is_in_writing = file_is_in_writing = file_info->getLevel()->isZoneInWriting(file_info->getLastSegment()->getZone());
    if (file_is_in_writing || CacheManager::cache(dbName_)->isWritable(file_it->first)) {
      Log(logger_, 5, "Skipping deallocation of file %lu. It has not been closed.", file_it->first);
      continue;
    }

    Log(logger_, 5, "Marked %d records deleted in value file %lu. Total records in file: %d, Records deleted: %d",
                file_it->second, file_it->first, file_info->values().getTotal(), file_info->values().getDeleted());

    if (file_info->values().getDeleted() == file_info->values().getTotal()) {
        versionEdit->DeleteValueFile(file_it->first);
        disk_->obsoleteValueFile(file_it->first);
    } else {
        if (file_info->values().getDeleted() > file_info->values().getTotal()) {
            Status::IOError("Deleteds > total");  // This line for debugging.
        }
    }
  }
  return Status::OK();
}

}
