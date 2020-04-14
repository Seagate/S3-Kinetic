#ifndef SMRDB_VALUEBUILDER_H
#define SMRDB_VALUEBUILDER_H

#include "leveldb/env.h"
#include "table/block_builder.h"
#include "smrdisk/ValueFileSection.h"
#include "smrdisk/FileInfo.h"
#include <memory>
#include <map>
#include <stdint.h>

#include "util/mutexlock.h"

namespace smr {

// ValueFile = Section*
// Section   = data+, sst block, ExternalSectionDescriptor
class ValueBuilder
{
public:
  // Constructor
  explicit ValueBuilder(const string& dbname, uint64_t sst_number, const Options& options);

  // Destructor
  ~ValueBuilder();

  // Directly appends value->data to a value file.
  // Returned result contains a serialized description of where the value has been stored.
  // It is only valid until the next call to Append
  Status Append(const Slice& key, const LevelDBData* value, ExternalValueInfo*& result);

  // Flushes in-memory metadata and sync value file
  Status Finish();

  // Marks any put values as deleted
  void Abandon();
  Status ObtainWritableFile();

private:
  // Flushes in-memory metadata
  Status Flush();

  // Attempts to obtain a writable file from ValueFileCache. If not successful, attempts to
  // create a new writable file.

private:
  // The database name
  const string& dbname_;
  // The sst number associated with the builder. Can be used to generate a unique value file
  const uint64_t sst_number_;
  // The options
  const Options& options_;
  // The descriptor for the current section
  SectionDescriptor section_;
  // The block builder used to build metadata for the current section
  BlockBuilder keyinfo_;
  // The number of keys added to the current section
  uint16_t num_added_;
  // The current writable file
  std::shared_ptr<WritableFile> file_;
  // Pointer to the file info structure associated with the current writable file
  FileInfo* fileInfo_;
  // The value info structure will be updated for each append operation
  ExternalValueInfo external_;
  port::Mutex mu_;
};

}
#endif //SMRDB_VALUEBUILDER_H
