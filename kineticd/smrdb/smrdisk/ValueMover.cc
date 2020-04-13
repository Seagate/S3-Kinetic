#include "ValueMover.h"
#include "leveldb/comparator.h"
#include "smrdisk/DriveEnv.h"
#include "smrdisk/SmrFile.h"
#include "db/dbformat.h"
#include "table/block.h"
#include "ValueFileCache.h"
#include "../mem/DynamicMemory.h"


namespace smr {

class ExternalComparator : public Comparator
{
private:
  /* Turn value into a valid LevelDBData MEM_INTERNAL structure for put operation */
  int FinishValue(LevelDBData* value, const LevelDBData* existing, const ExternalValueInfo& ext) const
  {
    // Allocate header buffer and copy header from existing LevelDBData structure
    value->headerSize = existing->headerSize; // Tri: TODO: They can be difference???
    value->header = new char[value->headerSize];
    if(!value->header) {
      status_ = Status::NoSpaceAvailable("Failed allocating memory for header buffer");
      return -1;
    }
    memcpy(value->header, existing->header, existing->headerSize);

    // Allocate data buffer and read in value from value file
    value->dataSize = ext.size;
    value->memType  = MEMORYType::MEM_FOR_DEFRAG;
    value->data = smr::DynamicMemory::getInstance()->allocate(value->dataSize, true);
    if(!value->data) {
      delete [] value->header;
      status_ = Status::NoSpaceAvailable("Failed allocating memory for data buffer");
      return -1;
    }
    Slice not_needed;
    status_ = file_->Read(ext.offset, ext.size, &not_needed, value->data);
    if(status_.ok()) {
      value->type = LevelDBDataType::MEM_INTERNAL;
      return 0;
    }
    delete [] value->header;
    free(value->data);
    smr::DynamicMemory::getInstance()->deallocate(value->dataSize, true);
    return -1;
  }

public:
  ExternalComparator(std::shared_ptr<RandomAccessFile>& file) : file_(file)
  {}

  virtual ~ExternalComparator() {
  }

  Status getStatus() const
  {
    return status_;
  }

  // If both supplied LevelDBData structures store the same external info structure ->
  //    allocate header and data buffers for new value and build valid MEM_INTERNAL
  // otherwise return nonzero
  int Compare(const Slice& fresh_value, const Slice& existing_value) const
  {
    LevelDBData* fresh = (LevelDBData*) fresh_value.data();
    LevelDBData* serializedExisting = (LevelDBData*) existing_value.data();

    if (!fresh || fresh->type != LevelDBDataType::MEM_EXTERNAL) {
      status_ = Status::InvalidArgument("Supplied new value is not MEM_EXTERNAL");
      return -1;
    }
    if (!serializedExisting || serializedExisting->type != LevelDBDataType::SERIALIZED_EXTERNAL) {
      // key has been deleted or current value stored for key is not external
      return -1;
    }
    LevelDBData getDBData;
    if (!getDBData.deserialize(existing_value.data())) {
      status_ = Status::Corruption("Existing value is corrupt.");
      return -1;
    }
    ExternalValueInfo fresh_external;
    if (!fresh_external.deserialize(fresh->data)) {
      status_ = Status::Corruption("New external value info is corrupt.");
      return -1;
    }
    return FinishValue(fresh, &getDBData, fresh_external);
  }

  const char* Name() const
  { return "ExternalComparator"; };

  void FindShortestSeparator(std::string* start, const Slice& limit) const
  {};

  void FindShortSuccessor(std::string* key) const
  {};

private:
  mutable Status status_;
  std::shared_ptr<RandomAccessFile>& file_;
};

ValueMover::ValueMover(const std::string& dbname, const Options& options, FileInfo* fileInfo, Env::put_func_t put_func)
    : dbname_(dbname), options_(options), fileInfo_(fileInfo), putFunc_(put_func)
{}


Status ValueMover::GetSectionDescriptors(std::shared_ptr<RandomAccessFile>& file, std::list<SectionDescriptor>& sections)
{
  SectionDescriptor section;
  section.prev_end_offset = fileInfo_->getSize();

  char buffer[sizeof(SectionDescriptor)];
  bool recovery = false;

  while(section.prev_end_offset > 0) {
    uint32_t read_offset = section.prev_end_offset - sizeof(SectionDescriptor);
    if(!recovery) {
      Log(options_.info_log, 5, "Reading in section from offset: %u", read_offset);
    }
    Slice not_needed;
    Status s = file->Read(read_offset, sizeof(SectionDescriptor), &not_needed, buffer);
    if (!s.ok()) {
      return s;
    }
    if(section.deserialize(buffer) && section.prev_end_offset < read_offset) {
      sections.push_front(section);
      recovery = false;
    }
    else if (sections.empty()){
      if(!recovery) {
        Log(options_.info_log, 0, "Failed deserializing section descriptor at offset: %lu. This may indicate "
            "a power loss during value-file write. Entering Recovery Mode: Searching for an existing valid "
            "section descriptor in value file.", section.prev_end_offset - sizeof(SectionDescriptor)
        );
        recovery = true;
      }
      // Check previous 4KB boundary in file for valid section descriptor
      section.prev_end_offset = ROUNDUP(read_offset - 4096, 4096);
    }
    else {
      return Status::Corruption("Value file is corrupt and not recoverable.");
    }
  }
  return Status::OK();
}

namespace {
Block* newSSTBlock(const SectionDescriptor& section, std::shared_ptr<RandomAccessFile>& file)
{
  BlockHandle blockHandle;
  blockHandle.set_offset(section.sst_offset);
  blockHandle.set_size(section.sst_size);

  ReadOptions readOptions;
  readOptions.verify_checksums = false;
  readOptions.fill_cache = false;

  BlockContents blockContents;
  ReadBlock(file.get(), readOptions, blockHandle, &blockContents);
  return new Block(blockContents);
}
};

Status ValueMover::Finish()
{
  std::shared_ptr<RandomAccessFile> file;

  Log(options_.info_log, 0, "Defragmenting file #%llu", (unsigned long long)fileInfo_->getNumber());
  Status s = CacheManager::cache(dbname_)->getReadable(fileInfo_->getNumber(), file);
  if (!s.ok()) {
    Log(options_.info_log, 0, "Defragmenting error: file #%llu, %s", (unsigned long long)fileInfo_->getNumber(), s.ToString().c_str());
    return s;
  }
  std::list<SectionDescriptor> sections;
  s = GetSectionDescriptors(file, sections);
  if (!s.ok()) {
    Log(options_.info_log, 0, "Defragmenting error: file #%llu, %s", (unsigned long long)fileInfo_->getNumber(), s.ToString().c_str());
    return s;
  }
  ExternalComparator comp(file);
  WriteOptions writeOptions;
  writeOptions.value_comparator = &comp;

  // Iterate over all sections
  for (auto secIt = sections.begin(); s.ok() && secIt != sections.end(); secIt++) {
    Block* block = newSSTBlock(*secIt, file);
    Iterator* iter = block->NewIterator(options_.comparator);
    iter->SeekToFirst();

    // Iterate over all keys in the current section
    for (; s.ok() && iter->Valid(); iter->Next()) {
      LevelDBData* value = new LevelDBData();
      if (!value->deserialize(iter->value().data()) || value->headerSize) {
        delete value;
        s = Status::Corruption("Corrupt value in value file.");
        break;
      }
      // conditional put for value into leveldb (success depending on matching external info
      // structures). header and data buffers will be allocated by comparator on success.
      s = putFunc_(writeOptions, iter->key(), Slice((char*) value, sizeof(char*)));
      if (!s.ok()) {
        if (value->type == LevelDBDataType::MEM_INTERNAL) {
	  free(value->data);
          smr::DynamicMemory::getInstance()->deallocate(value->dataSize, true);
          delete [] value->header;
        }
        delete value;

        // if value comparator failed, obtain status from comparator
        if (s.IsNotAttempted()) {
          s = comp.getStatus();
        }
      }
    }
    delete iter;
    delete block;
  }
    Log(options_.info_log, 0, "Defragmented file #%llu", (unsigned long long)fileInfo_->getNumber());
  return s;
}

}
