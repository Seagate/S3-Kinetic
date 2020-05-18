// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// BlockBuilder generates blocks where keys are prefix-compressed:
//
// When we store a key, we drop the prefix shared with the previous
// string.  This helps reduce the space requirement significantly.
// Furthermore, once every K keys, we do not apply the prefix
// compression and store the entire key.  We call this a "restart
// point".  The tail end of the block stores the offsets of all of the
// restart points, and can be used to do a binary search when looking
// for a particular key.  Values are stored as-is (without compression)
// immediately following the corresponding key.
//
// An entry for a particular key-value pair has the form:
//     shared_bytes: varint32
//     unshared_bytes: varint32
//     value_length: varint32
//     key_delta: char[unshared_bytes]
//     value: char[value_length]
// shared_bytes == 0 for restart points.
//
// The trailer of the block has the form:
//     restarts: uint32[num_restarts]
//     num_restarts: uint32
// restarts[i] contains the offset within the block of the ith restart point.

#include "table/block_builder.h"
#include <algorithm>
#include <assert.h>
#include "leveldb/comparator.h"
#include "leveldb/table_builder.h"
#include "leveldb/status.h"
#include "leveldb/mydata.h"
#include "table/table_rep.h"
#include "util/coding.h"
#include <iostream>
#include <sys/time.h>
#include "mem/DynamicMemory.h"

using namespace std;

namespace leveldb {

BlockBuilder::BlockBuilder(const Options* options)
    : options_(options),
      buffer_(NULL),
      bufferSize_(0),
      restarts_(),
      counter_(0),
      finished_(false)
{
  assert(options->block_restart_interval >= 1);
  restarts_.push_back(0);       // First restart point is at offset 0
  buffer_ = NULL;
  int s = posix_memalign((void**)&buffer_, 4096, ALIGNED_MEM_SIZE);

  if (buffer_ == NULL || s != 0) {
     Log(options->info_log, 0, "CAN NOT ALLOCATE MEMORY IN BLOCKBUILDER");
     #ifdef KDEBUG
     cout << "Cannot allocate Kernel Mem in BLOCKBUILDER" << endl;
     abort();
     #endif // KDEBUG
  }
}

BlockBuilder::~BlockBuilder()
{
  if (buffer_) {
    free(buffer_);
  }
}

void BlockBuilder::Reset()
{
  bufferSize_ = 0;
  restarts_.clear();
  restarts_.push_back(0);       // First restart point is at offset 0
  counter_ = 0;
  finished_ = false;
  last_key_.clear();
}

size_t BlockBuilder::CurrentSizeEstimate() const
{
  return (bufferSize_ +                           // Size of data buffer and flushed data
          restarts_.size() * sizeof(uint32_t) +   // Restart array
          sizeof(uint32_t));                      // Restart array length
}

Slice BlockBuilder::Finish()
{
  // Append restart array
  std::string temp;
  for (size_t i = 0; i < restarts_.size(); i++) {
    PutFixed32(&temp, restarts_[i]);
  }
  PutFixed32(&temp, restarts_.size());
  memcpy((buffer_ + bufferSize_), temp.data(), temp.size());
  bufferSize_ += temp.size();

  finished_ = true;
  return Slice(buffer_, bufferSize_);
}

void BlockBuilder::AddMetadata(const Slice& key, size_t valueSize)
{
  assert(buffer_);
  assert(!finished_);
  assert(counter_ <= options_->block_restart_interval);
  Slice last_key_piece(last_key_);
  assert(bufferSize_ == 0 // No values yet?
         || options_->comparator->Compare(key, last_key_piece) > 0);

  size_t shared = 0;

  if (counter_ < options_->block_restart_interval) {
    // See how much sharing to do with previous string
    const size_t min_length = std::min(last_key_piece.size(), key.size());
    while ((shared < min_length) && (last_key_piece[shared] == key[shared])) {
      shared++;
    }
  } else {
    // Restart compression
    restarts_.push_back(bufferSize_);
    counter_ = 0;
  }
  const size_t non_shared = key.size() - shared;

  // serialized entry has the form <shared size><non_shared size><value_size><non_shared_key><value>
  // Add everything except value to buffer
  char* pbuffer = buffer_ + bufferSize_;
  pbuffer = PutVarint32InBuff(pbuffer, shared);
  pbuffer = PutVarint32InBuff(pbuffer, non_shared);
  pbuffer = PutVarint32InBuff(pbuffer, valueSize);
  memcpy(pbuffer, key.data() + shared, non_shared);
  pbuffer += non_shared;

  // Update buffer_size_
  bufferSize_ = pbuffer - buffer_;

  // Update state
  last_key_.resize(shared);
  last_key_.append(key.data() + shared, non_shared);
  assert(Slice(last_key_) == key);
  counter_++;
}

void BlockBuilder::Add(const Slice& key, const Slice& value)
{
  AddMetadata(key, (value.data() ? value.size() : 0));

  // Add <value> to buffer_
  if(value.data()) {
    memcpy(buffer_ + bufferSize_, value.data(), value.size());
    bufferSize_ += value.size();
  }
}

void BlockBuilder::Add(const Slice& key, const LevelDBData* value, const ExternalValueInfo* ext, bool skip_header)
{
  AddMetadata(key, value->computeSerializedSize(ext, false, skip_header));
  //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow: key: " << key.ToString() << endl;
  if (ext) {
  //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow db val = " << *value << ", exInfo: " << *ext << endl;
  } else {
  //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow db val = " << *value << endl;
  }

  // Serialize value (using external data if available) and add as <value> to buffer_
  char* pbuffer = value->serialize(buffer_ + bufferSize_, ext, false, skip_header);
  bufferSize_ = pbuffer - buffer_;
}


}  // namespace leveldb
