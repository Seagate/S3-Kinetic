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

#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_

#include <stdint.h>
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "table/block_builder.h"
#include "table/filter_block.h"
#include "table/format.h"

namespace leveldb {

class BlockHandle;
class WritableFile;


struct Rep;

class TableBuilder {
 public:

  // Create a builder that will store the contents of the table it is
  // building in *file.  Does not close the file.  It is up to the
  // caller to close the file after calling Finish().
  TableBuilder(const Options& options, WritableFile *file);

  // REQUIRES: Either Finish() or Abandon() has been called.
  ~TableBuilder();

  // Change the options used by this builder.  Note: only some of the
  // option fields can be changed after construction.  If a field is
  // not allowed to change dynamically and its value in the structure
  // passed to the constructor is different from its value in the
  // structure passed to this method, this method will return an error
  // without changing any fields.
  Status ChangeOptions(const Options& options);

  // Add key,value to the table being constructed. If value is empty this is
  // a deletion entry.
  // REQUIRES: key is after any previously added key according to comparator.
  // REQUIRES: Finish(), Abandon() have not been called
  void Add(const Slice& key, const Slice& value);

  // Add key,value to the table being constructed.
  // If external value is provided it will be serialized instead of value->data.
  // Serialization is done directly into the buffer of the underlying BlockBuilder.
  // REQUIRES: key is after any previously added key according to comparator.
  // REQUIRES: Finish(), Abandon() have not been called
  void Add(const Slice& key, const LevelDBData* value, const ExternalValueInfo* external);

  // Advanced operation: flush any buffered key/value pairs to file.
  // Can be used to ensure that two adjacent entries never live in
  // the same data block.  Most clients should not need to use this method.
  // REQUIRES: Finish(), Abandon() have not been called
  void Flush();

  // Return non-ok iff some error has been detected.
  Status status() const;

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

  // Number of calls to Add() so far.
  uint64_t NumEntries() const;

  // Size of the file generated so far.  If invoked after a successful
  // Finish() call, returns the size of the final generated file.
  uint64_t FileSize() const;

private:
  bool ok() const  { return status().ok(); }

  // Write out a block that has been built by the supplied BlockBuilder but has not yet been finished().
  // The block handle will be updated with the correct offsets after the block has been written out.
  void WriteBlock(BlockBuilder* block, BlockHandle* handle);

  // Called by WriteBlock to do the actual IO
  void WriteRawBlock(const Slice& data, CompressionType, BlockHandle* handle);

  // Implementation for the various add options
  void InternalAdd(const Slice& key, const Slice* slice, const LevelDBData* value, const ExternalValueInfo* external);

  // The table-internal data members, placed in an extra struct
  Rep* rep_;

  // No copying allowed
  TableBuilder(const TableBuilder&);
  void operator=(const TableBuilder&);
};
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_
