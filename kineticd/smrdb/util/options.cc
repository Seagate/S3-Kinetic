// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/options.h"

#include "leveldb/comparator.h"
#include "leveldb/env.h"

#include "smrdisk/SmrEnv.h"
#include "smrdisk/DriveEnv.h"

namespace leveldb {

Options::Options()
    : comparator(BytewiseComparator()),
      create_if_missing(false),
      error_if_exists(false),
      paranoid_checks(false),
      env(NULL),
      info_log(NULL),
      outstanding_status_sender(NULL),
      write_buffer_size(4<<20),
      max_open_files(1000),
      block_cache(NULL),
      block_size(4096),
      table_cache_size(50<<20),
      block_restart_interval(4),
      compression(kNoCompression),
      filter_policy(NULL) {
	env = smr::DriveEnv::getInstance();
}


}  // namespace leveldb
