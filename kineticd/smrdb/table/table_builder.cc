// Copyright (c) 2011 The LevelDB Authors. All rights reserved

// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table_builder.h"

#include <assert.h>
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block_builder.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include <iostream>
#include <sys/time.h>
#include "smrdisk/DriveEnv.h"
#include "table/table_rep.h"
#include "smrdisk/Util.h"

using namespace std;

namespace leveldb {

TableBuilder::TableBuilder(const Options& options, WritableFile* file)
    : rep_(new Rep(options, file)) {
  if (rep_->filter_block != NULL) {
    rep_->filter_block->StartBlock(0);
  }
}

TableBuilder::~TableBuilder() {
  assert(rep_->closed);  // Catch errors where caller forgot to call Finish()
  delete rep_->filter_block;
  delete rep_;
}

Status TableBuilder::ChangeOptions(const Options& options) {
  // Note: if more fields are added to Options, update
  // this function to catch changes that should not be allowed to
  // change in the middle of building a Table.
  if (options.comparator != rep_->options.comparator) {
    return Status::InvalidArgument("changing comparator while building table");
  }

  // Note that any live BlockBuilders point to rep_->options and therefore
  // will automatically pick up the updated options.
  rep_->options = options;
  rep_->index_block_options = options;
  return Status::OK();
}

void TableBuilder::Add(const Slice& key, const Slice& value)
{
  return InternalAdd(key, &value, NULL, NULL);
}

void TableBuilder::Add(const Slice& key, const LevelDBData* value, const ExternalValueInfo* external)
{
  return InternalAdd(key, NULL, value, external);
}

void TableBuilder::InternalAdd(const Slice& key, const Slice* slice, const LevelDBData* value, const ExternalValueInfo* external)
{
  Rep* r = rep_;
  assert(!r->closed);
  if (!ok()) { return; }
  if (r->num_entries > 0) {
    assert(r->options.comparator->Compare(key, Slice(r->last_key)) > 0);
  }

#ifdef KDEBUG
  uint64_t start, end;
  start = smr::DriveEnv::getInstance()->NowMicros();
#endif

  if (r->pending_index_entry) {
    assert(r->data_block.empty());
    r->options.comparator->FindShortestSeparator(&r->last_key, key);
    std::string handle_encoding;
    r->pending_handle.EncodeTo(&handle_encoding);
    r->index_block.Add(r->last_key, Slice(handle_encoding));
    if (!r->status.ok()) {
      return;
    }
    r->pending_index_entry = false;
  }

#ifdef KDEBUG
  end = smr::DriveEnv::getInstance()->NowMicros();
  cout << "TIME TO FindShortestSeparator " << (end - start) << endl;
  start = end;
#endif

  if (r->filter_block != NULL) {
    r->filter_block->AddKey(key);
  }

#ifdef KDEBUG
  end = smr::DriveEnv::getInstance()->NowMicros();
  cout << "TIME TO filter_block->AddKey " << (end - start) << endl;
  start = end;
#endif

  r->last_key.assign(key.data(), key.size());
  r->num_entries++;

  if (slice) { //INTERNAL
      //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow interal: key: " << key.ToString() << endl;
    r->data_block.Add(key, *slice);
  } else {  //EXTERNAL
      //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow external: key: " << key.ToString() << endl;
      if (external) {
      //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow external: db val = " << *value << ", exInfo: " << *external << endl;
      } else {
      //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow external: db val = " << *value  << endl;
      }
    r->data_block.Add(key, value, external);
  }

#ifdef KDEBUG
  end = smr::DriveEnv::getInstance()->NowMicros();
  cout << "TIME TO r->data_block.Add " << (end - start) << endl;
  start=end;
#endif


  if (r->data_block.CurrentSizeEstimate() >= r->options.block_size) {
    Flush();
#ifdef KDEBUG
    end = smr::DriveEnv::getInstance()->NowMicros();
    cout << "TIME TO FLUSH " << (end - start) << endl;
#endif
  }
}

void TableBuilder::Flush()
{
  Rep* r = rep_;
  assert(!r->closed);

  if (!ok() || r->data_block.empty()) {
    return;
  }
  assert(!r->pending_index_entry);

  WriteBlock(&r->data_block, &r->pending_handle);

  if (ok()) {
    r->pending_index_entry = true;
    r->status = r->file->Flush();
  }
  /* Do not start a new filterblock for every block written
    if (r->filter_block != NULL) {
      r->filter_block->StartBlock(r->offset);
    }
  */
}

void TableBuilder::WriteBlock(BlockBuilder* block, BlockHandle* handle)
{
  // File format contains a sequence of blocks where each block has:
  //    block_data: uint8[n]
  //    type: uint8
  //    crc: uint32
  assert(ok());
  Rep* r = rep_;
  Slice raw = block->Finish();

  Slice block_contents;
  CompressionType type = r->options.compression;
  switch (type) {
    case kNoCompression:
      block_contents = raw;
      break;

    case kSnappyCompression: {
      std::string* compressed = &r->compressed_output;
      if (port::Snappy_Compress(raw.data(), raw.size(), compressed) &&
          compressed->size() < raw.size() - (raw.size() / 8u)) {
        block_contents = *compressed;
      } else {
        // Snappy not supported, or compressed less than 12.5%, so just
        // store uncompressed form
        block_contents = raw;
        type = kNoCompression;
      }
      break;
    }
  }
  WriteRawBlock(block_contents, type, handle);
  r->compressed_output.clear();
  r->compressed_output.shrink_to_fit();
  block->Reset();
}

void TableBuilder::WriteRawBlock(const Slice& block_contents,
                                 CompressionType type,
                                 BlockHandle* handle)
{
  handle->set_offset(rep_->offset);
  handle->set_size(rep_->file->GetCurrentFileSize() - rep_->offset + block_contents.size());
#ifdef KDEBUG
  cout << " HANDLE OFFSET " << handle->offset() << " HANDLE SIZE " << block_contents.size() << endl;
#endif
  rep_->status = rep_->file->Append(Slice(block_contents.data(), block_contents.size()));
  rep_->offset = rep_->file->GetCurrentFileSize();
}

Status TableBuilder::status() const
{
  return rep_->status;
}


Status TableBuilder::Finish()
{
  Rep* r = rep_;
  Flush();
  if (!ok()) {
    return this->status();
  }
  assert(!r->closed);
  r->closed = true;

  BlockHandle filter_block_handle, metaindex_block_handle, index_block_handle;

  // Write filter block
  if (ok() && r->filter_block != NULL) {
//    cout << "WRITE FILTER" << endl;
    WriteRawBlock(r->filter_block->Finish(), kNoCompression,
                  &filter_block_handle);
  }

  // Write metaindex block
  if (ok()) {
    BlockBuilder meta_index_block(&r->options);
    if (r->filter_block != NULL) {
      // Add mapping from "filter.Name" to location of filter data
      std::string key = "filter.";
      key.append(r->options.filter_policy->Name());
      std::string handle_encoding;
      filter_block_handle.EncodeTo(&handle_encoding);
      meta_index_block.Add(key, handle_encoding);
    }
    if (ok()) {
//      cout << " WRITE META INDEX" << endl;
      WriteBlock(&meta_index_block, &metaindex_block_handle);
    }
  }

  // Write index block
  if (ok()) {
    if (r->pending_index_entry) {
      r->options.comparator->FindShortSuccessor(&r->last_key);
      std::string handle_encoding;
      r->pending_handle.EncodeTo(&handle_encoding);
      r->index_block.Add(r->last_key, Slice(handle_encoding));
      if (r->status.ok()) {
        r->pending_index_entry = false;
      }
    }
    if (ok()) {
//      cout << " WRITE INDEX " << endl;
      WriteBlock(&r->index_block, &index_block_handle);
    }
  }

  // Write footer
  if (ok()) {
    Footer footer;
    footer.set_metaindex_handle(metaindex_block_handle);
    footer.set_index_handle(index_block_handle);
    footer.set_num_keys(NumEntries());
    std::string footer_encoding;
    footer.EncodeTo(&footer_encoding);

    /* Set offset manually, as the real file size might not reflect where the footer
     * will be stored (due to page aligned writes) */
    r->offset = r->file->GetCurrentFileSize() + footer_encoding.size();

    /* Use page aligned buffer in case the file used is direct writable. */
    char * buf;
    int s = posix_memalign((void**)&buf, 4096, ROUNDUP(footer_encoding.size(), 4096));
    if (buf == NULL || s != 0) {
      r->status = Status::IOError("CAN NOT ALLOCATE MEMORY FOR FOOTER");
      return r->status;
    }
    footer_encoding.copy(buf, footer_encoding.size());
//    cout << " WRITE FOOTER " << footer_encoding.size() << endl;
    r->status = r->file->Append(Slice(buf, footer_encoding.size()));
    free(buf);
  }
  return r->status;
}

void TableBuilder::Abandon()
{
  Rep* r = rep_;
  assert(!r->closed);
  r->closed = true;
}

uint64_t TableBuilder::NumEntries() const
{
  return rep_->num_entries;
}

uint64_t TableBuilder::FileSize() const
{
  if(rep_->closed) {
    return rep_->offset;
  }
  return rep_->file->GetCurrentFileSize();
}

}  // namespace leveldb
