// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/db_impl.h"
#include <algorithm>
#include <set>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <unordered_set>
#include <sys/syscall.h>
#include <memory>
#include "db/builder.h"
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/status.h"
#include "leveldb/table.h"
#include "leveldb/table_builder.h"
#include "kernel_mem_mgr.h"
#include "port/port.h"
#include "table/block.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include <iostream>
#include <sys/time.h>
#include "leveldb/env.h"
#include "smrdisk/SmrRandomAccessFile.h"
#include "smrdisk/ValueFileCache.h"
#include "smrdisk/DriveEnv.h"

extern "C" {
   extern bool kineticd_idle;
}

using namespace std;

namespace leveldb {

const int kNumNonTableCacheFiles = 10;
const int kMaxKeySize = 1024;
const uint32_t kMaxToLog = 2*1024*1024;

// Information kept for every waiting writer
struct DBImpl::Writer {
  Status status;
  WriteBatch* batch;
  bool sync;
  bool done;
  port::CondVar cv;

  explicit Writer(port::Mutex* mu) : cv(mu) { }
};

struct DBImpl::CompactionState {
  Compaction* const compaction;

  // Sequence numbers < smallest_snapshot are not significant since we
  // will never have to service a snapshot below smallest_snapshot.
  // Therefore if we have seen a sequence number S <= smallest_snapshot,
  // we can drop all entries for the same key with sequence numbers < S.
  SequenceNumber smallest_snapshot;

  // Files produced by compaction
  struct Output {
    uint64_t number;
    uint64_t file_size;
    InternalKey smallest, largest;
  };
  std::vector<Output> outputs;

  // State kept for output being generated
  WritableFile* outfile;
  TableBuilder* builder;
  ExternalValueDeleter* deleter;

  uint64_t total_bytes;

  Output* current_output() { return &outputs[outputs.size()-1]; }

  explicit CompactionState(Compaction* c)
      : compaction(c),
        outfile(NULL),
        builder(NULL),
        deleter(NULL),
        total_bytes(0) {
  }
};

// Fix user-supplied options to be reasonable
template <class T,class V>
static void ClipToRange(T* ptr, V minvalue, V maxvalue) {
  if (static_cast<V>(*ptr) > maxvalue) *ptr = maxvalue;
  if (static_cast<V>(*ptr) < minvalue) *ptr = minvalue;
}
Options SanitizeOptions(const std::string& dbname,
                        const InternalKeyComparator* icmp,
                        const InternalFilterPolicy* ipolicy,
                        const Options& src) {
  Options result = src;
  result.comparator = icmp;
  result.filter_policy = (src.filter_policy != NULL) ? ipolicy : NULL;
  ClipToRange(&result.max_open_files,    64 + kNumNonTableCacheFiles, 50000);
  ClipToRange(&result.write_buffer_size, 64<<10,                      1<<30);
  ClipToRange(&result.block_size,        1<<10,                       4<<20);
  if (result.info_log == NULL) {
    // Open a log file in the same directory as the db
    src.env->CreateDir(dbname);  // In case it does not exist
    src.env->RenameFile(InfoLogFileName(dbname), OldInfoLogFileName(dbname));
    Status s = src.env->NewLogger(InfoLogFileName(dbname), &result.info_log);
    if (!s.ok()) {
      // No place suitable for logging
      result.info_log = NULL;
    }
  }
  if (result.block_cache == NULL) {
    // Paul: This should NOT be 0, value-out processes blocks the normal leveldb way
    //       Reverting to leveldb standard configuration.
    result.block_cache = NewLRUCache(8 << 20);
  }
  return result;
}

DBImpl::DBImpl(const Options& raw_options, const std::string& dbname)
    : env_(raw_options.env),
      internal_comparator_(raw_options.comparator),
      internal_filter_policy_(raw_options.filter_policy),
      options_(SanitizeOptions(dbname, &internal_comparator_,
                               &internal_filter_policy_, raw_options)),
      owns_info_log_(options_.info_log != raw_options.info_log),
      owns_cache_(options_.block_cache != raw_options.block_cache),
      dbname_(dbname),
      db_lock_(NULL),
      shutting_down_(NULL),
      bg_cv_(&mutex_),
      mem_(new MemTable(internal_comparator_)),
      imm_(NULL),
      logfile_(NULL),
      persistedLogFile_(NULL),
      logfile_number_(0),
      log_(NULL),
      seed_(0),
      tmp_batch_(new WriteBatch),
      bg_compaction_scheduled_(false),
      manual_compaction_(NULL) {
  mem_->Ref();
  logmem_in_use_ = 0;
  current_logmem_ = &logmem_[logmem_in_use_];
  current_arena_for_log_ = new Arena;
  logmem_imm_ = NULL;
  arena_for_log_imm_ = NULL;
  has_imm_.Release_Store(NULL);
  defrag_going_ = false;
  valueDefragmentScheduled_ = false;
  sstDefragmentScheduled_ = false;
  KernelMemMgr::GetInstance();
  const int table_cache_size = options_.table_cache_size;
  table_cache_ = new TableCache(dbname_, &options_, table_cache_size);

  versions_ = new VersionSet(dbname_, &options_, table_cache_,
                             &internal_comparator_);

  // allocate max key size bytes for the end key member to use
  end_key_ = new char[kMaxKeySize];
  // initialize end key slice to be ""
  end_key_slice_ = Slice(end_key_, size_t(0));
}

DBImpl::~DBImpl() {
  // Wait for background work to finish
#ifdef KDEBUG
  cout << "FLUSH BY ~DBIMPL" << endl;
#endif

  mutex_.Lock();
  cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Enter" << endl;
  shutting_down_.Release_Store(this);  // Any non-NULL value is ok
  while (bg_compaction_scheduled_ || sstDefragmentScheduled_ || valueDefragmentScheduled_) {
    bg_cv_.Wait();
  }
  cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": No activity" << endl;
  mutex_.Unlock();

  if (db_lock_ != NULL) {
    env_->UnlockFile(db_lock_);
  }

  delete versions_;

  if (mem_ != NULL) {
    mem_->Unref();
  }
  if (imm_ != NULL) {
    imm_->Unref();
  }
  delete tmp_batch_;
  if (log_ != NULL) {
      delete log_;
  }
  if (logfile_ != NULL) {
      logfile_->Sync();
      env_->Sync();
      delete logfile_;
  }
  if (persistedLogFile_) {
      delete persistedLogFile_;
      std::string fname = LogFileName(dbname_, ((SmrWritableFile*)persistedLogFile_)->getFileInfo()->getNumber());
      env_->DeleteFile(fname);
      persistedLogFile_ = NULL;
  }
  std::deque<struct BatchRecord *>::iterator itr;
  for(itr= current_logmem_->begin(); itr != current_logmem_->end(); ++itr) {
    delete *itr;
  }
  current_logmem_->clear();
  delete table_cache_;

  if (owns_info_log_) {
    delete options_.info_log;
  }
  if (owns_cache_) {
    delete options_.block_cache;
  }
  delete[] end_key_;

  CacheManager::clear();
  env_->clearDisk();

  delete current_arena_for_log_;
  cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit" << endl;
}

Status DBImpl::NewDB() {
  VersionEdit new_db;
  new_db.SetComparatorName(user_comparator()->Name());
  new_db.SetLogNumber(0);
  new_db.SetNextFile(2);
  new_db.SetLastSequence(0);

  const std::string manifest = DescriptorFileName(dbname_, 1);
  WritableFile* file;
  Status s = env_->NewWritableFile(manifest, &file);
  if (!s.ok()) {
    return s;
  }
  {
    log::Writer log(file, env_, dbname_);
    std::string record;
    new_db.EncodeTo(&record);
    s = log.AddRecord(record);
    if (s.ok()) {
      s = file->Close();
    }
  }
  delete file;
  if (s.ok()) {
    // Make "CURRENT" file that points to the new manifest file.
    s = SetCurrentFile(env_, dbname_, 1);
  } else {
    env_->DeleteFile(manifest);
  }
  return s;
}

void DBImpl::MaybeIgnoreError(Status* s) const {
  if (s->ok() || options_.paranoid_checks) {
    // No change needed
  } else {
    Log(options_.info_log, 0, "Ignoring error %s", s->ToString().c_str());
    *s = Status::OK();
  }
}

void DBImpl::DeleteObsoleteFiles(VersionEdit* vEdit) {
  if (!bg_error_.ok() && !bg_error_.IsNoSpaceAvailable()) {
    // After a background error, we don't know whether a new version may
    // or may not have been committed, so we cannot safely garbage collect.
    return;
  }
#ifdef KDEBUG
  cout << "****START TO DELETE " << endl;
  uint64_t start, start1, end;
  start = env_->NowMicros();
  start1 = start;
#endif
  bool bNeedToSync = false;
  if (vEdit) {
        std::set< std::pair<int, uint64_t> >* obsoleteFiles = vEdit->GetDeletedFiles();
        this->deletedFiles_.insert(obsoleteFiles->begin(), obsoleteFiles->end());
	//DO NOT DELETE THE NEXT LINE. FOR SCALE DOWN TESTING.
        //if (bg_error_.IsNoSpaceAvailable() || deletedFiles_.size() > 8) {
        if (bg_error_.IsNoSpaceAvailable() || deletedFiles_.size() > 0 || versions_->HasDeletableValueFiles()) {
            std::unordered_set<uint64_t> live = pending_outputs_;
            versions_->AddLiveFiles(&live);

#ifdef KDEBUG
            end = env_->NowMicros();
            cout << "   TIME TO ADD LIVEFILES " << (end-start) << endl;
            start = end;
#endif
            std::set< std::pair<int, uint64_t> >::iterator it = deletedFiles_.begin();
            while (it != deletedFiles_.end()) {
                uint64_t number = it->second;
                if (live.find(number) == live.end()) {
                    string fname;
                    fname.resize(100);
                    int n = snprintf((char *)fname.c_str(),100, "%06llu.%s", (unsigned long long) number, "ldb");
                    fname.resize(n);
                    table_cache_->Evict(number);
                    env_->DeleteFile(dbname_ + "/" + fname);
                    Log(options_.info_log, 5, "Delete type=2 #%lld\n",
                        static_cast<unsigned long long>(number));
                    it = deletedFiles_.erase(it);
                } else {
                    ++it;
                }
            }
            if (versions_->HasDeletableValueFiles()) {
                bNeedToSync = true;
                versions_->DeallocateDeletableValueFiles();
            }
        }
    } else {
      // Make a set of all of the live files
      std::unordered_set<uint64_t> live = pending_outputs_;
      versions_->AddLiveFiles(&live);
    #ifdef KDEBUG
        end = env_->NowMicros();
        cout << "   TIME TO ADD LIVEFILES " << (end-start) << endl;
        start = end;
    #endif

      std::vector<std::string> filenames;
      env_->GetChildren(dbname_, &filenames); // Ignoring errors on purpose
    #ifdef KDEBUG
        end = env_->NowMicros();
        cout << "   TIME TO GET CHILDREN " << (end-start) << endl;
        start = end;
    #endif
      uint64_t number;
      FileType type;
      Status status;
      for (size_t i = 0; i < filenames.size() && status.ok(); i++) {
        if (ParseFileName(filenames[i], &number, &type)) {
          bool keep = true;
          switch (type) {
            case kLogFile:
              keep = ((number > versions_->LogNumber()) ||
                      (number == versions_->PrevLogNumber()));
              break;
            case kDescriptorFile:
              // Keep my manifest file, and any newer incarnations'
              // (in case there is a race that allows other incarnations)
              keep = (number >= versions_->ManifestFileNumber());
              break;
            case kTableFile:
              keep = (live.find(number) != live.end());
              break;
            case kTempFile:
              // Any temp files that are currently being written to must
              // be recorded in pending_outputs_, which is inserted into "live"
              keep = (live.find(number) != live.end());
              break;
            case kCurrentFile:
            case kDBLockFile:
            case kInfoLogFile:
              keep = true;
              break;
          }

          if (!keep) {
            bNeedToSync = true;
            if (type == kTableFile) {
              table_cache_->Evict(number);
            }
            Log(options_.info_log, 5, "Delete type=%d #%lld\n",
                int(type),
                static_cast<unsigned long long>(number));
            env_->DeleteFile(dbname_ + "/" + filenames[i]);
          }
        }
      }
      status = env_->Sync();
    #ifdef KDEBUG
        end = env_->NowMicros();
        cout << "   TIME TO DELETEFILES " << (end-start) << endl;
        start = end;
    #endif
  }
  if (bNeedToSync) {
      env_->Sync();
  }
}

Status DBImpl::Recover(VersionEdit* edit) {
  mutex_.AssertHeld();

  // If CreateDir returns error, then we were not able to loadDB, fail out
  // early.

  Status s = env_->CreateDir(dbname_, options_.create_if_missing);
  if (!s.ok()) {
    Log(options_.info_log, 5, "%s: DBImpl::Recover: Failed CreateDir", __func__);
    return s;
  }

  assert(db_lock_ == NULL);
  s = env_->LockFile(LockFileName(dbname_), &db_lock_);
  if (!s.ok()) {
    Log(options_.info_log, 5, "%s: Failed LockFile", __func__);
    return s;
  }

  // TODO(Gonzalo): May want to remove this whole conditional
  if (!env_->FileExists(CurrentFileName(dbname_))) {
    if (options_.create_if_missing) {
      s = NewDB();
      if (!s.ok()) {
        return s;
      }
    } else {
      return Status::InvalidArgument(
          dbname_, "does not exist (create_if_missing is false)");
    }
  } else {
    if (options_.error_if_exists) {
      return Status::InvalidArgument(
          dbname_, "exists (error_if_exists is true)");
    }
  }
  s = versions_->Recover();
  if (s.ok()) {
    SequenceNumber max_sequence(0);

    // Recover from all newer log files than the ones named in the
    // descriptor (new log files may have been added by the previous
    // incarnation without registering them in the descriptor).
    //
    // Note that PrevLogNumber() is no longer used, but we pay
    // attention to it in case we are recovering a database
    // produced by an older version of leveldb.
    const uint64_t min_log = versions_->LogNumber();
    const uint64_t prev_log = versions_->PrevLogNumber();
    std::vector<std::string> filenames;
    s = env_->GetChildren(dbname_, &filenames);
    if (!s.ok()) {
      return s;
    }
    std::unordered_set<uint64_t> expected;
    versions_->AddLiveFiles(&expected);
    uint64_t number;
    FileType type;
    std::vector<uint64_t> logs;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type)) {
        expected.erase(number);
        if (type == kLogFile && ((number > min_log) || (number == prev_log))) {
          logs.push_back(number);
        }
        versions_->MarkFileNumberUsed(number);
      }
    }

    if (!expected.empty()) {
      char buf[50];
      snprintf(buf, sizeof(buf), "%d missing files; e.g.",
               static_cast<int>(expected.size()));
       //TODO: Thai TableFileName need level
      return Status::Corruption(buf, TableFileName(dbname_, *(expected.begin())));
    }

    // Recover in the order in which the logs were generated
    std::sort(logs.begin(), logs.end());
    Log(options_.info_log, 5, "Number of Log to Recover %lu", logs.size());

    versions_->SetManifestFileNumber(versions_->NewFileNumber());

    for (size_t i = 0; i < logs.size() && s.ok(); i++) {
      s = RecoverLogFile(logs[i], edit, &max_sequence);
      // The previous incarnation may not have written any MANIFEST
      // records after allocating this log number.  So we manually
      // update the file number allocation counter in VersionSet.
      if (s.ok()) {
         versions_->MarkFileNumberUsed(logs[i]);
      }
    }

    if (s.ok()) {
      if (versions_->LastSequence() < max_sequence) {
        versions_->SetLastSequence(max_sequence);
      }
    }
  }

  return s;
}

Status DBImpl::RecoverLogFile(uint64_t log_number,
                              VersionEdit* edit,
                              SequenceNumber* max_sequence) {
  struct LogReporter : public log::Reader::Reporter {
    Env* env;
    Logger* info_log;
    const char* fname;
    Status* status;  // NULL if options_.paranoid_checks==false
    virtual void Corruption(size_t bytes, const Status& s) {
      Log(info_log, 5, "%s%s: dropping %d bytes; %s",
          (this->status == NULL ? "(ignoring error) " : ""),
          fname, static_cast<int>(bytes), s.ToString().c_str());
      if (this->status != NULL && this->status->ok()) *this->status = s;
    }
  };

  mutex_.AssertHeld();

  // Open the log file
  std::string fname = LogFileName(dbname_, log_number);
  SequentialFile* file;
  Status status = env_->NewSequentialFile(fname, &file);
  if (!status.ok()) {
    MaybeIgnoreError(&status);
    return status;
  }

  // Create the log reader.
  LogReporter reporter;
  reporter.env = env_;
  reporter.info_log = options_.info_log;
  reporter.fname = fname.c_str();
  reporter.status = (options_.paranoid_checks ? &status : NULL);
  // We intentially make log::Reader do checksumming even if
  // paranoid_checks==false so that corruptions cause entire commits
  // to be skipped instead of propagating bad information (like overly
  // large sequence numbers).
  log::Reader reader(file, &reporter, true/*checksum*/,
                     0/*initial_offset*/);
  Log(options_.info_log, 0, "Recovering log #%llu",
      (unsigned long long) log_number);

  // Read all the records and add to a memtable
  std::string scratch;
  Slice record;
  WriteBatch batch;
  MemTable* mem = NULL;
  while (reader.ReadRecord(&record, &scratch) &&
         status.ok()) {
    if (record.size() < 12) {
      reporter.Corruption(
          record.size(), Status::Corruption("log record too small"));
      continue;
    }
    WriteBatchInternal::SetContents(&batch, record);

    if (mem == NULL) {
      mem = new MemTable(internal_comparator_);
      mem->Ref();
    }
    status = WriteBatchInternal::InsertInto(&batch, mem);
    MaybeIgnoreError(&status);
    if (!status.ok()) {
      break;
    }
    const SequenceNumber last_seq =
        WriteBatchInternal::Sequence(&batch) +
        WriteBatchInternal::Count(&batch) - 1;
    if (last_seq > *max_sequence) {
      *max_sequence = last_seq;
    }

    if (mem->ApproximateMemoryUsage() > options_.write_buffer_size) {
      status = WriteLevel0Table(mem, edit, NULL);
      if (!status.ok()) {
        // Reflect errors immediately so that conditions like full
        // file-systems cause the DB::Open() to fail.
        break;
      }
      mem->Unref();
      mem = NULL;
    }
  }

  if (status.ok() && mem != NULL) {
    status = WriteLevel0Table(mem, edit, NULL);
    // Reflect errors immediately so that conditions like full
    // file-systems cause the DB::Open() to fail.
  }
  if (mem != NULL) {
    mem->Unref();
  }
  delete file;
  if (status.ok()) {
      versions_->SetPrevLogNumber(versions_->LogNumber());
      versions_->SetLogNumber(log_number);
  }
  return status;
}

Status DBImpl::WriteLevel0Table(MemTable* mem, VersionEdit* edit,
                                Version* base) {
  mutex_.AssertHeld();
  const uint64_t start_micros = env_->NowMicros();
  FileMetaData meta;
  meta.level=0;
  meta.number = versions_->NewFileNumber();
  pending_outputs_.insert(meta.number);
  Iterator* iter = mem->NewIterator();
#ifndef NLOG
  Log(options_.info_log, 0, "Level-0 table #%llu: started",
      (unsigned long long) meta.number);
#endif
  WritableFile *file;
#ifdef KDEBUG
    uint64_t start, end;
    start = env_->NowMicros();
#endif

  Status s;
  {
    //mutex_.Unlock();
    s = BuildTable(dbname_, env_, options_, table_cache_, iter, &meta, &file );
    //mutex_.Lock();
  }
#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO BUILDTABLE (LOCK-UNLOCK) " << (end-start) << endl;
    start = end;
#endif
  pending_outputs_.erase(meta.number);

  // Note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  int level = 0;
  if (s.ok() && meta.file_size > 0) {
    const Slice min_user_key = meta.smallest.user_key();
    const Slice max_user_key = meta.largest.user_key();
    if (base != NULL) {
      if (min_user_key.compare(end_key_slice_) > 0) {
        SetEndKey(max_user_key);
        level = config::kMaxMemCompactLevel;
      } else {
        if (max_user_key.compare(end_key_slice_) > 0) {
          SetEndKey(max_user_key);
        }
        level = base->PickLevelForMemTableOutput(min_user_key, max_user_key);
      }
    }
    // Clear largest if it is added to level > 0
    if (level > 0) {
      meta.largest.Clear();
    }
    meta.level = level;

#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO PICK LEVEL " << (end-start) << endl;
    start = end;
#endif

    // Finish and check for file errors
    if (s.ok()) {
      s = file->Sync();
    }
#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO SYNC 1 " << (end-start) << endl;
    start = end;
#endif

    delete file;

#ifdef KDEBUG  //Keep this for debug purpose.
    if (s.ok()) {
      // Verify that the table is usable
      Log(options_.info_log, 0, "Verify that table %llu is usable",
        meta.number);

      Iterator* it = table_cache_->NewIterator(ReadOptions(),
                                              meta.number,
                                              meta.file_size, 0);
      s = it->status();
      delete it;
    }
    if (s.ok() && meta.file_size > 0) {
    // Keep it
    } else {
      env_->DeleteFile(TableFileName(dbname_, meta.number));
      Log(options_.info_log, 0, " Table %llu is bad ", meta.number);
    }
#endif

#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO DELETE/CLOSE " << (end-start) << endl;
    start = end;
#endif

    file = NULL;
#ifndef NLOG
    Log(options_.info_log, 0, "Real Level %d table #%llu: %lld bytes %s",
       meta.level,
      (unsigned long long) meta.number,
      (unsigned long long) meta.file_size,
      s.ToString().c_str());
#endif
#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO FILL TABLE CACHE " << (end-start) << endl;
#endif

    // Check for input iterator errors
    if (!iter->status().ok()) {
     s = iter->status();
    }

    edit->AddFile(level, meta.number, meta.file_size,
                  meta.smallest, meta.largest);
  }

  CompactionStats stats;
  stats.micros = env_->NowMicros() - start_micros;
  stats.bytes_written = meta.file_size;
  stats_[level].Add(stats);
  delete iter;

  return s;
}
void DBImpl::ReleaseMem() {
    current_logmem_->clear();
    delete current_arena_for_log_;
    current_logmem_ = &logmem_[logmem_in_use_];
    current_arena_for_log_ = new Arena;
    mem_->Unref();
    mem_ = new MemTable(internal_comparator_);
    mem_->Ref();
}


void DBImpl::ReleaseImmu() {
    // Commit to the new state
    if (logmem_imm_ != NULL) {
      std::deque<struct BatchRecord *>::iterator itr;
      for(itr= logmem_imm_->begin(); itr != logmem_imm_->end(); ++itr) {
        log_->Clear(Slice(*itr));
        delete *itr;
      }
      if(arena_for_log_imm_ != NULL) {
          delete arena_for_log_imm_;
          arena_for_log_imm_ = NULL;
      }
      logmem_imm_->clear();
      logmem_imm_->shrink_to_fit();
      logmem_imm_ = NULL;
      if(log_ != NULL ) {
        delete log_;
        log_ = NULL;
      }
      if (logfile_ != NULL) {
        logfile_->Close();
        persistedLogFile_ = logfile_;
        logfile_ = NULL;
      }
    }
    imm_->Unref();
    imm_ = NULL;
    has_imm_.Release_Store(NULL);
}
Status DBImpl::CompactMemTable() {
  mutex_.AssertHeld();
  if (env_->numberOfGoodSuperblocks() == 0) {
      return Status::Corruption("There is no good superblock");
  }
  if (env_->numberOfGoodSuperblocks() == 1) {
      Status status = env_->Sync();
      if (!status.ok()) {
          ReleaseImmu();
          // Error occured failed to persist to disk
          // send failure status to pending commands
          RecordBackgroundError(status);
          SendCmdListStatus(options_.outstanding_status_sender, false, imm_token_list_);
          imm_token_list_.clear();
          this->ReleaseMem();
          SendCmdListStatus(options_.outstanding_status_sender, false, mem_token_list_);
          mem_token_list_.clear();
          return status;
      } else {
          RecordBackgroundError(status);
      }
  }
  assert(imm_ != NULL);

#ifdef KDEBUG
    cout << "****START TO COMPACTMEM " << endl;
    uint64_t start, start1, end;
    start = env_->NowMicros();
    start1 = start;
#endif

  // Save the contents of the memtable as a new Table
  VersionEdit edit;
  Version* base = versions_->current();
  Status s = WriteLevel0Table(imm_, &edit, base);
  base->Unref();
#ifdef KDEBUG
  if(s.ok()) {
    end = env_->NowMicros();
    Log(options_.info_log, 0, " TIME TO WRITEL0TABLE %lld", (end-start));
    start = end;
  }
#endif
  // Replace immutable memtable with the generated Table
  if (s.ok()) {
      if (logfile_number_ > versions_->LogNumber()) {
          edit.SetPrevLogNumber(versions_->LogNumber());
          edit.SetLogNumber(logfile_number_);  // Earlier logs no longer needed
      }
      set<uint64_t> obsoleteValueFiles;
      env_->GetObsoleteValueFiles(obsoleteValueFiles);
      versions_->SubtractObsoletedValueFiles(obsoleteValueFiles);
      edit.ObsoleteValueFile(obsoleteValueFiles);
      s = versions_->LogAndApply(&edit, &mutex_);
      if (s.ok()) {
          DeleteObsoleteFiles(&edit);
      }
  }
#ifdef KDEBUG
  if(s.ok()) {
    end = env_->NowMicros();
    Log(options_.info_log, 0, "TIME LOGANDAPPLY & DELETEFILES %ld",(end-start));
      if ((end-start) >= (uint64_t)2000000) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__
           << "TIME LOGANDAPPLY & DELETEFILES " << (end-start) << " us";
        Status::IOError(ss.str());
        start = end;
      }
    }
#endif
#ifndef NLOG
  VersionSet::LevelSummaryStorage tmp;
  Log(options_.info_log, 0, "Level Summary: %s", versions_->LevelSummary(&tmp));
#endif
  ReleaseImmu();
  if (s.ok()) {
    // Commit to the new state
#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO DELETEVALUEPOINTER " << (end-start) << endl;
    start = end;
#endif
    SendCmdListStatus(options_.outstanding_status_sender, true, imm_token_list_);
  } else {
#ifdef KDEBUG
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << s.ToString() << endl;
#endif  //KDEBUG
    RecordBackgroundError(s);
    // Error occured failed to persist to disk
    // send failure status to pending commands
    SendCmdListStatus(options_.outstanding_status_sender, false, imm_token_list_);
  }
  imm_token_list_.clear();
#ifdef KDEBUG
    end = env_->NowMicros();
    cout << "   TIME TO DELETEOBSOLETEFILES " << (end-start) << endl;
    cout << "****END TO COMPACTMEM " << (end-start1) << endl;

#endif
    return s;
}

void DBImpl::CompactLevel(int level) {
  ManualCompaction manual;
  manual.level = level;
  manual.begin = NULL;
  manual.end = NULL;
  manual.done = false;
  MutexLock l(&mutex_);
  if (manual_compaction_ != NULL) {
    return;
  }
  while (!manual.done && !shutting_down_.Acquire_Load() && bg_error_.ok()) {
    if (manual_compaction_ == NULL) {
      manual_compaction_ = &manual;
      MaybeScheduleCompaction();
    } else {  // Running either my compaction or another compaction.
      bg_cv_.Wait();
    }
  }
  if (manual_compaction_ == &manual) {
    // Cancel my manual compaction since we aborted early for some reason.
    manual_compaction_ = NULL;
  }
}

void DBImpl::CompactRange(const Slice* begin, const Slice* end) {
  int max_level_with_files = 1;
  {
    MutexLock l(&mutex_);
    Version* base = versions_->current();
    for (int level = 1; level < config::kNumLevels; level++) {
      if (base->OverlapInLevel(level, begin, end)) {
        max_level_with_files = level;
      }
    }
    base->Unref();
  }
  for (int level = 0; level < max_level_with_files; level++) {
    TEST_CompactRange(level, begin, end);
  }
}

void DBImpl::TEST_CompactRange(int level, const Slice* begin,const Slice* end) {
  assert(level >= 0);
  assert(level + 1 < config::kNumLevels);

  InternalKey begin_storage, end_storage;

  ManualCompaction manual;
  manual.level = level;
  manual.done = false;
  if (begin == NULL) {
    manual.begin = NULL;
  } else {
    begin_storage = InternalKey(*begin, kMaxSequenceNumber, kValueTypeForSeek);
    manual.begin = &begin_storage;
  }
  if (end == NULL) {
    manual.end = NULL;
  } else {
    end_storage = InternalKey(*end, 0, static_cast<ValueType>(0));
    manual.end = &end_storage;
  }

  MutexLock l(&mutex_);
  while (!manual.done && !shutting_down_.Acquire_Load() && bg_error_.ok()) {
    if (manual_compaction_ == NULL) {  // Idle
      manual_compaction_ = &manual;
      MaybeScheduleCompaction();
    } else {  // Running either my compaction or another compaction.
      bg_cv_.Wait();
    }
  }
  if (manual_compaction_ == &manual) {
    // Cancel my manual compaction since we aborted early for some reason.
    manual_compaction_ = NULL;
  }
}

Status DBImpl::TEST_CompactMemTable() {
  // NULL batch means just wait for earlier writes to be done
  MutexLock lock(&mutex_);
  Status s = Write(WriteOptions(), NULL);
  if (s.ok()) {
    // Wait until the compaction completes
  //  MutexLock l(&mutex_);
    while (imm_ != NULL && bg_error_.ok()) {
      bg_cv_.Wait();
    }
    if (imm_ != NULL) {
      s = bg_error_;
    }
  }
  return s;
}

void DBImpl::RecordBackgroundError(const Status& s) {
  mutex_.AssertHeld();
  if (s.IsNotAttempted()) {
      return;
  }
  if (bg_error_.ok() || bg_error_.IsSuperblockIO()) {
    bg_error_ = s;
    bg_cv_.SignalAll();
  }
}

void DBImpl::MaybeScheduleCompaction() {
  mutex_.AssertHeld();
  static int bgCounter = 0;
  static int defragCounter = -1;
  if (defragCounter == -1 && !shutting_down_.Acquire_Load()) {
      defragCounter = 0;
      // Call schedule to create threads in advance
      valueDefragmentScheduled_ = true;
      env_->ScheduleDefrag(&DBImpl::BGDefragValue, this, NULL);
  }

  if (bg_compaction_scheduled_ || sstDefragmentScheduled_) {
  } else if (!bg_error_.ok() && !bg_error_.IsSuperblockIO() && !bg_error_.IsNoSpaceAvailable()) {
  } else if (valueDefragmentScheduled_) {
      // Schedule bg compaction only
      bg_compaction_scheduled_ = true;
      env_->Schedule(&DBImpl::BGWork, this, NULL);
  } else if (shutting_down_.Acquire_Load()) {
      Log(options_.info_log, 0, "Shutting down...");
      bg_cv_.SignalAll();
  } else {
      // Alternate among the 3 background tasks
      if (bgCounter == 0 && env_->IsHighDiskUsage() && env_->numberOfGoodSuperblocks() > 1) {
          bgCounter = 1;
          if (defragCounter == 1 && env_->IsFragmented() && kineticd_idle) {
              defragCounter = 0;
              sstDefragmentScheduled_ = true;
              env_->ScheduleDefrag(&DBImpl::BGDefragmentSST, this, NULL);
          } else {
              if (kineticd_idle) {
                  defragCounter = 1;
                  valueDefragmentScheduled_ = true;
                  env_->ScheduleDefrag(&DBImpl::BGDefragValue, this, NULL);
              }
          }
      } else if (imm_ != NULL || manual_compaction_ != NULL || versions_->NeedsCompaction()) {
          bgCounter = 0;
          bg_compaction_scheduled_ = true;
          env_->Schedule(&DBImpl::BGWork, this, NULL);
      } else {
          if ((kineticd_idle || env_->IsHighDiskUsage()) && env_->numberOfGoodSuperblocks() > 1) {
              if (defragCounter == 1 && env_->IsFragmented()) {
                  bgCounter = 1;
                  defragCounter = 0;
                  sstDefragmentScheduled_ = true;
                  env_->ScheduleDefrag(&DBImpl::BGDefragmentSST, this, NULL);
              } else if (env_->IsValueFragmented()) {
                  bgCounter = 1;
                  defragCounter = 1;
                  valueDefragmentScheduled_ = true;
                  env_->ScheduleDefrag(&DBImpl::BGDefragValue, this, NULL);
              } else {
                  bgCounter = 0;
                  versions_->ForceCompaction();
                  if (versions_->NeedsCompaction()) {
                      bg_compaction_scheduled_ = true;
                      env_->Schedule(&DBImpl::BGWork, this, NULL);
                  }
              }
          }
      }
  }
}
void DBImpl::BGDefragValue(void* db) {
    reinterpret_cast<DBImpl*>(db)->DefragmentValue();
}
void DBImpl::DefragmentValue() {
    MutexLock l(&mutex_);
    if (shutting_down_.Acquire_Load()) {
        valueDefragmentScheduled_ = false;
        bg_cv_.SignalAll();
        return;
    }
    Env::put_func_t putFunc = std::bind(
        &DBImpl::PutByInternal, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3
    );
   mutex_.Unlock();
   Status status = env_->DefragmentExternal(options_, putFunc);

    // If no bg compaction schedule, delete obsolete value files
    // else leave the delete work to bg compaction.
    mutex_.Lock();
    if (!status.ok() && !status.IsNotAttempted()) {
      RecordBackgroundError(status);
    }
    valueDefragmentScheduled_ = false;
    MaybeScheduleCompaction();
    bg_cv_.SignalAll();
}

void DBImpl::BGDefragmentSST(void* db) {
    reinterpret_cast<DBImpl*>(db)->DefragmentSST();
}

void DBImpl::DefragmentSST() {
    Log(options_.info_log, 0, "Defragmenting...");
    MutexLock l(&mutex_);
    if (shutting_down_.Acquire_Load()) {
        sstDefragmentScheduled_ = false;
        bg_cv_.SignalAll();
        return;
    }
    mutex_.Unlock();
    Status status = env_->Defragment(0);
    mutex_.Lock();
    if (status.ok()) {
        // Do nothing
    } else if (status.IsNotAttempted()) {
        status = Status::OK();
    } else if (status.IsNoSpaceAvailable()) {
        Status s;
        stringstream context;
        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Cannot free up zones";
        s = Status::Frozen(context.str());
        RecordBackgroundError(s);
    } else if (status.IsCorruption() || status.IsSuperblockIO()) {
        RecordBackgroundError(status);
    }
    versions_->NotifyDefragmentComplete();
    Log(options_.info_log, 0, "Defragment completes");
    sstDefragmentScheduled_ = false;
    MaybeScheduleCompaction();
    bg_cv_.SignalAll();
}

void DBImpl::BGSchedule() {
    MutexLock l(&mutex_);
    MaybeScheduleCompaction();
}

void DBImpl::BGWork(void* db) {
  reinterpret_cast<DBImpl*>(db)->BackgroundCall();
}

void DBImpl::BackgroundCall() {
      MutexLock l(&mutex_);
      assert(bg_compaction_scheduled_);
      if (shutting_down_.Acquire_Load() && !valueDefragmentScheduled_) {
         bg_compaction_scheduled_ = false;
         bg_cv_.SignalAll();
         return;
          // No more background work when shutting down.
      } else if (!bg_error_.ok() && !bg_error_.IsSuperblockIO() && !bg_error_.IsNoSpaceAvailable()) {
        // No more background work after a background error.
          bg_compaction_scheduled_ = false;
      } else {
          Status status = BackgroundCompaction();
          bg_compaction_scheduled_ = false;
          if (status.IsSuperblockIO() && imm_ == NULL) {
              // Do not schedule compaction.  Let idle compaction and compact memtable reschedule
          } else {
              MaybeScheduleCompaction();
          }
      }
      bg_cv_.SignalAll();
}

Status DBImpl::BackgroundCompaction() {
  mutex_.AssertHeld();
  bool nothingToDo = false;

  Status status;
  if (imm_ != NULL) {
      if (HasSpaceForMemCompaction()) {
          status = CompactMemTable();
          bg_cv_.SignalAll();  // Wakeup MakeRoomForWrite() if necessary
          if (!status.ok()) {
              return status;
          }
      } else {
          status = Status::NoSpaceAvailable("Cannot compact memtable");
          return status;
      }
  }
  if (versions_->IsNoSpaceForAllLevelCompaction()) {
      Log(options_.info_log, 2, "No space to compact");
      return status;
  }

  if (env_->numberOfGoodSuperblocks() == 1) {
      status = this->env_->Sync();
      if (!status.ok()) {
          RecordBackgroundError(status);
          return status;
      } else {
          RecordBackgroundError(status);
      }
  }

  Compaction* c;
  bool is_manual = (manual_compaction_ != NULL);
  InternalKey manual_end;
  if (is_manual) {
    ManualCompaction* m = manual_compaction_;
    c = versions_->CompactRange(m->level, m->begin, m->end);
    m->done = (c == NULL);
    if (c != NULL) {
      manual_end = c->Input0LargestKey();
    }
    Log(options_.info_log, 5,
        "Manual compaction at level-%d from %s .. %s\n",
        m->level,
        (m->begin ? m->begin->DebugString().c_str() : "(begin)"),
        (m->end ? m->end->DebugString().c_str() : "(end)"));
  } else {
       c = versions_->PickCompaction();
  }
  if (c == NULL) {
    // Nothing to do
  } else if (!is_manual && c->IsTrivialMove() && c->level() > 0) {
    // Move file to next level
    assert(c->num_input_files(0) == 1);
    FileMetaData* f = c->input(0, 0);
    c->edit()->DeleteFile(c->level(), f->number);
    // Clear largest as it is added to level > 0
    f->largest.Clear();
    c->edit()->AddFile(c->level() + 1, f->number, f->file_size,
                       f->smallest, f->largest);
    set<uint64_t> obsoleteValueFiles;
    env_->GetObsoleteValueFiles(obsoleteValueFiles);
    versions_->SubtractObsoletedValueFiles(obsoleteValueFiles);
    c->edit()->ObsoleteValueFile(obsoleteValueFiles);
    status = versions_->LogAndApply(c->edit(), &mutex_);
    if (status.ok()) {
        DeleteObsoleteFiles(c->edit());
    }
    if (!status.ok()) {
      RecordBackgroundError(status);
    }
    VersionSet::LevelSummaryStorage tmp;
#ifndef NLOG
    Log(options_.info_log, 0, "Moved #%lld to level-%d %lld bytes %s: %s\n",
        static_cast<unsigned long long>(f->number),
        c->level() + 1,
        static_cast<unsigned long long>(f->file_size),
        status.ToString().c_str(),
        versions_->LevelSummary(&tmp));
#endif
  } else {
      CompactionState* compact = new CompactionState(c);
      bool obsoleteDeleted = false;
      status = DoCompactionWork(compact);
      if(status.ok() && compact->deleter) {
          // If external data has been marked for deletion, deletions may only be applied
          // after compaction has completed successfully.
          // A power cycle will waste disk capacity until the value file is defragmented
          // at some point (defrag will garbage collect).
         // compact->deleter->Finalize();
      }
      if (status.IsNotAttempted()) {
      } else if (status.IsNoSpaceAvailable()) {
          versions_->RevertCompactPointer(c->level());
          if (c->num_input_files(0) == 1) {
              versions_->AddFailedCompactionLevel(c->level(), versions_->NumLevelFiles(c->level() + 1));
          }
          if (c->num_input_files(0) > 0) {
              versions_->SetCompactPointer(c->level(), c->input(0, 0)->smallest);
          }
          if (c->num_input_files(1) > 0) {
              versions_->SetCompactPointer(c->level() + 1, c->input(1, 0)->smallest);
          }
          if (compact->builder != NULL) {
              compact->builder->Abandon();
              delete compact->builder;
              compact->builder = NULL;
          }
          delete compact->outfile;
          compact->outfile = NULL;
          uint64_t spaceNeeds = 0;
          if (compact->outfile) {
              spaceNeeds = compact->outfile->GetSize();
          }
          for (size_t i = 0; i < compact->outputs.size(); i++) {
            const CompactionState::Output& out = compact->outputs[i];
            spaceNeeds += out.file_size;
            c->edit()->DeleteFile(c->level() + 1, out.number);
            pending_outputs_.erase(out.number);
          }

          compact->outputs.clear();
          DeleteObsoleteFiles(c->edit());
          c->edit()->GetDeletedFiles()->clear();
          obsoleteDeleted = true;

          versions_->RevertCompactPointer(c->level());
          if (c->num_input_files(0) == 1) {
              versions_->AddFailedCompactionLevel(c->level(), spaceNeeds);
          }
          if (c->num_input_files(0) > 0) {
              versions_->SetCompactPointer(c->level(), c->input(0, 0)->smallest);
          }
          if (c->num_input_files(1) > 0) {
              versions_->SetCompactPointer(c->level() + 1, c->input(1, 0)->smallest);
          }
          if (c->level() > 0) {
              if (versions_->CanLevelExpand(c->level()) && c->num_input_files(0) > 1) {
                  versions_->RemoveFailedCompactionLevel(c->level());
              } else {
                  // Split the first input file of the compaction level, input[0]
                  FileMetaData* firstInFile = c->input(0, 0);
                  c->ClearInputs();
                  c->AddInput(firstInFile, 0);
                  status = SplitTable(firstInFile, compact);
                  if (status.ok()) {
                      versions_->RemoveFailedCompactionLevel(c->level());
                      obsoleteDeleted = false;
                  } else if (status.IsNotAttempted()) {
                  } else {
                      obsoleteDeleted = false;
                  }
              }
          }
          versions_->SetNoExpandLevel(c->level());
      } else {
          versions_->SetExpandLevel(c->level());
          versions_->RemoveFailedCompactionLevel(c->level());
          versions_->RemoveFailedCompactionLevel(c->level()-1);
      }
      if (!status.ok() && !status.IsNotAttempted()) {
          RecordBackgroundError(status);
      }
      CleanupCompaction(compact);
      c->ReleaseInputs();
      if (obsoleteDeleted == false) {
          DeleteObsoleteFiles(c->edit());
      }
  }
  delete c;

  if (status.ok()) {
    // Done
  } else if (status.IsNotAttempted()) {
      status = Status::OK();
  } else if (shutting_down_.Acquire_Load() && !valueDefragmentScheduled_) {
    // Ignore compaction errors found during shutting down
  } else {
    Log(options_.info_log, 2, "Compaction error: %s", status.ToString().c_str());
  }

  if (is_manual) {
    ManualCompaction* m = manual_compaction_;
    if (!status.ok()) {
      m->done = true;
    }
    if (!m->done) {
      // We only compacted part of the requested range.  Update *m
      // to the range that is left to be compacted.
      m->tmp_storage = manual_end;
      m->begin = &m->tmp_storage;
    }
    manual_compaction_ = NULL;
  }
  if (bg_error_.IsNoSpaceAvailable()) {
      versions_->PrintCompactStatus();
  }
  return status;
}
Status DBImpl::SplitTable(FileMetaData* sourceFileMetaData, CompactionState* compact) {
    // Create iterator from the source table
    ReadOptions options;
    options.verify_checksums = options_.paranoid_checks;
    options.fill_cache = false;
    Table* srcTable = NULL;
    Iterator* iter = table_cache_->NewIterator(options, sourceFileMetaData->number,
            sourceFileMetaData->file_size, sourceFileMetaData->level, &srcTable);
    Status status = iter->status();
    std::string srcFname = TableFileName(dbname_, sourceFileMetaData->number, sourceFileMetaData->level);
    if (status.ok() && srcTable->NumKeys() < 2) {
         // We can't split a less than 2 keys SST.  Compaction won't be executed
         delete iter;
         Log(options_.info_log, 1, "Cannot split 1-key");
         status = Status::NotAttempted("Compaction", "Cannot split 1-key");
         return status;
    }
    Log(options_.info_log, 5, "Splitting %s", srcFname.c_str());
    VersionSet::LevelSummaryStorage tmp;
    Log(options_.info_log, 5, "Initial files: %s", versions_->LevelSummary(&tmp));
    if (status.ok()) {
        iter->SeekToFirst();
    } else {
        string statusStr = status.ToString();
        // This will go to customer facing log so it must be vague to protect propriety information
        Log(options_.info_log, 2, "Failed to split: bad source.");
        // Verbose message that will only show up for verbosity levels 1+
        Log(options_.info_log, 5, "Failed to split %s: %s.  Bad source table.", srcFname.c_str(), statusStr.c_str());
        delete iter;
        return status;
    }
    // Determine number of new tables.  Maximnum number of new tables is 10.
    // In the worst case, there will be log(n) base 10 number of new files created
    // from splitting an SST.  n is the number of keys in the SST.
    int nNewTables = 10;
    if (srcTable->NumKeys() < nNewTables) {
        nNewTables = srcTable->NumKeys();
    }
    uint64_t nKeysToMigrate = srcTable->NumKeys()/nNewTables;
    // Looping through nNewTables time to create nNewTables of SSTs.
    for (int i = 0; iter->Valid() && i < nNewTables && status.ok(); ++i) {
        FileMetaData destFileMetaData;
        destFileMetaData.level = sourceFileMetaData->level;
        destFileMetaData.number = versions_->NewFileNumber();
        uint64_t file_number;
        pending_outputs_.insert(destFileMetaData.number);
        CompactionState::Output out;
        out.number = destFileMetaData.number;
        out.smallest.Clear();
        out.largest.Clear();
        compact->outputs.push_back(out);
        std::string fname = TableFileName(dbname_, destFileMetaData.number, destFileMetaData.level);
        status = env_->NewWritableFile(fname, &compact->outfile);
        if (status.ok()) {
          compact->builder = new TableBuilder(options_, compact->outfile);
        } else {
          break;
        }
        if (i + 1 == nNewTables) {
            // Last file has the rest of keys from the source splitted table
            nKeysToMigrate = -1;
        }
        // Append keys/values to new table
        for (int j = 0; (j < nKeysToMigrate || nKeysToMigrate == -1) && iter->Valid(); ++j) {
            if (compact->builder->NumEntries() == 0) {
               compact->current_output()->smallest.DecodeFrom(iter->key());
            }
           compact->builder->Add(iter->key(), iter->value());
           iter->Next();
        }
        status = FinishCompactionOutputFile(compact, iter);
    }
    delete iter;
    if (status.ok()) {
        compact->compaction->AddInputDeletions(compact->compaction->edit());
        const int level = sourceFileMetaData->level;
        for (size_t i = 0; i < compact->outputs.size(); ++i) {
            CompactionState::Output& out = compact->outputs[i];
            // Clear largest as it is added to level > 0
            out.largest.Clear();
            compact->compaction->edit()->AddFile(level,out.number, out.file_size, out.smallest,
                out.largest);
        }
        InternalKey key;
        key.DecodeFrom(versions_->GetPrevCompactPointer(level));
        compact->compaction->edit()->SetCompactPointer(level, key);
        versions_->RevertCompactPointer(level);
        status = versions_->LogAndApply(compact->compaction->edit(), &mutex_);
    }
    if (status.ok()) {
        Log(options_.info_log, 5, "Split %s@%d into %lu@%d files => %llu bytes",
            srcFname.c_str(),
            compact->compaction->level(),
            compact->outputs.size(),
            compact->compaction->level(),
            static_cast<long long>(compact->total_bytes));
        Log(options_.info_log, 5,
            "Result files: %s", versions_->LevelSummary(&tmp));
    } else {
        // This will go to customer facing log so it must be vague to protect propriety information
        Log(options_.info_log, 2, "Failed to split");
        string statusStr = status.ToString();
        // Verbose message that will only show up for verbosity levels 1+
        Log(options_.info_log, 5, "%s: Failed to split %s: ", srcFname.c_str(), statusStr.c_str());
    }
    return status;
}
void DBImpl::CleanupCompaction(CompactionState* compact) {
  mutex_.AssertHeld();
  if (compact->builder != NULL) {
    // May happen if we get a shutdown call in the middle of compaction
    compact->builder->Abandon();
    delete compact->builder;
  } else {
    assert(compact->outfile == NULL);
  }
  if (compact->deleter != NULL) {
    // No need to clean up deleter.
    delete compact->deleter;
  }
  delete compact->outfile;
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const CompactionState::Output& out = compact->outputs[i];
    pending_outputs_.erase(out.number);
  }
  delete compact;
}

Status DBImpl::OpenCompactionOutputFile(CompactionState* compact) {
  assert(compact != NULL);
  assert(compact->builder == NULL);
  uint64_t file_number;
  {
    mutex_.Lock();
    file_number = versions_->NewFileNumber();
    pending_outputs_.insert(file_number);
    CompactionState::Output out;
    out.number = file_number;
    out.file_size = 0;
    out.smallest.Clear();
    out.largest.Clear();
    compact->outputs.push_back(out);
    mutex_.Unlock();
  }

  // Make the output file
  std::string fname = TableFileName(dbname_, file_number, compact->compaction->level()+1);
  Status s = env_->NewWritableFile(fname, &compact->outfile);
  if (s.ok()) {
    compact->builder = new TableBuilder(options_, compact->outfile);
  }
  return s;
}

Status DBImpl::FinishCompactionOutputFile(CompactionState* compact,
                                          Iterator* input) {
  assert(compact != NULL);
  assert(compact->outfile != NULL);
  assert(compact->builder != NULL);

  const uint64_t output_number = compact->current_output()->number;
  assert(output_number != 0);

  // Check for iterator errors
  Status s = input->status();
  const uint64_t current_entries = compact->builder->NumEntries();
  if (s.ok()) {
    s = compact->builder->Finish();
  } else {
#ifdef KDEBUG
    cout << " ABANDON" << endl;
#endif
    compact->builder->Abandon();
  }
  const uint64_t current_bytes = compact->builder->FileSize();
  compact->current_output()->file_size = current_bytes;
  compact->total_bytes += current_bytes;
  delete compact->builder;
  compact->builder = NULL;
// For testing power cycles  abort();
//  abort();
  // Finish and check for file errors
  if (s.ok()) {
    s = compact->outfile->Sync();
  }
// DO NOT DELETE THE NEXT LINE. For testing power cycles  abort();
//  abort();
  if (s.ok()) {
    s = compact->outfile->Close();
    if(!s.ok()) {
        #ifdef KDEBUG
        cout << " ABANDON.2" << endl;
        #endif // KDEBUG
    }
  } else {
      #ifdef KDEBUG
      cout << " ABANDON.1" << endl;
      #endif // KDEBUG
  }
  delete compact->outfile;
  compact->outfile = NULL;

  if (s.ok() && current_entries > 0) {
    // Verify that the table is usable
    // Assume this is a split. So, output file level is initialized to
    // low input level compact->compaction->level()
    int outLevel = compact->compaction->level();
    if (compact->compaction->num_input_files(1) > 0) {
        // This is not a split operation.  So, output level is at the next level
        ++outLevel;
    }
    Iterator* iter = table_cache_->NewIterator(ReadOptions(),
                                               output_number,
                                               current_bytes,
                                               outLevel);
    s = iter->status();
    delete iter;

    if (s.ok()) {
#ifndef NLOG
      Log(options_.info_log, 0,
          "Generated table #%llu: %lld keys, %lld bytes",
          (unsigned long long) output_number,
          (unsigned long long) current_entries,
          (unsigned long long) current_bytes);
#endif
    } else {
      Log(options_.info_log, 1, "%llu is bad ", (unsigned long long) output_number);
    }
  }
  return s;
}


Status DBImpl::InstallCompactionResults(CompactionState* compact) {
  mutex_.AssertHeld();
  // This will go to customer facing log so it must be vague to protect propriety information
#ifndef NLOG
  Log(options_.info_log, 0, "Compacted %d:%d - %d:%d => %lld",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1,
      static_cast<long long>(compact->total_bytes));
#endif
  // Add compaction outputs
  compact->compaction->AddInputDeletions(compact->compaction->edit());
  const int level = compact->compaction->level();
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    CompactionState::Output& out = compact->outputs[i];
    // Clear largest as it is added to level > 0
    out.largest.Clear();
    compact->compaction->edit()->AddFile(
        level + 1,
        out.number, out.file_size, out.smallest, out.largest);
  }
  if (compact->deleter) {
      compact->deleter->Finalize(compact->compaction->edit());
  }
  return versions_->LogAndApply(compact->compaction->edit(), &mutex_);
}

Status DBImpl::DoCompactionWork(CompactionState* compact) {
  if (compact->compaction->level() < 0) {
      return Status::NotAttempted("Negative compaction level");
  }
  const uint64_t start_micros = env_->NowMicros();
  int64_t imm_micros = 0;  // Micros spent doing imm_ compactions
#ifndef NLOG
  Log(options_.info_log, 0, "Compacting %d@%d + %d@%d files",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1);
#endif
#ifdef KDEBUG
  Log(options_.info_log, 0, "   At level %d", compact->compaction->level());
  for (int i = 0 ; i < compact->compaction->num_input_files(0); ++i) {
      Log(options_.info_log, 0, "     Compacting files %lu ", compact->compaction->input(0,i)->number);
  }
  Log(options_.info_log, 0, "   At level %d", compact->compaction->level()+1);

  for (int i = 0; i < compact->compaction->num_input_files(1); ++i) {
      Log(options_.info_log, 0, "     Compacting files %lu ", compact->compaction->input(1,i)->number);
  }
#endif
  assert(versions_->NumLevelFiles(compact->compaction->level()) > 0);
  assert(compact->builder == NULL);
  assert(compact->outfile == NULL);
  if (snapshots_.empty()) {
    compact->smallest_snapshot = versions_->LastSequence();
  } else {
    compact->smallest_snapshot = snapshots_.oldest()->number_;
  }

  compact->compaction->GetLevelLargest();
  // Release mutex while we're actually doing the compaction work
  mutex_.Unlock();
  Iterator* input = versions_->MakeInputIterator(compact->compaction);
  input->SeekToFirst();
  Status status;
  ParsedInternalKey ikey;
  std::string current_user_key;
  bool has_current_user_key = false;
  SequenceNumber last_sequence_for_key = kMaxSequenceNumber;

  for (; input->Valid() && !IsCorrupted();) {
    // Prioritize immutable compaction work
    if (has_imm_.NoBarrier_Load() != NULL) {
        const uint64_t imm_start = env_->NowMicros();
        mutex_.Lock();
        if (imm_ != NULL) {
            if (HasSpaceForMemCompaction()) {
                status = CompactMemTable();
                bg_cv_.SignalAll();  // Wakeup MakeRoomForWrite() if necessary
                if (!status.ok()) {
                    mutex_.Unlock();
                    break;
                }
            }
        }
        mutex_.Unlock();
        imm_micros += (env_->NowMicros() - imm_start);
    }

    Slice key = input->key();
    if (compact->compaction->ShouldStopBefore(key) && compact->builder != NULL) {
        status = FinishCompactionOutputFile(compact, input);
        if (!status.ok()) {
            break;
        }
    }
    // Handle key/value, add to state, etc.
    LevelDBData myData;
    bool drop = false;
    if (!ParseInternalKey(key, &ikey)) {
        // Do not hide error keys
        current_user_key.clear();
        has_current_user_key = false;
        last_sequence_for_key = kMaxSequenceNumber;
    } else {
        if (!has_current_user_key || user_comparator()->Compare(ikey.user_key, Slice(current_user_key)) != 0) {
            // First occurrence of this user key
            current_user_key.assign(ikey.user_key.data(), ikey.user_key.size());
            has_current_user_key = true;
            last_sequence_for_key = kMaxSequenceNumber;
        }

        if (last_sequence_for_key <= compact->smallest_snapshot) {
            // Hidden by an newer entry for same user key
            LevelLogEvent(options_.info_log, compact->compaction->level() + 1);
            // If value is stored externally, mark it as deleted
            if (ikey.type == kTypeValue) {
                const LevelDBData* p = (const LevelDBData*) input->value().data();
                if (p && p->type == LevelDBDataType::SERIALIZED_EXTERNAL) {
                    if(!compact->deleter) {
                        status = env_->NewExternalValueDeleter(options_.info_log, dbname_, &compact->deleter);;
                        if (!status.ok())
                            break;
                        }
                    if (myData.deserialize(input->value().data())) {
                        compact->deleter->Add(myData.data);
                    }
                }
            }
        drop = true;    // (A)
        } else if (ikey.type == kTypeDeletion &&
                 ikey.sequence <= compact->smallest_snapshot &&
                 compact->compaction->IsBaseLevelForKey(ikey.user_key)) {
        // For this user key:
        // (1) there is no data in higher levels
        // (2) data in lower levels will have larger sequence numbers
        // (3) data in layers that are being compacted here and have
        //     smaller sequence numbers will be dropped in the next
        //     few iterations of this loop (by rule (A) above).
        // Therefore this deletion marker is obsolete and can be dropped.
            LevelLogEvent(options_.info_log, compact->compaction->level() + 1);
            drop = true;
        }

        last_sequence_for_key = ikey.sequence;
    }
    if (!drop) {
      // Open output file if necessary
      if (compact->builder == NULL) {
        status = OpenCompactionOutputFile(compact);
        if (!status.ok()) {
          #ifdef KDEBUG
          cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": OpenCompactionOutputFile failed" << endl;
          #endif // KDEBUG
          break;
        }
      }
      if (compact->builder->NumEntries() == 0) {
         compact->current_output()->smallest.DecodeFrom(key);
      }
      // Not storing largest for compaction output files as they are
      // in level > 0. (For end key removal)
      //compact->current_output()->largest.DecodeFrom(key);
      compact->builder->Add(key, input->value());
      status = compact->builder->status();
      // Close output file if it is big enough
      if (status.ok() && compact->builder->FileSize() >=
          compact->compaction->MaxOutputFileSize()) {
        status = FinishCompactionOutputFile(compact, input);
        if (!status.ok()) {
          break;
        }
      }
    }
    input->Next();
  }

  if (false) { //status.ok() && shutting_down_.Acquire_Load() && !valueDefragmentScheduled_) {
    status = Status::IOError("Deleting DB during compaction");
  }
  if (status.ok() && compact->builder != NULL) {
    status = FinishCompactionOutputFile(compact, input);
  }
  if (status.ok()) {
    status = input->status();
  }
  delete input;
  input = NULL;

  CompactionStats stats;
  stats.micros = env_->NowMicros() - start_micros - imm_micros;
  for (int which = 0; which < 2; which++) {
    for (int i = 0; i < compact->compaction->num_input_files(which); i++) {
      stats.bytes_read += compact->compaction->input(which, i)->file_size;
    }
  }
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    stats.bytes_written += compact->outputs[i].file_size;
  }

  mutex_.Lock();
  stats_[compact->compaction->level() + 1].Add(stats);

  if (status.ok()) {
    status = InstallCompactionResults(compact);
  }
  if (!status.ok()) {
    RecordBackgroundError(status);
  }
#ifndef NLOG
  VersionSet::LevelSummaryStorage tmp;
  Log(options_.info_log, 0,
      "compacted to: %s", versions_->LevelSummary(&tmp));
#endif
  return status;
}

namespace {
struct IterState {
  port::Mutex* mu;
  Version* version;
  MemTable* mem;
  MemTable* imm;
};

static void CleanupIteratorState(void* arg1, void* arg2) {
  IterState* state = reinterpret_cast<IterState*>(arg1);
  state->mu->Lock();
  state->mem->Unref();
  if (state->imm != NULL) state->imm->Unref();
  state->version->Unref();
  state->mu->Unlock();
  delete state;
}
}  // namespace

Iterator* DBImpl::NewInternalIterator(const ReadOptions& options,
                                      SequenceNumber* latest_snapshot,
                                      uint32_t* seed) {
  IterState* cleanup = new IterState;
  mutex_.Lock();
  *latest_snapshot = versions_->LastSequence();

  // Collect together all needed child iterators
  std::vector<Iterator*> list;
  list.push_back(mem_->NewIterator());
  mem_->Ref();
  if (imm_ != NULL) {
    list.push_back(imm_->NewIterator());
    imm_->Ref();
  }
  cleanup->version = versions_->current();
  cleanup->mu = &mutex_;
  cleanup->mem = mem_;
  cleanup->imm = imm_;
  cleanup->version->AddIterators(options, &list);
  Iterator* internal_iter =
      NewMergingIterator(&internal_comparator_, &list[0], list.size());
  internal_iter->RegisterCleanup(CleanupIteratorState, cleanup, NULL);

  *seed = ++seed_;

  mutex_.Unlock();
  return internal_iter;
}

Iterator* DBImpl::TEST_NewInternalIterator() {
  SequenceNumber ignored;
  uint32_t ignored_seed;
  return NewInternalIterator(ReadOptions(), &ignored, &ignored_seed);
}

int64_t DBImpl::TEST_MaxNextLevelOverlappingBytes() {
  MutexLock l(&mutex_);
  return versions_->MaxNextLevelOverlappingBytes();
}

namespace {
char* allocate_getvalue_buffer(bool byGetInternal) {
  char *buff = NULL;
  cout << "++++ byGetInternal = " << (byGetInternal? "True" : "False") << endl;
  if (!byGetInternal) {
  	buff = (char*) KernelMemMgr::pInstance_->AllocMem();
  } else {
        int s = posix_memalign((void**)&buff, 4096, ROUNDUP((size_t)5*1048576,4096));
	if (s != 0) {
	  cout << "FAILED TO ALLOC" << endl;
	  buff = NULL;
	}
  }
  return buff;
}

void deallocate_getvalue_buffer(bool byGetInternal, char* buff) {
  if (!byGetInternal) {
    KernelMemMgr::pInstance_->FreeMem((void*) buff);
  } else {
    free(buff);
  }
}
}

Status DBImpl::Get(const ReadOptions& options,
                   const Slice& key, char* value,
                   bool ignore_value, bool using_bloom_filter, char* buff) {
  Status s;
  bool fromMem = false;
  MutexLock l(&mutex_);
  SequenceNumber snapshot;
  if (options.snapshot != NULL) {
    snapshot = reinterpret_cast<const SnapshotImpl*>(options.snapshot)->number_;
  } else {
    snapshot = versions_->LastSequence();
  }

  MemTable* mem = mem_;
  MemTable* imm = imm_;
  Version* current = versions_->current();
  mem->Ref();
  if (imm != NULL) imm->Ref();

  bool have_stat_update = false;
  Version::GetStats stats;

  // Unlock while reading from files and memtables
  {
//    mutex_.Unlock();
    // First look in the memtable, then in the immutable memtable (if any).
    LookupKey lkey(key, snapshot);
    if (mem->Get(lkey, value, &s)) {
      fromMem = true;
    } else if (imm != NULL && imm->Get(lkey, value, &s)) {
      fromMem = true;
    } else {
      s = current->Get(options, lkey, value, &stats, using_bloom_filter);
      if (s.ok()) {
          have_stat_update = true;
      }

    }

    if (s.ok() ) {
     if (buff == NULL) {
        buff = allocate_getvalue_buffer(false);
        if (buff == NULL) {
        //mutex_.Lock(); // prevent double-unlock by MutexLock
          mem->Unref();
          if (imm != NULL) imm->Unref();
          current->Unref();
          return Status::IOError("CAN NOT ALLOC MEM ", "IN GET FROM MEM");
        }
     }
      char* value_pointer = NULL;
      memcpy((char*) &value_pointer, value, sizeof(void*));
      if (fromMem) {
        ((LevelDBData*) value_pointer)->serialize(buff);
      } else {
        LevelDBData myData;
        myData.deserialize(value_pointer);

        if (myData.type == LevelDBDataType::MEM_EXTERNAL) {
            ExternalValueInfo external;
            if (external.deserialize(myData.data)) {
                std::shared_ptr<RandomAccessFile> file;
                Status s = smr::CacheManager::cache(this->dbname_)->getReadable(external.file_number, file);
                if (s.ok()) {
                    Slice not_needed;
                    myData.type = LevelDBDataType::MEM_INTERNAL;
                    myData.dataSize = external.size;
                    char* data = myData.serialize(buff, NULL, true);
                    if (!ignore_value) {
                      s = file->Read(external.offset, external.size, &not_needed, data);
                    }
                    if (s.ok()) {
                        // TODO:  delete external info object.????  DON'T NEED TO ???
                        myData.data = data;
                    } else {
                        myData.data = NULL;
                    }
                }
            }
        } else {
            myData.serialize(buff);
        }
      }
      memcpy(value, (char*) &buff, sizeof(void*));
    }
 //   mutex_.Lock();  // In pair with previous Unlock
  }

  mem->Unref();
  if (imm != NULL) imm->Unref();
  current->Unref();

  if ((have_stat_update && current->UpdateStats(stats))) {
      MaybeScheduleCompaction();
  }
  return s;
}

Iterator* DBImpl::NewIterator(const ReadOptions& options) {
  SequenceNumber latest_snapshot;
  uint32_t seed;
  Iterator* iter = NewInternalIterator(options, &latest_snapshot, &seed);
  return NewDBIterator(
      this, user_comparator(), iter,
      (options.snapshot != NULL
       ? reinterpret_cast<const SnapshotImpl*>(options.snapshot)->number_
       : latest_snapshot),
      seed);
}

void DBImpl::RecordReadSample(Slice key) {
  MutexLock l(&mutex_);
  Version* version = versions_->current();
  if (version->RecordReadSample(key)) {
    MaybeScheduleCompaction();
  }
  version->Unref();
}

const Snapshot* DBImpl::GetSnapshot() {
  MutexLock l(&mutex_);
  return snapshots_.New(versions_->LastSequence());
}

void DBImpl::ReleaseSnapshot(const Snapshot* s) {
  MutexLock l(&mutex_);
  snapshots_.Delete(reinterpret_cast<const SnapshotImpl*>(s));
}

// Convenience methods
Status DBImpl::Put(const WriteOptions& o, const Slice& key, const Slice& val) {
#ifdef KDEBUG
  uint64_t start, end;
  start = env_->NowMicros();
#endif
  MutexLock l(&write_mutex_);

  Status s;
  s = DB::Put(o, key, val);

#ifdef KDEBUG
  end = env_->NowMicros();
#endif

  return s;
}

Status DBImpl::GetByInternal(const ReadOptions& options,const Slice& key,
                   char* value, bool using_bloom_filter, uint64_t* seqNum) {
    Status s;
    bool fromMem = false;
    MutexLock l(&mutex_);
    SequenceNumber snapshot;
    if (options.snapshot != NULL) {
        snapshot = reinterpret_cast<const SnapshotImpl*>(options.snapshot)->number_;
    } else {
        snapshot = versions_->LastSequence();
    }

    MemTable* mem = mem_;
    MemTable* imm = imm_;
    Version* current = versions_->current();
    mem->Ref();
    if (imm != NULL) imm->Ref();

    bool have_stat_update = false;
    Version::GetStats stats;

    // First look in the memtable, then in the immutable memtable (if any).
    LookupKey lkey(key, snapshot);
    if (mem->Get(lkey, value, &s, seqNum)) {
        fromMem = true;
    } else if (imm != NULL && imm->Get(lkey, value, &s, seqNum)) {
        fromMem = true;
    } else {
        s = current->Get(options, lkey, value, &stats, using_bloom_filter, seqNum);

        if (s.ok()) {
            have_stat_update = true;
        }
    }

    if (s.ok()) {
        char* buff = allocate_getvalue_buffer(true);
        if (buff == NULL) {
            mem->Unref();
            if (imm != NULL) imm->Unref();
            current->Unref();
            return Status::IOError("CAN NOT ALLOC MEM ", "IN GET FROM MEM");
        }
        char* value_pointer = NULL;
        memcpy((char*) &value_pointer, value, sizeof(void*));
        if (fromMem) {
            ((LevelDBData*) value_pointer)->serialize(buff);
        } else {
            LevelDBData myData;
            myData.deserialize(value_pointer);
            myData.serialize(buff);
        }
        memcpy(value, (char*) &buff, sizeof(void*));
    }
    mem->Unref();
    if (imm != NULL) imm->Unref();
    current->Unref();
    return s;
}

Status DBImpl::PutByInternal(const WriteOptions& option, const Slice& sInternalKey, const Slice& value) {
    Status status;
    WriteBatch batch;
    MutexLock lock(&write_mutex_);
    InternalKey internalKey;
    internalKey.DecodeFrom(sInternalKey);
    ParsedInternalKey parsedInternalKey;
    ::ParseInternalKey(sInternalKey, &parsedInternalKey);
    char* packed_value =  new char[sizeof(packed_value)];
    uint64_t seqNumber;
    status = GetByInternal(ReadOptions(), parsedInternalKey.user_key, packed_value, true, &seqNumber);
    // Compare using Comparator
    size_t ptr_size = sizeof(void*);
    int compare = -1;
    if (status.ok()) {
        char* value_pointer = NULL;
        memcpy((char*) &value_pointer, packed_value, ptr_size);
        if (parsedInternalKey.sequence >= seqNumber) {
	    //write_mutex_.Unlock();
            compare = option.value_comparator->Compare(value, Slice(value_pointer, ptr_size));
	    //write_mutex_.Lock();
        }
        deallocate_getvalue_buffer(true, value_pointer);
    }
    if (compare != 0) {
        delete[] packed_value;
        if(status.ok() || status.IsNotFound()) {
          return Status::NotAttempted("Failed value_comparator");
        }
        return status;
    }

    delete[] packed_value;
    batch.Put(parsedInternalKey.user_key, value);
    status = Write(option, &batch);
    return status;
}


Status DBImpl::Delete(const WriteOptions& options, const Slice& key) {
    MutexLock lock(&write_mutex_);
    if (!HasSpaceForDelCommand()) {
        return Status::NoSpaceAvailable("DELETE is temporarily unacceptable.");
    }
    return DB::Delete(options, key);
}

Status DBImpl::WriteBat(const WriteOptions& options, WriteBatch* my_batch, KineticMemory* memory) {
    MutexLock lock(&write_mutex_);
    return  Write(options, my_batch, memory);
}

Status DBImpl::Write(const WriteOptions& options, WriteBatch* my_batch, KineticMemory* memory) {
  // Only one write operation at a time. This lock is never taken by anyone else
  // and is not released until the write function completes (unlike the main
  // database mutex).
//  MutexLock l(&mutex_);
  Writer w(&mutex_);
  w.batch = my_batch;
  w.sync = options.sync;
  w.done = false;

  MutexLock l(&mutex_);
  writers_.push_back(&w);
  while (!w.done && &w != writers_.front()) {
    w.cv.Wait();
  }

  if (w.done) {
    return w.status;
  }

  // May temporarily unlock and wait.
  Status status;
  uint64_t last_sequence = versions_->LastSequence();
  Writer* last_writer = &w;
  if (status.ok() && my_batch != NULL) {  // NULL batch is for compactions
    WriteBatch* updates = BuildBatchGroup(&last_writer);
    WriteBatchInternal::SetSequence(updates, last_sequence + 1);
    last_sequence += WriteBatchInternal::Count(updates);

    // Add to log and apply to memtable.  We can release the lock
    // during this phase since &w is currently responsible for logging
    // and protects against concurrent loggers and concurrent writes
    // into mem_.
    {

//      mutex_.Unlock();
      //TODO: Drop this if we want performance.
      // Power cycle will lose data in log
      struct BatchRecord *brecord = new BatchRecord;

      Slice  record = WriteBatchInternal::Contents(updates);
      brecord->sequence = record.mydata()->sequence;
      brecord->count =   record.mydata()->count;
      std::deque<struct kvData *>::iterator itr;
      for (itr=record.mydata()->kvrecord.begin();
        itr!=record.mydata()->kvrecord.end(); ++itr) {
        kvData *kvdata = (struct kvData*)current_arena_for_log_->AllocateAligned(sizeof(struct kvData));
        if(kvdata == NULL) {
            status = Status::IOError("CAN NOT ALLOC MEM ","IN DBIMPL::WRITE");
 //           mutex_.Lock();
            return status;
        }
        kvdata->kType = (*itr)->kType;
        kvdata->keySize = (*itr)->keySize;
        char *key = current_arena_for_log_->AllocateAligned((*itr)->keySize); //new kvData;
        memcpy(key, (*itr)->key, (*itr)->keySize);
        kvdata->key = key;
        kvdata->value = (*itr)->value;
        brecord->kvrecord.push_back(kvdata);
      }

      current_logmem_->push_back(brecord);
      if (current_logmem_->size() == kNumKeysBeforeWriteToLog) {
          Log(options_.info_log, 0, "Calling INTERNAL FLUSH from Write");
          status = InternalFlush();
      }

      if (status.ok()) {
        // Only add token to token list if command is write_through
        if (options.sync || memory) {
          // Add tuple if connection_id is not present in map
          //  otherwise update ack_sequence for the given connection_id.
          // Only need to store most recent ack_sequence.
          auto token_iter = mem_token_list_.find(std::get<1>(options.token));
          if (token_iter != mem_token_list_.end()) {
            token_iter->second = std::get<0>(options.token);
          } else {
            mem_token_list_.insert({std::get<1>(options.token), std::get<0>(options.token)});
          }
        }
        status = WriteBatchInternal::MyInsertInto(updates, mem_);
        if (status.ok()) {
           status = MakeRoomForWrite(my_batch == NULL);
        }
      }
    }
    if (updates == tmp_batch_) tmp_batch_->Clear();

    versions_->SetLastSequence(last_sequence);
  } else {
    Slice record = WriteBatchInternal::Contents(my_batch);
    std::deque<struct kvData *>::iterator itr;
    for (itr=record.mydata()->kvrecord.begin();
        itr!=record.mydata()->kvrecord.end(); ++itr) {
      delete (*itr);
    }
  }
  if (status.ok() && memory && current_logmem_->size() > 0) {
//      this->InternalFlush();
  }
  while (true) {
    Writer* ready = writers_.front();
    writers_.pop_front();
    if (ready != &w) {
      ready->status = status;
      ready->done = true;
      ready->cv.Signal();
    }
    if (ready == last_writer) break;
  }

  // Notify new head of write queue
  if (!writers_.empty()) {
    writers_.front()->cv.Signal();
  }
  return status;
}

// REQUIRES: Writer list must be non-empty
// REQUIRES: First writer must have a non-NULL batch
WriteBatch* DBImpl::BuildBatchGroup(Writer** last_writer) {
  assert(!writers_.empty());
  Writer* first = writers_.front();
  WriteBatch* result = first->batch;
  assert(result != NULL);

  size_t size = WriteBatchInternal::ByteSize(first->batch);

  // Allow the group to grow up to a maximum size, but if the
  // original write is small, limit the growth so we do not slow
  // down the small write too much.
  size_t max_size = 1 << 20;
  if (size <= (128<<10)) {
    max_size = size + (128<<10);
  }

  *last_writer = first;
  std::deque<Writer*>::iterator iter = writers_.begin();
  ++iter;  // Advance past "first"
  for (; iter != writers_.end(); ++iter) {
    Writer* w = *iter;
    if (w->sync && !first->sync) {
      // Do not include a sync write into a batch handled by a non-sync write.
      break;
    }

    if (w->batch != NULL) {
      size += WriteBatchInternal::ByteSize(w->batch);
      if (size > max_size) {
        // Do not make batch too big
        break;
      }

      // Append to *result
      if (result == first->batch) {
        // Switch to temporary batch instead of disturbing caller's batch
        result = tmp_batch_;
        assert(WriteBatchInternal::Count(result) == 0);
        WriteBatchInternal::Append(result, first->batch);
      }
      WriteBatchInternal::Append(result, w->batch);
    }
    *last_writer = w;
  }
  return result;
}
// REQUIRES: mutex_ is held
// REQUIRES: this thread is currently at the front of the writer queue
Status DBImpl::MakeRoomForWrite(bool force) {
  mutex_.AssertHeld();
  assert(!writers_.empty());
  bool allow_delay = !force;
  Status s;

  while (true) {
    //cout << "MEM APPROX " << mem_->ApproximateMemoryUsage() << endl;
    if (!bg_error_.ok() && !bg_error_.IsSuperblockIO() && !bg_error_.IsNoSpaceAvailable()) {
      // Yield previous error
      Log(options_.info_log, 0, "BG_ERROR %s", bg_error_.ToString().c_str());
      s = bg_error_;
      break;
    } else if (allow_delay &&
               versions_->NumLevelFiles(0) >= config::kL0_SlowdownWritesTrigger) {
      // We are getting close to hitting a hard limit on the number of
      // L0 files.  Rather than delaying a single write by several
      // seconds when we hit the hard limit, start delaying each
      // individual write by (10ms)*(number of file at level 0 minus kL0_SlowdownWritesTrigger) ms
      //                     or 0.5 sec which ever is less.
      // to reduce latency variance.  Also,
      // this delay hands over some CPU to the compaction thread in
      // case it is sharing the same core as the writer.
        uint diff = versions_->NumLevelFiles(0) - config::kL0_SlowdownWritesTrigger;
        if (diff > 50) {  //Limit to 0.5 seconds max
          diff = 50;
        }
        time_t delayTime = (10 * diff)*1000;
        mutex_.Unlock();
        env_->SleepForMicroseconds(delayTime);
        allow_delay = false;  // Do not delay a single write more than once
        mutex_.Lock(); 
    } else if (!force &&
               (mem_->ApproximateMemoryUsage() <= options_.write_buffer_size) &&
               (mem_->ApproximateL0sstSize() <= options_.sst_size)) {
        break;
    } else if (imm_ != NULL) {
      // We have filled up the current memtable, but the previous
      // one is still being compacted, so we wait.
        if (!this->bg_compaction_scheduled_) {
           MaybeScheduleCompaction();
        }
	//cout << "WAIT FOR THE PREVIOUS MEM TABLE TO BE DONE" << endl;
        bg_cv_.Wait();
    } else if (versions_->NumLevelFiles(0) >= config::kL0_StopWritesTrigger) {
      // There are too many level-0 files.
        if (!this->bg_compaction_scheduled_) {
           MaybeScheduleCompaction();
        }
      Log(options_.info_log, 0, "Waiting for room...\n");
      bg_cv_.Wait();
    } else {
      Log(options_.info_log, 5, "Switching Memtable. approx L0size=%lu, approx memSize=%lu",
          mem_->ApproximateL0sstSize(), mem_->ApproximateMemoryUsage());
      s = SwitchMemTable();
      if (!s.ok()) {
        break;
      }
      force = false;   // Do not force another compaction if have room
    }
  }
  //cout << "EXIT MAKEROOM" << endl;
  return s;
}

bool DBImpl::GetProperty(const Slice& property, std::string* value) {
  value->clear();

  MutexLock l(&mutex_);
  Slice in = property;
  Slice prefix("leveldb.");
  if (!in.starts_with(prefix)) return false;
  in.remove_prefix(prefix.size());

  if (in.starts_with("num-files-at-level")) {
    in.remove_prefix(strlen("num-files-at-level"));
    uint64_t level;
    bool ok = ConsumeDecimalNumber(&in, &level) && in.empty();
    if (!ok || level >= config::kNumLevels) {
      return false;
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "%d",
               versions_->NumLevelFiles(static_cast<int>(level)));
      *value = buf;
      return true;
    }
  } else if (in == "stats") {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "                               Compactions\n"
             "Level  Files Size(MB) Time(sec) Read(MB) Write(MB)\n"
             "--------------------------------------------------\n"
             );
    value->append(buf);
    for (int level = 0; level < config::kNumLevels; level++) {
      int files = versions_->NumLevelFiles(level);
      if (stats_[level].micros > 0 || files > 0) {
        snprintf(
            buf, sizeof(buf),
            "%3d %8d %8.0f %9.0f %8.0f %9.0f\n",
            level,
            files,
            versions_->NumLevelBytes(level) / 1048576.0,
            stats_[level].micros / 1e6,
            stats_[level].bytes_read / 1048576.0,
            stats_[level].bytes_written / 1048576.0);
        value->append(buf);
      }
    }
    return true;
  } else if (in == "sstables") {
    Version* version = versions_->current();
    *value = version->DebugString();
    version->Unref();
    return true;
  }

  return false;
}


void DBImpl::FillZoneMap() {
  Log(options_.info_log, 5, "Marking all zones as full");
  env_->FillZoneMap();
}

void DBImpl::GetApproximateSizes(
    const Range* range, int n,
    uint64_t* sizes) {
  // TODO(opt): better implementation
  Version* v;
  {
    MutexLock l(&mutex_);
    v = versions_->current();
  }

  for (int i = 0; i < n; i++) {
    // Convert user_key into a corresponding internal key.
    InternalKey k1(range[i].start, kMaxSequenceNumber, kValueTypeForSeek);
    InternalKey k2(range[i].limit, kMaxSequenceNumber, kValueTypeForSeek);
    uint64_t start = versions_->ApproximateOffsetOf(v, k1);
    uint64_t limit = versions_->ApproximateOffsetOf(v, k2);
    sizes[i] = (limit >= start ? limit - start : 0);
  }

  {
    MutexLock l(&mutex_);
    v->Unref();
  }
}



Status DBImpl::InternalFlush() {
    if (this->bg_error_.IsCorruption()) {
        return bg_error_;
    }

    Status s;
    WritableFile* lfile = NULL;
    while (has_imm_.NoBarrier_Load() != NULL) {
       MaybeScheduleCompaction();
       bg_cv_.Wait();
   }
    if (env_->numberOfGoodSuperblocks() <= 1) {
        s = env_->Sync();
        if (!s.ok()) {
            RecordBackgroundError(s);
            SendCmdListStatus(options_.outstanding_status_sender, false, mem_token_list_);
            ReleaseMem();
            mem_token_list_.clear();
            return s;
        } else {
            RecordBackgroundError(s);
        }
    }
    if (current_logmem_->size() > 0) {
        if (logfile_ == NULL) {
          uint64_t new_log_number = versions_->NewFileNumber();
          Log(options_.info_log, 5, "Creating log file #%lld", (unsigned long long)new_log_number);
          s = env_->NewWritableFile(LogFileName(dbname_, new_log_number), &lfile);
          if (!s.ok()) {
            Log(options_.info_log, 1," %s",  s.ToString().c_str());
            // Avoid chewing through file number space in a tight loop.
            versions_->ReuseFileNumber(new_log_number);
            return s;
          }
      FileInfo* logFileInfo = ((SmrWritableFile*)lfile)->getFileInfo();
      if (persistedLogFile_) {
          FileInfo* persistedFileInfo = ((SmrWritableFile*)persistedLogFile_)->getFileInfo();
          std::string fname = LogFileName(dbname_, persistedFileInfo->getNumber());
          delete persistedLogFile_;
          env_->DeleteFile(fname);
          persistedLogFile_ = NULL;
      }
          logfile_number_ = new_log_number;
          logfile_ = lfile;
          log_ = new log::Writer(lfile, env_, dbname_);
        }
        std::deque<struct BatchRecord *>::iterator itr;
        for(itr= current_logmem_->begin(); itr != current_logmem_->end(); ++itr) {
          s = log_->MyAddRecord(Slice(*itr));
          delete *itr;
        }
        s = logfile_->Sync();
        if (s.ok()) {
            s = env_->Sync();
            if (!s.ok()) {
                RecordBackgroundError(s);
                SendCmdListStatus(options_.outstanding_status_sender, false, mem_token_list_);
                ReleaseMem();
                mem_token_list_.clear();
                return s;
            } else {
                RecordBackgroundError(s);
            }
        }
        current_logmem_->clear();
        delete current_arena_for_log_;
        current_arena_for_log_ = new Arena;
    }
    return s;
}

Status DBImpl::Flush(bool toSST, bool clearMems, bool toClose) {
    mutex_.Lock();
    Status s;
    if (clearMems) {
        while (has_imm_.NoBarrier_Load() != NULL) {
           MaybeScheduleCompaction();
           bg_cv_.Wait();
        }
        if (mem_ && !mem_->IsEmpty()) {
            SwitchMemTable();
            while (has_imm_.NoBarrier_Load() != NULL) {
                MaybeScheduleCompaction();
                bg_cv_.Wait();
            }
        }
    } else {
        if (toClose) {
           cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": To close" << endl;
           shutting_down_.Release_Store(this);  // Any non-NULL value is ok
        }
        bool send_cmd_list = true;
        if (toSST) {
            while (has_imm_.NoBarrier_Load() != NULL) {
                MaybeScheduleCompaction();
                bg_cv_.Wait();
            }
            if (mem_ && !mem_->IsEmpty()) {
                SwitchMemTable();
                while (has_imm_.NoBarrier_Load() != NULL) {
                    MaybeScheduleCompaction();
                    bg_cv_.Wait();
                }
                send_cmd_list = false;
            } else {
                s = InternalFlush();
            }
        } else {
            s = InternalFlush();
        }
        if (send_cmd_list && s.ok()) {
            SendCmdListStatus(options_.outstanding_status_sender, true, mem_token_list_);
            mem_token_list_.clear();
        } else if (!s.ok()) {
            RecordBackgroundError(s);
            SendCmdListStatus(options_.outstanding_status_sender, false, mem_token_list_);
            ReleaseMem();
            mem_token_list_.clear();
        }
    }
    bg_cv_.SignalAll();
    mutex_.Unlock();
    return s;
}
// Default implementations of convenience methods that subclasses of DB
// can call if they wish
Status DB::Put(const WriteOptions& opt, const Slice& key, const Slice& value) {
  WriteBatch batch;
  batch.Put(key, value);
  return Write(opt, &batch);
}

Status DB::Delete(const WriteOptions& opt, const Slice& key) {
  WriteBatch batch;
  batch.Delete(key);
  return Write(opt, &batch);
}

DB::~DB() { }

Status DB::Open(const Options& options, const std::string& dbname,
                DB** dbptr) {
  *dbptr = NULL;
  Log(options.info_log, 5, "DB NAME %s", dbname.c_str());
  DBImpl* impl = new DBImpl(options, dbname);
  impl->mutex_.Lock();
  VersionEdit edit;
  Status s = impl->Recover(&edit); // Handles create_if_missing, error_if_exists
  if (s.ok() || s.IsNoSpaceAvailable()) {
    s = impl->versions_->LogAndApply(&edit, &impl->mutex_);
    impl->logfile_ = NULL;
    impl->log_ = NULL;
    impl->DeleteObsoleteFiles();
  }
#ifndef NLOG
  VersionSet::LevelSummaryStorage tmp;
  Log(options.info_log, 5, "Level Summary: %s", impl->versions_->LevelSummary(&tmp));
#endif
  impl->mutex_.Unlock();
  if (s.ok() || s.IsNoSpaceAvailable()) {
    *dbptr = impl;

    if (impl->versions_->descriptor_file_ != NULL) {
      impl->versions_->descriptor_file_->subscribe(impl);
    }
    // try to find current end key using the DB iterator
    Iterator* it = impl->NewIterator(leveldb::ReadOptions());
    it->SeekToLast();
    // if it is valid, call DBImpl::SetEndKey()
    if (it->Valid()) {
      impl->SetEndKey(it->key());
    }
    // clean up DB iterator
    delete it;
    impl->mutex_.Lock();
    impl->MaybeScheduleCompaction();
    impl->mutex_.Unlock();
  } else {
    delete impl;
  }
  return s;
}

Snapshot::~Snapshot() {
}

Status DestroyDB(const std::string& dbname, const Options& options) {
  Env* env = options.env;
  std::vector<std::string> filenames;
  // Ignore error in case directory does not exist
  env->GetChildren(dbname, &filenames);
  if (filenames.empty()) {
    return Status::OK();
  }

  FileLock* lock;
  const std::string lockname = LockFileName(dbname);
  Status result = env->LockFile(lockname, &lock);
  if (result.ok()) {
    uint64_t number;
    FileType type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type) &&
          type != kDBLockFile) {  // Lock file will be deleted at end
        Status del = env->DeleteFile(dbname + "/" + filenames[i]);
        if (result.ok() && !del.ok()) {
          result = del;
        }
      }
    }
    env->UnlockFile(lock);  // Ignore error since state is already gone
    env->DeleteFile(lockname);
    env->DeleteDir(dbname);  // Ignore error in case dir contains other files
  }
  return result;
}

}  // namespace leveldb
