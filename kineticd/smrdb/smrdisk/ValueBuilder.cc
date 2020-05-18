#include "ValueBuilder.h"
#include "ValueFileCache.h"
#include "smrdisk/DriveEnv.h"
#include "smrdisk/SmrFile.h"

namespace smr {

ValueBuilder::ValueBuilder(const string& dbname, uint64_t sst_number, const Options& options)
    : dbname_(dbname), sst_number_(sst_number), options_(options), keyinfo_(&options), num_added_(0)
{
//  ObtainWritableFile();
}

ValueBuilder::~ValueBuilder()
{
    if(false) { //file_) {
      uint64_t nFileNumber = fileInfo_->getNumber();
      file_->Close();
      file_.reset();
      if (file_.use_count() > 0) {
      cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow >>>>>>>>>>>> : value file #" << nFileNumber << ", use count = " << file_.use_count() << endl;
      char c;
      cout << "Hit Enter: ";
      cin >> c;
      }
      CacheManager::cache(dbname_)->removeWritable(external_.file_number);
    }
}

Status ValueBuilder::ObtainWritableFile()
{
    MutexLock lock(&mu_);
  if(false) { //file_) {
    file_->Close();
    file_.reset();
    CacheManager::cache(dbname_)->removeWritable(external_.file_number);
    if(external_.file_number == sst_number_) {
        return Status::NoSpaceAvailable("Cannot obtain any more value files.");
    }
  }

  Status s = CacheManager::cache(dbname_)->getWritable(sst_number_, file_);
  if(!s.ok()) {
    return s;
  }

  fileInfo_ = dynamic_cast<smr::SmrFile*>(file_.get())->getFileInfo();
  if (!fileInfo_) {
    file_.reset();
    return Status::InvalidArgument("Writable file does not have file info structure");
  }

  external_.file_number = fileInfo_->getNumber();
  section_.prev_end_offset = file_->GetCurrentFileSize();
  return Status::OK();
}

Status ValueBuilder::Flush()
{
//    MutexLock lock(&mu_);

  if (!num_added_) {
    return Status::OK();
  }
  if (!file_) {
    return Status::IOError("Cannot flush metadata.");
  }

  // Finish metadata block builder and append section footer
  Slice sst = keyinfo_.Finish();
  section_.sst_offset = file_->GetCurrentFileSize();
  section_.sst_size = sst.size();
  const char* ptr = section_.append((char*) sst.data());

  // Append metadata + section footer to file
  Status s = file_->Append(Slice(sst.data(), ptr - sst.data()));

  if (s.ok()) {
      // Add number of values added to ValueInfo
      fileInfo_->values().incrTotal(num_added_);
      fileInfo_->getLevel()->disk()->updateFileValueInfo(fileInfo_);
      num_added_ = 0;
      Log(options_.info_log, 6, "Finished section of value file %llu. section_size=%llu, space left=%d, status=%s",
          (unsigned long long)fileInfo_->getNumber(), (unsigned long long)(file_->GetCurrentFileSize() - section_.prev_end_offset),
          fileInfo_->getLastSegment()->getSpaceLeft(), s.ToString().c_str()
      );
      section_.prev_end_offset = file_->GetCurrentFileSize();
      keyinfo_.Reset();
  }
  return s;
}

Status ValueBuilder::Finish()
{
    MutexLock lock(&mu_);
    Log(options_.info_log, 6, "Enter Finish");
  Status s = Flush();
  if (s.ok() && file_) {
    s = file_->Sync();
//    Closing files < 64 MB capacity to minimize zone switching
//    if(fileInfo_->getLastSegment()->getSpaceLeft() < 64*1024*1024) {
//      file_->Close();
//      file_.reset();
//      CacheManager::cache(dbname_)->removeWritable(external_.file_number);
//    }
  }
  Log(options_.info_log, 6, "Exit Finish");
  return s;
}

void ValueBuilder::Abandon()
{
  MutexLock lock(&mu_);
  Status::IOError("Abandon ValueBuilder");  // This line for debugging.
  uint16_t remember_added = num_added_;
  if(Flush().ok()) {
    fileInfo_->values().incrDeleted(remember_added);
    fileInfo_->getLevel()->disk()->updateFileValueInfo(fileInfo_);
  }
  else if(file_) {
    // value file metadata could not be flushed... ensure that the value file is not re-used
    CacheManager::cache(dbname_)->removeWritable(fileInfo_->getNumber());
    file_->Close();
  }
}

Status ValueBuilder::Append(const Slice& key, const LevelDBData* value, ExternalValueInfo*& result)
{
  MutexLock lock(&mu_);
  if (!file_) {
      Log(options_.info_log, 6, "Exit Append with error");
    return Status::NotAttempted("Value Builder could not obtain writable file");
  }
  Status s;
  if (keyinfo_.CurrentSizeEstimate() > 512000) {
    s = Flush();
    if (!s.ok()) {
        Log(options_.info_log, 6, "Exit Append with error");
      return s;
    }
  }

  /* If current file has insufficient capacity for data + sst + section metadata, finish section and obtain
   * new writable file. We do not have to worry about the maximum number of values exceeding uint16_t, as
   * a value file will never grow beyond zone size and uint16_t is even sufficient for 4KB values. */
  size_t avail_capacity = fileInfo_->getLastSegment()->getSpaceLeft();
  size_t req_capacity = 0;
  if(avail_capacity < 3000000) {
    // Only compute required capacity if availing capacity is getting low
    // Reserve an extra 32 bytes for incoming block builder metadata (keysize, restart, etc.)
    size_t metadata_size = keyinfo_.CurrentSizeEstimate() + key.size() + 32 + sizeof(SectionDescriptor)
                           + value->computeSerializedSize(&external_, true, true);
    req_capacity = ROUNDUP(metadata_size, 4096) + ROUNDUP(value->dataSize, 4096);
  }
  if (avail_capacity < req_capacity) {
      Log(options_.info_log, 6, "Calling Flush from Append");
    s = Flush();
    if (s.ok() && file_) {
        s = file_->Sync();
    }

    if (s.ok() && file_) {
      s = file_->Close();
      uint64_t fnumber = fileInfo_->getNumber();
      CacheManager::cache(dbname_)->removeWritable(fnumber);
    }

    if (s.ok()) {
        mu_.Unlock();
      s = ObtainWritableFile();
      mu_.Lock();
    }
    if (!s.ok()) {
      return s;
    }
  }

  // Append data to file
  size_t write_offset = file_->GetCurrentFileSize();
  s = file_->Append(Slice(value->data, value->dataSize));

  if (s.ok()) {
    num_added_++;

    // Set external info values and result pointer.
    external_.offset = write_offset;
    external_.size = value->dataSize;
    result = &external_;

    // Add key + external info to section metadata, do not store header (stored by TableBuilder)
    keyinfo_.Add(key, value, &external_, true);
  }
  return s;
}

}
