// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// WriteBatch::rep_ :=
//    sequence: fixed64
//    count: fixed32
//    data: record[count]
// record :=
//    kTypeValue varstring varstring         |
//    kTypeDeletion varstring
// varstring :=
//    len: varint32
//    data: uint8[len]

#include "leveldb/write_batch.h"

#include <iostream>

#include "leveldb/db.h"
#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "util/coding.h"
#include "kernel_mem_mgr.h"

using namespace std;

namespace leveldb {

// WriteBatch header has an 8-byte sequence number followed by a 4-byte count.
static const size_t kHeader = 12;

WriteBatch::WriteBatch() {
  Clear();
}

WriteBatch::~WriteBatch() {}

WriteBatch::Handler::~Handler() { }

void WriteBatch::Clear() {
  rep_.count = 0;
  rep_.sequence = 0;
}


Status WriteBatch::Iterate(Handler* handler)  {
  //Deserialize data from Disk
  Slice input(rrep_);
  Status s;
  if (input.size() < kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }
  char *ptr;
  ptr = (char*)rrep_.data();
  //Get sequence
  memcpy((char*)(&(rep_.sequence)), ptr, sizeof(rep_.sequence));
  ptr += sizeof(rep_.sequence);
  //Count
  memcpy((char*)(&(rep_.count)), ptr, sizeof(rep_.count));
  ptr += sizeof(rep_.count);

  int count;
  count = rep_.count;
  kvData *kvdata;

  while (count > 0) {
    kvdata = new kvData;
    //kType
    memcpy((char*)(&(kvdata->kType)), ptr, sizeof(kvdata->kType));
    ptr += sizeof(kvdata->kType);
    //key size
    memcpy((char*)(&(kvdata->keySize)), ptr, sizeof(kvdata->keySize));
    ptr += sizeof(kvdata->keySize);
    //key
    kvdata->key = new char[kvdata->keySize];
    memcpy(kvdata->key, ptr, kvdata->keySize);
    ptr += kvdata->keySize;
    if (kvdata->kType == kTypeValue) {
      kvdata->value = new LevelDBData();
      if (!kvdata->value->deserialize(ptr)) {
        // Todo: should this error be swallowed in order not to abort the whole batch?
        return Status::Corruption("Corrupt Value.");
      }

      // Copy header into new buffer memory and assign it to LevelDBData structure
      char* buf = new char[kvdata->value->headerSize];
      memcpy(buf, kvdata->value->header, kvdata->value->headerSize);
      kvdata->value->header = buf;
      kvdata->value->memType = MEMORYType::MEM_FOR_CLIENT;

      // Copy value into new buffer memory and assign it to LevelDBData structure
      buf = smr::DynamicMemory::getInstance()->allocate(kvdata->value->dataSize);
      if (buf == NULL) {
        return Status::NoSpaceAvailable("No space");
      }
      memcpy(buf, kvdata->value->data, kvdata->value->dataSize);
      kvdata->value->data = buf;
      ptr += kvdata->value->dataSize;
    } else {
      kvdata->value = NULL;
    } // end of kTypeValue

    count--;
    rep_.kvrecord.push_back(kvdata);
  }
  s = MyIterate(handler, true);
  return s;
}



Status WriteBatch::MyIterate(Handler* handler, bool recover) {

  Slice key, value;
  int found = 0;
  int count = rep_.count;
  kvData* kvdata;

  while (!rep_.kvrecord.empty()) {
    found++;
    count--;
    kvdata = rep_.kvrecord.front();
    rep_.kvrecord.pop_front();
    size_t valueSize = 0;
    switch(kvdata->kType) {
      case kTypeValue:
          handler->Put(Slice(kvdata->key, kvdata->keySize),
	               Slice((char *)kvdata->value, sizeof(kvdata->value)));
        break;
      case kTypeDeletion:
          handler->Delete(Slice(kvdata->key, kvdata->keySize));
        break;
      default:
        return Status::Corruption("unknown WriteBatch tag");
    }
    if (recover) {
        delete[] kvdata->key;
    }
//    cout << "DELETE KVDATA IN MYITERATE " << (void*)kvdata << endl;
    delete kvdata;
  }

  if (found != WriteBatchInternal::Count(this)) {
    return Status::Corruption("WriteBatch has wrong count");
  } else {
    return Status::OK();
  }
}

int WriteBatchInternal::Count(const WriteBatch* b) {
//  return DecodeFixed32(b->rep_.data() + 8);
  return (b->rep_.count);

}

void WriteBatchInternal::SetCount(WriteBatch* b, int n) {
//  EncodeFixed32(&b->rep_[8], n);
  b->rep_.count = n;
}

SequenceNumber WriteBatchInternal::Sequence(const WriteBatch* b) {
//  return SequenceNumber(DecodeFixed64(b->rep_.data()));
    return (b->rep_.sequence);
}

void WriteBatchInternal::SetSequence(WriteBatch* b, SequenceNumber seq) {
//  EncodeFixed64(&b->rep_[0], seq);
    b->rep_.sequence = seq;
}

void WriteBatch::Put(const Slice& key, const Slice& value) {
    WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
    kvData *kvdata = new kvData;

    kvdata->kType   = kTypeValue;
    kvdata->keySize = key.size();
    kvdata->key     = (char*)key.data();
    kvdata->value   = (LevelDBData*)value.data();;
    rep_.kvrecord.push_back(kvdata);
}

void WriteBatch::Delete(const Slice& key) {
    WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
    kvData *bdata;
    bdata = new kvData;

    bdata->kType = kTypeDeletion;
    bdata->keySize = key.size();
    bdata->key     = (char*)key.data();
    bdata->value   = NULL;
    rep_.kvrecord.push_back(bdata);

}

namespace {
class MemTableInserter : public WriteBatch::Handler {
 public:
  SequenceNumber sequence_;
  MemTable* mem_;

  virtual void Put(const Slice& key, const Slice& value) {
     mem_->Add(sequence_, kTypeValue, key, value);
    sequence_++;
  }
  virtual void Delete(const Slice& key) {
    mem_->Add(sequence_, kTypeDeletion, key, Slice());
    sequence_++;
  }
};
}  // namespace

Status WriteBatchInternal::MyInsertInto(WriteBatch* b,
                                      MemTable* memtable) {
  MemTableInserter inserter;
  inserter.sequence_ = WriteBatchInternal::Sequence(b);
  inserter.mem_ = memtable;
  return b->MyIterate(&inserter);
}

Status WriteBatchInternal::InsertInto(WriteBatch* b,
                                      MemTable* memtable) {
  MemTableInserter inserter;
//  inserter.sequence_ = WriteBatchInternal::Sequence(b);
  char *ptr;
  ptr = (char*)(b->rrep_.data());
  //Get sequence
  memcpy((char*)(&(inserter.sequence_)), ptr, sizeof(inserter.sequence_));

  inserter.mem_ = memtable;
  return b->Iterate(&inserter);
}

void WriteBatchInternal::SetContents(WriteBatch* b, const Slice& contents) {
  assert(contents.size() >= kHeader);
  b->rrep_.assign(contents.data(), contents.size());
}

void WriteBatchInternal::Append(WriteBatch* dst, WriteBatch* src) {
  int count = Count(src);

  SetCount(dst, Count(dst) + count);
  kvData *kvdata=NULL;
  while (!src->rep_.kvrecord.empty()) {
    count--;
    kvdata = src->rep_.kvrecord.front();
    src->rep_.kvrecord.pop_front();
    dst->rep_.kvrecord.push_back(kvdata);
  }
}

}  // namespace leveldb
