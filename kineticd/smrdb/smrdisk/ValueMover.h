#ifndef SMRDB_VALUEMOVER_H
#define SMRDB_VALUEMOVER_H

#include "leveldb/env.h"
#include "smrdisk/ValueFileSection.h"
#include "smrdisk/FileInfo.h"
#include <memory>
#include <list>

using namespace leveldb;

namespace smr {

class ValueMover
{
public:
  // leveldb::Put all values stored in the value file with the condition that
  // current external value info structures still refer to values
  Status Finish();

  // Constructor.
  explicit ValueMover(const std::string& dbname, const Options& options, FileInfo* fileInfo, Env::put_func_t put_func);

private:
  // Load all section descriptors from file.
  Status GetSectionDescriptors(std::shared_ptr<RandomAccessFile>& file, std::list<SectionDescriptor>& sections);

private:
  const std::string& dbname_;
  const Options& options_;
  FileInfo* fileInfo_;
  Env::put_func_t putFunc_;
};

}

#endif //SMRDB_VALUEMOVER_H
