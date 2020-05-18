// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/log_writer.h"
#include <stdint.h>
#include <sys/time.h>
#include <iostream>
#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "leveldb/mydata.h"
#include "smrdisk/DriveEnv.h"

using namespace std;

namespace leveldb {
namespace log {

void BinToCharStr(unsigned char * bin, char *str) {
  for(int i = 0; i<8; ++i)
  {
    sprintf(&str[i*2], "%02X", bin[7-i]);
  }
  str[16]=0;
}

Writer::Writer(WritableFile* dest, Env* env, const std::string& dbname)
    : dest_(dest),
      block_offset_(0),
      env_(env),
      dbname_(dbname) {
  for (int i = 0; i <= kMaxRecordType; i++) {
    char t = static_cast<char>(i);
    type_crc_[i] = crc32c::Value(&t, 1);
  }
  buffer_ = NULL;
  int s = posix_memalign((void**)&buffer_, 4096, ALIGNED_MEM_SIZE);
  if (buffer_ == NULL || s != 0) {
#ifdef KDEBUG
     cout << " CAN NOT ALLOCATE MEMORY IN LOG WRITER " << endl;
     abort();
#endif
  }
}

Writer::~Writer()
{
  if(buffer_) {
//  cout << "LOG WRITER DEALLOC" << endl;
//    smr::AlignedMemory::getInstance()->deallocate(buffer_);
    free(buffer_);
  }
}


void Writer::Clear(const Slice slice) {
/*
  BatchRecord *ptr = slice.mydata();
  struct kvData *kvdata;
  deque<struct kvData *>::iterator itr;
  for (itr=ptr->kvrecord.begin();
       itr!=ptr->kvrecord.end(); ++itr) {
    kvdata = *itr;
    delete[] kvdata->key;
    delete kvdata;
  }
*/
}

Status Writer::MyAddRecord(const Slice slice) {
  // Serialize Batch Record for writing to Disk
  // Value bit 0
  // Version bit 1;
  // Tag bit 2;
  // Algorithm bit 3;
  //Order on Disk before going to Disk:
  //  Sequence:count:kType{kTypeValue, kTypeDeletion}:key:LevelDBData
  //  LevelDBData =  (Options[Version, Tag, Algo]:ValuseSize:Value)

  Status s;
  uint32_t totalSize = 0;
  BatchRecord* ptr = slice.mydata();
  struct kvData* kvdata;
  deque<struct kvData*>::iterator itr;

  if (buffer_ == NULL ) {
      s = Status::IOError("CAN NOT ALLOC MEM ","IN MYADDREC");
      return s;
  }

  // Header: 9 bytes total [CRC:4|Length:4|BlockType:1]. CRC + length sections are just reserved for now.
  buffer_[8] = kFullType;

  char* sequence_start = buffer_ + 9;
  char* pbuffer = sequence_start;
  uint64_t sequence = ptr->sequence;
  uint32_t count = ptr->count;

  for (itr = ptr->kvrecord.begin(); itr != ptr->kvrecord.end(); ++itr) {
    memcpy(pbuffer, &sequence, sizeof(sequence));
    pbuffer += sizeof(sequence);
    uint32_t recCount = 1;
    memcpy(pbuffer, &recCount, sizeof(ptr->count));
    pbuffer += sizeof(ptr->count);
    sequence++;
    kvdata = *itr;

    // kType
    memcpy(pbuffer, &(kvdata->kType), sizeof(kvdata->kType));
    pbuffer += sizeof(kvdata->kType);

    // KeySize
    memcpy(pbuffer, &(kvdata->keySize), sizeof(kvdata->keySize)); //Need to change to VARINT32
    pbuffer += sizeof(kvdata->keySize);

    // Key
    memcpy(pbuffer, kvdata->key, kvdata->keySize);
    pbuffer += kvdata->keySize;

    uint32_t size = pbuffer - sequence_start;

    if (kvdata->kType == kTypeValue) {
      pbuffer = kvdata->value->serialize(pbuffer, NULL, true);
      size = pbuffer - sequence_start + kvdata->value->dataSize;
    }

    buffer_[4] = size & 0xff;
    buffer_[5] = (size >> 8) & 0xff;
    buffer_[6] = (size >> 16) & 0xff;
    buffer_[7] = (size >> 24) & 0xff;

    s = dest_->Append(Slice(buffer_, pbuffer - buffer_));
    if (kvdata->kType == kTypeValue && s.ok()) {
      s = dest_->Append(Slice(kvdata->value->data, kvdata->value->dataSize));
    }
    pbuffer = sequence_start;
  }
  return s;
}

Status Writer::AddRecord(const Slice& slice) {
  const char* ptr = slice.data();
  size_t left = slice.size();

  // Fragment the record if necessary and emit it.  Note that if slice
  // is empty, we still want to iterate once to emit a single
  // zero-length record
  Status s;
  bool begin = true;
  do {
    const int leftover = kBlockSize - block_offset_;
    assert(leftover >= 0);
    if (leftover < kHeaderSize) {
      // Switch to a new block
      if (leftover > 0) {
        // Fill the trailer (literal below relies on kHeaderSize being 9)
        assert(kHeaderSize == 9);
        s = dest_->Append(Slice("\x00\x00\x00\x00\x00\x00\x00\x00", leftover));
        if (!s.ok()) {
            break;
        }
      }
      block_offset_ = 0;
    }

    // Invariant: we never leave < kHeaderSize bytes in a block.
    assert(kBlockSize - block_offset_ - kHeaderSize >= 0);

    const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
    const size_t fragment_length = (left < avail) ? left : avail;

    RecordType type;
    const bool end = (left == fragment_length);
    if (begin && end) {
      type = kFullType;
    } else if (begin) {
      type = kFirstType;
    } else if (end) {
      type = kLastType;
    } else {
      type = kMiddleType;
    }

    s = EmitPhysicalRecord(type, ptr, fragment_length);
    ptr += fragment_length;
    left -= fragment_length;
    begin = false;
  } while (s.ok() && left > 0);
  return s;
}

Status Writer::EmitPhysicalRecord(RecordType t, const char* ptr, size_t n) {
//  assert(n <= 0xffff);  // Must fit in two bytes
  assert(block_offset_ + kHeaderSize + n <= kBlockSize);

  // Format the header
  char buf[kHeaderSize];
  buf[4] = static_cast<char>(n & 0xff);
  buf[5] = static_cast<char>(n >> 8);
  buf[6] = static_cast<char>(n >> 16);
  buf[7] = static_cast<char>(n >> 24);
//  buf[6] = static_cast<char>(t);
  buf[8] = static_cast<char>(t);
//#ifdef KDEBUG
//  uint64_t start, end;
//  start = DriveEnv::getInstance()->NowMicros();
//#endif
#ifdef CRC_ENABLED
  // Compute the crc of the record type and the payload.
  uint32_t crc = crc32c::Extend(type_crc_[t], ptr, n);
  crc = crc32c::Mask(crc);                 // Adjust for storage
#else
  uint32_t crc=0;  //Thai
#endif

//#ifdef KDEBUG
//  end = DriveEnv::getInstance()->NowMicros();
//  cout << " CRC TIME " << (end -start) << endl;
//#endif

//  uint32_t crc=0;  //Thai
  EncodeFixed32(buf, crc);

  // Write the header and the payload
  Status s = dest_->Append(Slice(buf, kHeaderSize));
  if (s.ok()) {
    s = dest_->Append(Slice(ptr, n));
    if (s.ok()) {
      s = dest_->Flush();
    }
  }
  if (s.ok()) {
      block_offset_ += kHeaderSize + n;
  }
  return s;
}

}  // namespace log
}  // namespace leveldb
