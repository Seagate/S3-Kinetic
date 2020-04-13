// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// TableBuilder provides the interface used to build a Table
// (an immutable and sorted map from keys to values).
//
// Multiple threads can invoke const methods on a TableBuilder without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same TableBuilder must use
// external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_USTABLE_BUILDER_H_
#define STORAGE_LEVELDB_INCLUDE_USTABLE_BUILDER_H_

#include <stdint.h>
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "db/dbformat.h"

namespace leveldb {

class BlockBuilder;
class BlockHandle;
class WritableFile;
class RandomAccessFile;

class USTableBuilder {
 public:
  // Create a builder that will store the contents of the table it is
  // building in *file.  Does not close the file.  It is up to the
  // caller to close the file after calling Finish().
  explicit USTableBuilder(const Options& options, WritableFile* file, RandomAccessFile* rfile, const InternalKeyComparator& cmp);

  void Ref() { ++refs_;}

  void Unref() {
      --refs_;
      assert(refs_ >= 0);
      if (refs_ <= 0) {
          delete this;
      }
  }

  // Add key,value to the table being constructed.
  // REQUIRES: key is after any previously added key according to comparator.
  // REQUIRES: Finish(), Abandon() have not been called
  void Add(SequenceNumber s, ValueType type, const Slice& key, const Slice& value);

  // Return non-ok iff some error has been detected.
  Status status() const;

  Status Sync();
  // Finish building the table.  Stops using the file passed to the
  // constructor after this function returns.
  // REQUIRES: Finish(), Abandon() have not been called
  Status Finish();

  // Indicate that the contents of this builder should be abandoned.  Stops
  // using the file passed to the constructor after this function returns.
  // If the caller is not going to call Finish(), it must call Abandon()
  // before destroying this builder.
  // REQUIRES: Finish(), Abandon() have not been called
  void Abandon();

  // Size of the file generated so far.  If invoked after a successful
  // Finish() call, returns the size of the final generated file.
  uint64_t FileSizeEstimate() const;

  uint64_t FileSize() const;

  InternalKey SmallestKey() const;

  InternalKey LargestKey() const;

  bool Get(const LookupKey& key, std::string* value, Status* s);

  Iterator* NewIterator();

 private:
  // REQUIRES: Either Finish() or Abandon() has been called.
  ~USTableBuilder();

  bool ok() const { return status().ok(); }
  void WriteBlock(BlockBuilder* block, BlockHandle* handle);
  void WriteRawBlock(const Slice& data, BlockHandle* handle);

  struct Rep;
  Rep* rep_;

  int refs_;

  // No copying allowed
  USTableBuilder(const USTableBuilder&);
  void operator=(const USTableBuilder&);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_USTABLE_BUILDER_H_
