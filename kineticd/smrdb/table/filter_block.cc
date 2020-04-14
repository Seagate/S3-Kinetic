// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"
#include "smrdisk/Util.h"
#include "kernel_mem_mgr.h"
#include <iostream>
#include <sstream>
#include "leveldb/status.h"
#include "mem/DynamicMemory.h"

namespace leveldb {

// See doc/table_format.txt for an explanation of the filter block format.

// Generate new filter every 2KB of data
static const size_t kFilterBaseLg = 11;
static const size_t kFilterBase = 1 << kFilterBaseLg;

FilterBlockBuilder::~FilterBlockBuilder() {
    if (keys_) {
        free(keys_);
    }
}

FilterBlockBuilder::FilterBlockBuilder(const FilterPolicy* policy)
    : policy_(policy) {
  keys_ = NULL;
  int s = posix_memalign((void**)&keys_, 4096, ALIGNED_MEM_SIZE_1M);
  if (keys_ == NULL || s != 0) {
      stringstream ss;
      ss << __FILE__ << ":" << __LINE__ << ":" << __func__
         << ":CAN NOT ALLOCATE MEMORY IN FilterBlockBuilder";
         Status::IOError(ss.str());
      #ifdef KDEBUG
      abort();
      #endif // KDEBUG
  }

  keys_size_ = 0; 
}

void FilterBlockBuilder::StartBlock(uint64_t block_offset) {
  uint64_t filter_index = (block_offset / kFilterBase);
  assert(filter_index >= filter_offsets_.size());
  while (filter_index > filter_offsets_.size()) {
    GenerateFilter();
  }
}

void FilterBlockBuilder::AddKey(const Slice& key) {
  if(keys_size_ + key.size() > 1024*1024){
    GenerateFilter();
  }

  // Set pointer to approriate place
  char* buff = keys_ + keys_size_;
  start_.push_back(keys_size_);
  memcpy(buff, key.data(), key.size());
  keys_size_ += key.size();
}

Slice FilterBlockBuilder::Finish() {
  //  Filter block have many segments as needed and it is terminated with metadata segment 
  //  segment1+segment2 .....+end
  //  Each segment has:
  //   - Filter array bit map
  //   - size_t K_ (number of probes;
  //  Meta data segment: 
  //   - list of segment offsets.
  //   - Number of segment offsets.
  //   - Total size of all segments
  //   -  kFilterBasedLg
  if (!start_.empty()) {
    GenerateFilter();
  }

  // Append array of per-filter offsets
 filter_offsets_.push_back(result_.size());

  const uint32_t array_offset = result_.size();
  for (size_t i = 0; i < filter_offsets_.size(); i++) {
    PutFixed32(&result_, filter_offsets_[i]);
  }
  PutFixed32(&result_, filter_offsets_.size()); //save number of filter segments;
  PutFixed32(&result_, array_offset);
  result_.push_back(kFilterBaseLg);  // Save encoding parameter in result
  keys_size_ = 0;

  /* Use the page-aligned buffer to serve the result in case of direct I/O */
  result_.copy(keys_, result_.size());
/*  cout << " FILTER SIZE " << result_.size() << endl;
  for(int i=0; i<result_.size(); ++i) {
     printf(" %02x", keys_[i]);
     if(i%15 == 0) {
        cout << endl;
     }
  }
        cout << endl;
*/  
  return Slice(keys_, result_.size());
}

void FilterBlockBuilder::GenerateFilter() {
  const size_t num_keys = start_.size();
  if (num_keys == 0) {
    // Fast path if there are no keys for this filter
    filter_offsets_.push_back(result_.size());
    return;
  }

  // Make list of keys from flattened key structure
  start_.push_back(keys_size_);  // Simplify length computation
  tmp_keys_.resize(num_keys);
  for (size_t i = 0; i < num_keys; i++) {
    const char* base = keys_ + start_[i];
    size_t length = start_[i+1] - start_[i];
    tmp_keys_[i] = Slice(base, length);
  }

  // Generate filter for current set of keys and append to result_.
  filter_offsets_.push_back(result_.size()); // Not used currently, if we moved
  policy_->CreateFilter(&tmp_keys_[0], num_keys, &result_);
  tmp_keys_.clear();
  start_.clear();
  keys_size_ = 0;
}

FilterBlockReader::FilterBlockReader(const FilterPolicy* policy,
                                     const Slice& contents)
    : policy_(policy),
      data_(NULL),
      base_lg_(0) {
  size_t n = contents.size();
  if (n < 5) return;  // 1 byte for base_lg_ and 4 for start of offset array
  base_lg_ = contents[n-1];
  uint32_t last_word = DecodeFixed32(contents.data() + n - 5);
  nsegments_ = DecodeFixed32(contents.data() + n - 5 - 4);
  
  if (last_word > n - 5) return;
  data_ = contents.data();
  uint32_t offset;
  for (int i = 0; i < nsegments_; i++) {
    offset =  DecodeFixed32(contents.data() + n - 5 - 4 - (nsegments_-i)*4);
    offset_.push_back(offset);
  }
}

bool FilterBlockReader::KeyMayMatch(uint64_t block_offset, const Slice& key) {
  bool match = false;
  for(int i=0; i< (nsegments_ - 1); i++) {
     uint32_t start = offset_[i];
     uint32_t limit = offset_[i+1];
     if (start < limit) {
        Slice filter = Slice(data_ + start, limit - start);
        match =  policy_->KeyMayMatch(key, filter);
        if (match) {
          return true;
        }
     }else if (start == limit) {
      // Empty filters do not match any keys
        return false;
     }
  }
  return false;
}

}
