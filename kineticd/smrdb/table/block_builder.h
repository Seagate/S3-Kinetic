// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_

#include <vector>
#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb {

struct Options;

class BlockBuilder {
 public:
  // Constructor, allocates memory buffer
  explicit BlockBuilder(const Options* options);

  // Destructor, de-allocates memory buffer
  ~BlockBuilder();

  // Reset the contents as if the BlockBuilder was just constructed.
  void Reset();

  // Copies the value into the internal buffer
  // REQUIRES: Finish() has not been called since the last call to Reset().
  // REQUIRES: key is larger than any previously added key
  void Add(const Slice& key, const Slice& value);

  // Serializes the LevelDBData into the internal buffer. If external
  // structure is provided, it will be used for the serialization.
  // If skip_header flag is set, header will not be serialized.
  // REQUIRES: Finish() has not been called since the last call to Reset().
  // REQUIRES: key is larger than any previously added key
  void Add(const Slice& key, const LevelDBData* value, const ExternalValueInfo* ext, bool skip_header = false);

  // Finish building the block and return a slice that refers to the
  // block contents.  The returned slice will remain valid for the
  // lifetime of this builder or until Reset() is called.
  Slice Finish();

  // Returns an estimate of the current (uncompressed) size of the block
  // we are building.
  size_t CurrentSizeEstimate() const;

  // Return true if no entries have been added since the last Reset()
  bool empty() const {
    return !bufferSize_;
  }

 private:
  void AddMetadata(const Slice& key, size_t valueSize);

 private:
  const Options*        options_;
  char*                 buffer_;          // Use memory buffer
  size_t                bufferSize_;      // Currently used buffer size
  std::vector<uint32_t> restarts_;        // Restart points
  int                   counter_;         // Number of entries emitted since restart
  bool                  finished_;        // Has Finish() been called?
  std::string           last_key_;

  // No copying allowed
  BlockBuilder(const BlockBuilder&);
  void operator=(const BlockBuilder&);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
