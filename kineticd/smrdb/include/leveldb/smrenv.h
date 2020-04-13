// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_HELPERS_SMRENV_SMRENV_H_
#define STORAGE_LEVELDB_HELPERS_SMRENV_SMRENV_H_

namespace leveldb {

class Env;

// Returns a new SMR environment that stores its data directly in disk bands
Env* NewSmrEnv();
Env* GetSmrEnv();
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_HELPERS_SMRENV_SMRENV_H_
