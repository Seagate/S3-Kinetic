// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/builder.h"
#include "db/filename.h"
#include "db/dbformat.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "leveldb/mydata.h"
#include "smrdisk/DriveEnv.h"
#include "smrdisk/ValueBuilder.h"
#include <iostream>
#include <sys/time.h>

using namespace std;
using namespace smr;

namespace leveldb {

/* Generate L0 SST table. Embed all small values into the SST table. Write out bigger values into
 * a data file and only store corresponding ExternalValueInfo structure in SST. */
Status BuildTable(const std::string& dbname,
                  Env* env,
                  const Options& options,
                  TableCache* table_cache,
                  Iterator* iter,
                  FileMetaData* meta,
                  WritableFile** rfile)
{
#ifdef KDEBUG
  uint64_t start, start_all, end, vb_total, tb_total;
  start = start_all = DriveEnv::getInstance()->NowMicros();
#endif

  Status s;
  iter->SeekToFirst();
  meta->file_size = 0;
  meta->smallest.DecodeFrom(iter->key());

  // Generate new writable file for the sst table, a new sst table builder and a new value builder
  std::string tableFileName = TableFileName(dbname, meta->number, meta->level);
  WritableFile* sst_file;
  s = env->NewWritableFile(tableFileName, &sst_file);
  if (!s.ok()) {
    return s;
  }
  *rfile = sst_file;
  auto table_builder = new TableBuilder(options, sst_file);
#ifdef KDEBUG
  end = DriveEnv::getInstance()->NowMicros();
  cout << "   TIME TO GENERATE TABLE BUILDER " << (end-start) << endl << endl;;
  start = end;
#endif

  auto value_builder = new ValueBuilder(dbname, meta->number, options);
  s = value_builder->ObtainWritableFile();
  
#ifdef KDEBUG
  end = DriveEnv::getInstance()->NowMicros();
  cout << "   TIME TO GENERATE VALUE BUILDER " << (end-start) << endl << endl;;
  start = end;
#endif
  int i = 0;
  for (; iter->Valid() && s.ok(); iter->Next(), i++) {
    Slice key = iter->key();

    // Function called only for L0; So largest access is fine.
    meta->largest.DecodeFrom(key);

    /* Deletion key */
    if (!iter->value().data()) {
      table_builder->Add(key, iter->value());
    }
    /* Value key */
    else {
      ExternalValueInfo* ext = NULL;
      const LevelDBData* data = (const LevelDBData*) iter->value().data();
      if (data->dataSize >= options.value_size_threshold) {
        s = value_builder->Append(key, data, ext);
      }
      if(s.ok()) {
        table_builder->Add(key, data, ext);
      }
    }
    if (s.ok()) {
      s = table_builder->status();
    }
  }
#ifdef KDEBUG
  end = DriveEnv::getInstance()->NowMicros();
  cout << "   TIME TO ADD " << i << " KEYS " << (end-start) << " (average=" << (end-start)/i << ")" << endl;
  start = end;
#endif

  if (!iter->status().ok()) {
    s = iter->status();
  }

  // Finish value builder first
  if (s.ok()) {
    s = value_builder->Finish();
  }
  // Finish and check for table builder errors
  if (s.ok()) {
    s = table_builder->Finish();
    if (s.ok()) {
      meta->file_size = table_builder->FileSize();
      assert(meta->file_size > 0);
    }
  }
  if (!s.ok()) {
      cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << endl;
    value_builder->Abandon();
    table_builder->Abandon();
  }
  delete value_builder;
  delete table_builder;

#ifdef KDEBUG
  end = DriveEnv::getInstance()->NowMicros();
  cout << "   TIME TO FINISH BUILDERS " << (end-start) << endl;
  cout << "   TOTAL TIME IN BUILDTABLE: " << end - start_all << endl;
#endif
  return s;
}


}  // namespace leveldb
