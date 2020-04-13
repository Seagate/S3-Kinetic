// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/table_cache.h"

#include "db/filename.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/coding.h"
#include <iostream>
using namespace std;

namespace leveldb {

struct TableAndFile {
  RandomAccessFile* file;
  Table* table;
};

static void DeleteEntry(const Slice& key, void* value) {
  TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
  delete tf->table;
  delete tf->file;
  delete tf;
}

static void UnrefEntry(void* arg1, void* arg2) {
  Cache* cache = reinterpret_cast<Cache*>(arg1);
  Cache::Handle* h = reinterpret_cast<Cache::Handle*>(arg2);
  cache->Release(h);
}

TableCache::TableCache(const std::string& dbname,
                       const Options* options,
                       size_t tc_size)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      cache_(NewLRUCache(tc_size)) {
}

TableCache::~TableCache() {
  delete cache_;
}

Status TableCache::FindTable(uint64_t file_number, uint64_t file_size, int level,
                             Cache::Handle** handle) {
  Status s;
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  Slice key(buf, sizeof(buf));
  *handle = cache_->Lookup(key);
  if (*handle == NULL) {
    std::string fname = TableFileName(dbname_, file_number, level);
    RandomAccessFile* file = NULL;
    Table* table = NULL;
    s = env_->NewRandomAccessFile(fname, &file);
    if (!s.ok()) {
      std::string old_fname = SSTTableFileName(dbname_, file_number);
      if (env_->NewRandomAccessFile(old_fname, &file).ok()) {
        s = Status::OK();
      }
    }
    if (s.ok()) {
      s = Table::Open(*options_, file, file_size, &table);
    }

    if (!s.ok()) {
      Log(options_->info_log, 5," %s", s.ToString().c_str());
      assert(table == NULL);
      delete file;
      // We do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else {
      TableAndFile* tf = new TableAndFile;
      tf->file = file;
      tf->table = table;
      // Insert with size based charge for the entry.
      *handle = cache_->Insert(key, tf, tf->table->GetMetadataSize(), &DeleteEntry);
    }
  }
  return s;
}

Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number,
                                  uint64_t file_size,
				  int level,
                                  Table** tableptr) {
  if (tableptr != NULL) {
    *tableptr = NULL;
  }
  Cache::Handle* handle = NULL;
  Status s = FindTable(file_number, file_size, level, &handle);
  if (!s.ok()) {
    return NewErrorIterator(s);
  }

  Table* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
  Iterator* result = table->NewIterator(options);
  result->RegisterCleanup(&UnrefEntry, cache_, handle);
  if (tableptr != NULL) {
    *tableptr = table;
  }
  return result;
}

Status TableCache::Get(const ReadOptions& options,
                       uint64_t file_number,
                       uint64_t file_size,
		       int level,
                       const Slice& k,
                       void* arg,
                       void (*saver)(void*, const Slice&, const Slice&),
		       bool using_bloom_filter) {
  Cache::Handle* handle = NULL;
#ifdef KDEBUG
    struct timeval tv;
    uint64_t start, end;
    gettimeofday(&tv, NULL);
    start = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif
  Status s = FindTable(file_number, file_size, level, &handle);
#ifdef KDEBUG
    gettimeofday(&tv, NULL);
    end = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    cout << "TIME TO FIND TABLE " << file_number << " file size " << file_size << " " << (end - start) << endl;
#endif
  if (s.ok()) {
//    cout << " TABLE CACHE GET " << file_number << endl;
  
    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    s = t->InternalGet(options, k, arg, saver, using_bloom_filter);
#ifdef KDEBUG
    gettimeofday(&tv, NULL);
    start = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    cout << "TIME TO INTERNALGET " << file_number << " file size " << file_size << " " << (start - end) << endl;
#endif
    cache_->Release(handle);
  }
  return s;
}

void TableCache::Evict(uint64_t file_number) {
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  cache_->Erase(Slice(buf, sizeof(buf)));
}

}  // namespace leveldb
