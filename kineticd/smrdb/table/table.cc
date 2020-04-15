// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table.h"
#include <stdio.h>
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include <iostream>
#include "db/version_set.h"
#include "smrdisk/SmrRandomAccessFile.h"
#include "zac_mediator.h"
using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using ::zac_ha_cmd::ZacZone;


using namespace std;
namespace leveldb {

struct Table::Rep {
  ~Rep() {
    delete filter;
    delete [] filter_data;
    delete index_block;
  }

  Options options;
  Status status;
  RandomAccessFile* file;
  uint64_t cache_id;
  FilterBlockReader* filter;
  const char* filter_data;

  BlockHandle metaindex_handle;  // Handle to metaindex_block: saved from footer
  Block* index_block;
  uint64_t num_keys;
};

Status Table::Open(const Options& options,
                   RandomAccessFile* file,
                   uint64_t size,
                   Table** table) {
  *table = NULL;
  if (size < Footer::kEncodedLength) {
    return Status::InvalidArgument("file is too short to be an sstable");
  }

  char footer_space[Footer::kEncodedLength];
  Slice footer_input;
  Status s = file->Read(size - Footer::kEncodedLength, Footer::kEncodedLength,
                        &footer_input, footer_space);			
  if (!s.ok()) {
#ifdef KDEBUG
     cout << "1. BAD FOOTER" << endl;
#endif
     return s;
  }

  Footer footer;
  s = footer.DecodeFrom(&footer_input);
  if (!s.ok()) {
#ifdef KDEBUG
    cout << "2. BAD FOOTER" << endl;
#endif
    smr::SmrRandomAccessFile* smrFile = (smr::SmrRandomAccessFile*)file;
    FileInfo* finfo = smrFile->getFileInfo();
    stringstream ss;
    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": "
       << *finfo;

    AtaCmdHandler ataCmdHandler;
    ZacMediator zacMediator(&ataCmdHandler);

    if (zacMediator.OpenDevice(DriveEnv::getInstance()->storePartition().c_str()) < 0) {
        cout << "Could not open device" << endl;
    } else {
        vector<Segment*>* segments = finfo->getSegments();
        vector<Segment*>::iterator it = segments->begin();
        for (; it != segments->end(); ++it) {
            ZacZone zacZone;
            zacMediator.GetZoneInfo(&zacZone, (*it)->getZoneNumber());
            ss << endl << zacZone;
        }
        if (zacMediator.CloseDevice() != 0) {
            cout << "Could not close device" << endl;
        }
    }
    Status newStatus(s.code(), s.ToString(), ss.str());
    ss.clear();
    return newStatus;
  }
  // Read the index block
  BlockContents contents;
  Block* index_block = NULL;
  if (s.ok()) {
    s = ReadBlock(file, ReadOptions(), footer.index_handle(), &contents);
    if (s.ok()) {
      index_block = new Block(contents);
    } else {
#ifdef KDEBUG
     cout << "1. BAD INDEX BLOCK" << endl;
#endif
    }
  }

  if (s.ok()) {
    // We've successfully read the footer and the index block: we're
    // ready to serve requests.
    Rep* rep = new Table::Rep;
    rep->options = options;
    rep->file = file;
    rep->metaindex_handle = footer.metaindex_handle();
    rep->index_block = index_block;
    rep->cache_id = (options.block_cache ? options.block_cache->NewId() : 0);
    rep->filter_data = NULL;
    rep->filter = NULL;
    rep->num_keys = footer.num_keys();
    *table = new Table(rep);
    (*table)->md_size_ = sizeof(rep) + index_block->size();
    (*table)->ReadMeta(footer);
  } else {
    if (index_block) delete index_block;
  }

  return s;
}

void Table::ReadMeta(const Footer& footer) {
  if (rep_->options.filter_policy == NULL) {
    return;  // Do not need any metadata
  }

  // TODO(sanjay): Skip this if footer.metaindex_handle() size indicates
  // it is an empty block.
  ReadOptions opt;
  BlockContents contents;
  if (!ReadBlock(rep_->file, opt, footer.metaindex_handle(), &contents).ok()) {
    // Do not propagate errors since meta info is not needed for operation
    return;
  }
  Block* meta = new Block(contents);

  Iterator* iter = meta->NewIterator(BytewiseComparator());
  std::string key = "filter.";
  key.append(rep_->options.filter_policy->Name());
  iter->Seek(key);
  if (iter->Valid() && iter->key() == Slice(key)) {
    ReadFilter(iter->value());
  }
  delete iter;
  delete meta;
}

void Table::ReadFilter(const Slice& filter_handle_value) {
  Slice v = filter_handle_value;
  BlockHandle filter_handle;
  if (!filter_handle.DecodeFrom(&v).ok()) {
    return;
  }

  // We might want to unify with ReadBlock() if we start
  // requiring checksum verification in Table::Open.
  ReadOptions opt;
  BlockContents block;
  if (!ReadBlock(rep_->file, opt, filter_handle, &block).ok()) {
    return;
  }
  if (block.heap_allocated) {
    rep_->filter_data = block.data.data();     // Will need to delete later
  }
  rep_->filter = new FilterBlockReader(rep_->options.filter_policy, block.data);
  md_size_ += block.data.size();
/*  cout << " READ FILTER SIZE " << block.data.size() << endl;
  for(int i=0; i< block.data.size(); ++i) {
     printf(" %02x", rep_->filter_data[i]);
     if(i%15 == 0) {
        cout << endl;
     }
  } 
  cout << dec << endl;
*/
}

size_t Table::GetMetadataSize() const {
  return md_size_;
}

Table::~Table() {
  delete rep_;
}

static void DeleteBlock(void* arg, void* ignored) {
  delete reinterpret_cast<Block*>(arg);
}

static void DeleteCachedBlock(const Slice& key, void* value) {
  Block* block = reinterpret_cast<Block*>(value);
  delete block;
}

static void ReleaseBlock(void* arg, void* h) {
  Cache* cache = reinterpret_cast<Cache*>(arg);
  Cache::Handle* handle = reinterpret_cast<Cache::Handle*>(h);
  cache->Release(handle);
}

uint64_t Table::NumKeys() const {
    return rep_->num_keys;
}

// Convert an index iterator value (i.e., an encoded BlockHandle)
// into an iterator over the contents of the corresponding block.
Iterator* Table::BlockReader(void* arg,
                             const ReadOptions& options,
                             const Slice& index_value,
                             char** buff) {
  Table* table = reinterpret_cast<Table*>(arg);
  Cache* block_cache = table->rep_->options.block_cache;
  Block* block = NULL;
  Cache::Handle* cache_handle = NULL;

  BlockHandle handle;
  Slice input = index_value;
  Status s = handle.DecodeFrom(&input);
  // We intentionally allow extra stuff in index_value so that we
  // can add more features in the future.

  if (s.ok()) {
    BlockContents contents;
    if (block_cache != NULL) {
      char cache_key_buffer[16];
      EncodeFixed64(cache_key_buffer, table->rep_->cache_id);
      EncodeFixed64(cache_key_buffer+8, handle.offset());
      Slice key(cache_key_buffer, sizeof(cache_key_buffer));
      cache_handle = block_cache->Lookup(key);
      if (cache_handle != NULL) {
        block = reinterpret_cast<Block*>(block_cache->Value(cache_handle));
      } else {
        s = ReadBlock(table->rep_->file, options, handle, &contents);
        if (s.ok()) {
          block = new Block(contents);
//          char* buff1 = (char*)contents.data.data();
//          if(buff != NULL) {
//            *buff = buff1;
//          }
          if (contents.cachable && options.fill_cache) {
            cache_handle = block_cache->Insert(
                key, block, block->size(), &DeleteCachedBlock);
          }
        }
      }
    } else {
      s = ReadBlock(table->rep_->file, options, handle, &contents);
      if (s.ok()) {
        block = new Block(contents);
        char* buff1 = (char*)contents.data.data();
        if(buff != NULL) {
           *buff = buff1;
        }
      }
    }
  }

  Iterator* iter;
  if (block != NULL) {
    iter = block->NewIterator(table->rep_->options.comparator);
    if (cache_handle == NULL) {
      iter->RegisterCleanup(&DeleteBlock, block, NULL);
    } else {
      iter->RegisterCleanup(&ReleaseBlock, block_cache, cache_handle);
    }
  } else {
    iter = NewErrorIterator(s);
  }
  if (!iter->status().ok()) {
      stringstream ss;
      smr::SmrRandomAccessFile* smrFile = (smr::SmrRandomAccessFile*)table->rep_->file;
      ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": "
         << endl << *(smrFile->getFileInfo());
      Status s(iter->status().code(), iter->status().ToString(), ss.str());
      iter->status(s);
      ss.clear();
  }

  return iter;
}

Iterator* Table::NewIterator(const ReadOptions& options) const {
  return NewTwoLevelIterator(
      rep_->index_block->NewIterator(rep_->options.comparator),
      &Table::BlockReader, const_cast<Table*>(this), options);
}

Status Table::InternalGet(const ReadOptions& options, const Slice& k,
                          void* arg,
                          void (*saver)(void*, const Slice&, const Slice&),
			  bool using_bloom_filter) {
  Status s;
  char* buff;
  // Check the single BF and goto index only if found

  FilterBlockReader* filter = rep_->filter;
//  fprintf(stdout, "Table::InternalGet using bloom %s\n", using_bloom_filter ? "true" : "false");
  if (filter != NULL && using_bloom_filter && !filter->KeyMayMatch( 0, k)) {
      // Not found
  } else {
    Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
    iiter->Seek(k);
    if (iiter->Valid()) {
      Iterator* block_iter = BlockReader(this, options, iiter->value(), &buff);
      block_iter->Seek(k);
      if (block_iter->Valid()) {
        (*saver)(arg, block_iter->key(), block_iter->value());
      }
      s = block_iter->status();
      delete block_iter;
    }
    if (s.ok()) {
      s = iiter->status();
    }
    delete iiter;
  }
  return s;
}


uint64_t Table::ApproximateOffsetOf(const Slice& key) const {
  Iterator* index_iter =
      rep_->index_block->NewIterator(rep_->options.comparator);
  index_iter->Seek(key);
  uint64_t result;
  if (index_iter->Valid()) {
    BlockHandle handle;
    Slice input = index_iter->value();
    Status s = handle.DecodeFrom(&input);
    if (s.ok()) {
      result = handle.offset();
    } else {
      // Strange: we can't decode the block handle in the index block.
      // We'll just return the offset of the metaindex block, which is
      // close to the whole file size for this case.
      result = rep_->metaindex_handle.offset();
    }
  } else {
    // key is past the last key in the file.  Approximate the offset
    // by returning the offset of the metaindex block (which is
    // right near the end of the file).
    result = rep_->metaindex_handle.offset();
  }
  delete index_iter;
  return result;
}

}  // namespace leveldb
