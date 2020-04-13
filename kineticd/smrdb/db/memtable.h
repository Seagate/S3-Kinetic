// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>
#include "leveldb/db.h"
#include "leveldb/status.h"
#include "db/dbformat.h"
#include "db/skiplist.h"
#include "util/arena.h"
#include "mem/KineticMemory.h"
#include "mem/DynamicMemory.h"
#include <iostream>
using namespace std;

namespace leveldb {

class InternalKeyComparator;
class Mutex;
class MemTableIterator;

class MemTable {
 public:
  // MemTables are reference counted.  The initial reference count
  // is zero and the caller must call Ref() at least once.
  explicit MemTable(const InternalKeyComparator& comparator);
  int GetRef() {
      return refs_;
  }
  // Increase reference count.
  void Ref() { ++refs_; }

  // Drop reference count.  Delete if no more references exist.
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ == 0) {
      delete this;
    }
  }
  bool IsEmpty() const {
      return (ApproximateMemoryUsage() == 0);
  }

  // Returns an estimate of the number of bytes of data in use by this
  // data structure.
  //
  // REQUIRES: external synchronization to prevent simultaneous
  // operations on the same MemTable.
  size_t ApproximateMemoryUsage() const;

  // Returns an estimate of the size of an L0 sst table generated from
  // this mem table.
  //
  // REQUIRES: external synchronization to prevent simultaneous
  // operations on the same MemTable.
  size_t ApproximateL0sstSize() const;

  // Return an iterator that yields the contents of the memtable.
  //
  // The caller must ensure that the underlying MemTable remains live
  // while the returned iterator is live.  The keys returned by this
  // iterator are internal keys encoded by AppendInternalKey in the
  // db/format.{h,cc} module.
  Iterator* NewIterator();

  // Add an entry into memtable that maps key to value at the
  // specified sequence number and with the specified type.
  // Typically value will be empty if type==kTypeDeletion.
  void Add(SequenceNumber seq, ValueType type,
           const Slice& key,
           const Slice& value);

  // If memtable contains a value for key, store it in *value and return true.
  // If memtable contains a deletion for key, store a NotFound() error
  // in *status and return true.
  // Else, return false.
  bool Get(const LookupKey& key, char* value, Status* s, uint64_t* seqNum = NULL);

 private:
  ~MemTable() {
    assert(refs_ == 0);
    this->DeleteValuePointers();
  }
  Status DeleteValuePointers() {
    Status s;
    Iterator* iter = NewIterator();
    iter->SeekToFirst();
    smr::DynamicMemory* Memory = smr::DynamicMemory::getInstance();
    uint32_t clientSize=0;
    uint32_t defragSize=0;
    LevelDBData *myData;
    for (; iter->Valid(); iter->Next()) {
        Slice key = iter->key();
        Slice value_pointer = Slice(key.data() + key.size(), sizeof(void *));
        memcpy(&myData,value_pointer.data(), sizeof(void *));
        if (myData == NULL) {
          //  cout << " DATA = NULL " << endl;
          // This is the deleted key which has no value
          continue;
        } 
        if (myData->data) {
          //cout << " FREE " << (void*)myData->data << " SiZE " << myData->dataSize << endl;
            free(myData->data);
            if (myData->memType == MEMORYType::MEM_FOR_CLIENT) {
                clientSize += ROUNDUP(myData->dataSize, 4096);
            } else if (myData->memType == MEMORYType::MEM_FOR_DEFRAG) {
                defragSize += ROUNDUP(myData->dataSize, 4096);
            }
        }
	delete [] myData->header;
        delete myData;
    }
    if (clientSize > 0) {
        Memory->deallocate(clientSize);
    }
    if (defragSize > 0) {
        Memory->deallocate(defragSize, true);
    }
    delete iter;
    return s.OK();
  }

  struct KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) { }
    int operator()(const char* a, const char* b) const;
  };
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;

  typedef SkipList<const char*, KeyComparator> Table;

  KeyComparator comparator_;
  int refs_;
  Arena arena_;
  Table table_;
  uint32_t memSize_;
  uint32_t sstSize_;

  // No copying allowed
  MemTable(const MemTable&);
  void operator=(const MemTable&);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_MEMTABLE_H_
