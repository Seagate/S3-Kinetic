// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/memtable.h"

#include <sstream>

#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "util/coding.h"
#include "db/log_writer.h"
#include "smrdisk/Util.h"

#include <iostream>
using namespace std;
namespace leveldb {

static Slice GetLengthPrefixedSlice(const char* data) {
  uint32_t len;
  const char* p = data;
  p = GetVarint32Ptr(p, p + 5, &len);  // +5: we assume "p" is not corrupted
  return Slice(p, len);
}

MemTable::MemTable(const InternalKeyComparator& cmp)
    : comparator_(cmp),
      refs_(0),
      table_(comparator_, &arena_),
      memSize_(0),
      sstSize_(0) {
}

size_t MemTable::ApproximateMemoryUsage() const { return memSize_;}

size_t MemTable::ApproximateL0sstSize() const { return sstSize_;}

int MemTable::KeyComparator::operator()(const char* aptr, const char* bptr)
    const {
  // Internal keys are encoded as length-prefixed strings.
  Slice a = GetLengthPrefixedSlice(aptr);
  Slice b = GetLengthPrefixedSlice(bptr);
  return comparator.Compare(a, b);
}

// Encode a suitable internal key target for "target" and return it.
// Uses *scratch as scratch space, and the returned pointer will point
// into this scratch space.
static const char* EncodeKey(std::string* scratch, const Slice& target) {
  scratch->clear();
  PutVarint32(scratch, target.size());
  scratch->append(target.data(), target.size());
  return scratch->data();
}

class MemTableIterator: public Iterator {
 public:
  explicit MemTableIterator(MemTable::Table* table) : iter_(table) { }

  virtual bool Valid() const { return iter_.Valid(); }
  virtual void Seek(const Slice& k) { iter_.Seek(EncodeKey(&tmp_, k)); }
  virtual void SeekToFirst() { iter_.SeekToFirst(); }
  virtual void SeekToLast() { iter_.SeekToLast(); }
  virtual void Next() { iter_.Next(); }
  virtual void Prev() { iter_.Prev(); }
  virtual Slice key() const { return GetLengthPrefixedSlice(iter_.key()); }
  virtual Slice value() const {
    Slice key_slice = GetLengthPrefixedSlice(iter_.key());
    char *value;
    memcpy(&value, key_slice.data() + key_slice.size(), sizeof(void *));
    return Slice(value, sizeof(LevelDBData));
  }

  virtual Status status() const { return Status::OK(); }

 private:
  MemTable::Table::Iterator iter_;
  std::string tmp_;       // For passing to EncodeKey

  // No copying allowed
  MemTableIterator(const MemTableIterator&);
  void operator=(const MemTableIterator&);
};

Iterator* MemTable::NewIterator() {
  return new MemTableIterator(&table_);
}

void MemTable::Add(SequenceNumber s, ValueType type,
                   const Slice& key,
                   const Slice& value) {
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  LevelDBData *myData = NULL;
  uint32_t internalValueSSTSize = 0;
  if (type != kTypeDeletion) {
    myData = (LevelDBData *) value.data();
    // assume page aligned memory buffers for mem size computation
    memSize_ += sizeof(LevelDBData) + ROUNDUP(myData->headerSize, 4096) + ROUNDUP(myData->dataSize, 4096);
    //memSize_ += sizeof(LevelDBData) + myData->headerSize + ROUNDUP(myData->dataSize, 4096);
    if(myData->dataSize < 8192) { // TODO(Paul): use configured value_size_threshhold
      internalValueSSTSize = ROUNDUP(myData->computeSerializedSize(), 4096);
    }
    else{
      internalValueSSTSize = ROUNDUP(myData->computeSerializedSize(NULL, true), 4096) + sizeof(ExternalValueInfo);
    }
  } else {
    memSize_ += 4096; //myData->headerSize; //4096;
  }
  memSize_ += key_size;
  sstSize_ += key_size + internalValueSSTSize;

  size_t internal_key_size = key_size + 8;
  const size_t encoded_len =
      VarintLength(internal_key_size) + internal_key_size +
             sizeof(void*); // + size of poinster to internal value
  char* buf = arena_.Allocate(encoded_len);
  char* p = EncodeVarint32(buf, internal_key_size);
  memcpy(p, key.data(), key_size);
  p += key_size;
  EncodeFixed64(p, (s << 8) | type);
  p += 8;
  memcpy(p, &myData, sizeof(void*));
  table_.Insert(buf);
}

bool MemTable::Get(const LookupKey& key, char* value, Status* s, uint64_t* seqNum) {
  Slice memkey = key.memtable_key();
  Table::Iterator iter(&table_);
  iter.Seek(memkey.data());
  if (iter.Valid()) {
    // entry format is:
    //    klength  varint32
    //    userkey  char[klength]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter.key();
    uint32_t key_length;
    const char* key_ptr = GetVarint32Ptr(entry, entry+5, &key_length);
    if (comparator_.comparator.user_comparator()->Compare(
            Slice(key_ptr, key_length - 8),
            key.user_key()) == 0) {
      // Correct user key
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      if (seqNum) {
          *seqNum = (tag >> 8);
      }
      switch (static_cast<ValueType>(tag & 0xff)) {
        case kTypeValue: {
	  LevelDBData *myData;
	  memcpy(&myData, key_ptr + key_length, sizeof(void*));

          //Pass internalValue pointer to user.
          memcpy(value, (char*)&myData, sizeof(void*));
          return true;
        }
        case kTypeDeletion: {
            stringstream context;
            context << __FILE__ << ":" << __LINE__ << ":" << __func__
                    << ": " << key_length << ": " << key.user_key().ToString();
            *s = Status::NotFound(context.str());
            return true;
        }
      }
    }
  }
  return false;
}

}  // namespace leveldb
