// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_IMPL_H_
#define STORAGE_LEVELDB_DB_DB_IMPL_H_

#include <deque>
#include <set>
#include <unordered_set>

#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "db/version_set.h"
#include "db/memtable.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "port/thread_annotations.h"
using namespace smr;

namespace leveldb {

class TableCache;
class Version;
class VersionEdit;

class DBImpl : public DB {
 public:
  static const int MIN_ZONE_FOR_MEM_COMPACTION = 5;
  static const int MIN_ZONE_FOR_ACCEPT_DELETE = 7;

 public:
  static const uint32_t kNumKeysBeforeWriteToLog = 2500;
  DBImpl(const Options& options, const std::string& dbname);
  virtual ~DBImpl();
  virtual void notify() {
      MutexLock lock(&mutex_);
      if (has_imm_.NoBarrier_Load() == NULL) {
         //cout << "SWITCHMEM" << endl;
         SwitchMemTable();
      } else {
         MaybeScheduleCompaction();
      }
  }
  void ReleaseImmu();
  void ReleaseMem();

  virtual void notifyNewManifestSegments() {
      versions_->newManifestSegments();
      MaybeScheduleCompaction();
  }
  // Implementations of the DB interface
  virtual Status Put(const WriteOptions&, const Slice& key, const Slice& value);
  virtual Status Delete(const WriteOptions&, const Slice& key);
  virtual Status Write(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL);
  virtual Status WriteBat(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL);
  virtual Status Get(const ReadOptions& options,
                     const Slice& key,
                     char* value, bool ignore_value,
		             bool using_bloom_filter);
  Status PutByInternal(const WriteOptions& option, const Slice& sInternalKey, const Slice& value);

  virtual Iterator* NewIterator(const ReadOptions&);
  virtual const Snapshot* GetSnapshot();
  virtual void ReleaseSnapshot(const Snapshot* snapshot);
  virtual bool GetProperty(const Slice& property, std::string* value);
  virtual void FillZoneMap();
  virtual void GetApproximateSizes(const Range* range, int n, uint64_t* sizes);
  virtual void CompactRange(const Slice* begin, const Slice* end);
  virtual void CompactLevel(int level);
  virtual Status Flush(bool toSST = false, bool clearMems = false, bool toClose = false);
  virtual Status InternalFlush();


  // Extra methods (for testing) that are not in the public DB interface

  // Compact any files in the named level that overlap [*begin,*end]
  void TEST_CompactRange(int level, const Slice* begin, const Slice* end);

  // Force current memtable contents to be compacted.
  Status TEST_CompactMemTable();

  // Return an internal iterator over the current state of the database.
  // The keys of this iterator are internal keys (see format.h).
  // The returned iterator should be deleted when no longer needed.
  Iterator* TEST_NewInternalIterator();

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t TEST_MaxNextLevelOverlappingBytes();

  // Record a sample of bytes read at the specified internal key.
  // Samples are taken approximately once every config::kReadBytesPeriod
  // bytes.
  void RecordReadSample(Slice key);
  virtual void unsubscribe() {
      if (versions_->descriptor_file_ != NULL) {
        versions_->descriptor_file_->unsubscribe(this);
      }
  }
  inline void SetEndKey(const Slice& key);

 private:
  friend class DB;
  struct CompactionState;
  struct Writer;

  Iterator* NewInternalIterator(const ReadOptions&,
                                SequenceNumber* latest_snapshot,
                                uint32_t* seed);

  Status NewDB();
  Status GetByInternal(const ReadOptions& options,
                     const Slice& key,
                     char* value, bool using_bloom_filter, uint64_t* seqNum);

  // Recover the descriptor from persistent storage.  May do a significant
  // amount of work to recover recently logged updates.  Any changes to
  // be made to the descriptor are added to *edit.
  Status Recover(VersionEdit* edit) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Status SplitTable(FileMetaData* sourceFileMetaData, CompactionState* compact);
  void MaybeIgnoreError(Status* s) const;

  // Delete any unneeded files and stale in-memory entries.
  void DeleteObsoleteFiles(VersionEdit* vEdit = NULL);

  // Compact the in-memory write buffer to disk.  Switches to a new
  // log-file/memtable and writes a new descriptor iff successful.
  // Errors are recorded in bg_error_.
  Status CompactMemTable() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status RecoverLogFile(uint64_t log_number,
                        VersionEdit* edit,
                        SequenceNumber* max_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status MakeRoomForWrite(bool force /* compact even if there is room? */)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  WriteBatch* BuildBatchGroup(Writer** last_writer);

  void RecordBackgroundError(const Status& s);
  void MaybeScheduleCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  static void BGWork(void* db);
  void BGSchedule();
  static void BGDefragmentSST(void* db);
  void DefragmentSST();
  bool IsCorrupted() {
      return (env_->numberOfGoodSuperblocks() == 0);
  }
  void BackgroundCall();
  Status  BackgroundCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  static void BGDefragValue(void* db);
  void DefragmentValue();

  void CleanupCompaction(CompactionState* compact)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Status DoCompactionWork(CompactionState* compact)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status OpenCompactionOutputFile(CompactionState* compact);
  Status FinishCompactionOutputFile(CompactionState* compact, Iterator* input);
  Status InstallCompactionResults(CompactionState* compact)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  inline Status SwitchMemTable();
  bool HasSpaceForMemCompaction() {
      return (env_->GetNumberFreeZones() >= MIN_ZONE_FOR_MEM_COMPACTION);
  }
  bool HasSpaceForDelCommand() {
      return (env_->GetNumberFreeZones() >= MIN_ZONE_FOR_ACCEPT_DELETE);
  }

  // Constant after construction
  Env* const env_;
  const InternalKeyComparator internal_comparator_;
  const InternalFilterPolicy internal_filter_policy_;
  const Options options_;  // options_.comparator == &internal_comparator_
  bool owns_info_log_;
  bool owns_cache_;
  const std::string dbname_;

  // end key in the DB
  char* end_key_;
  // slice used to compare end key in the DB to other keys
  Slice end_key_slice_;

  // table_cache_ provides its own synchronization
  TableCache* table_cache_;

  // Lock over the persistent DB state.  Non-NULL iff successfully acquired.
  FileLock* db_lock_;

  // An additional mutex to support conditional write calls
  port::Mutex write_mutex_;

  // State below is protected by mutex_
  port::Mutex mutex_;
  port::AtomicPointer shutting_down_;
  port::CondVar bg_cv_;          // Signalled when background work finishes
  MemTable* mem_;
  MemTable* imm_;                // Memtable being compacted
  std::deque<struct BatchRecord *> logmem_[2];
  std::deque<struct BatchRecord *> *current_logmem_;
  std::deque<struct BatchRecord *> *logmem_imm_;
  Arena *arena_for_log_imm_;
  Arena *current_arena_for_log_;
  int logmem_in_use_;
  std::unordered_map<int64_t, uint64_t> mem_token_list_;
  std::unordered_map<int64_t, uint64_t> imm_token_list_; // Token list associated with Memtable being compacted
  port::AtomicPointer has_imm_;  // So bg thread can detect non-NULL imm_
  WritableFile* logfile_;
  WritableFile* persistedLogFile_;
  uint64_t logfile_number_;
  log::Writer* log_;
  uint32_t seed_;                // For sampling.
  bool defrag_going_;
  // Queue of writers.
  std::deque<Writer*> writers_;
  WriteBatch* tmp_batch_;

  SnapshotList snapshots_;

  // Set of table files to protect from deletion because they are
  // part of ongoing compactions.
  std::unordered_set<uint64_t> pending_outputs_;
  std::set< std::pair<int, uint64_t> > deletedFiles_;

  // Has a background compaction been scheduled or is running?
  bool bg_compaction_scheduled_;
  bool valueDefragmentScheduled_;
  bool sstDefragmentScheduled_;

  // Information for a manual compaction
  struct ManualCompaction {
    int level;
    bool done;
    const InternalKey* begin;   // NULL means beginning of key range
    const InternalKey* end;     // NULL means end of key range
    InternalKey tmp_storage;    // Used to keep track of compaction progress
  };
  ManualCompaction* manual_compaction_;

  VersionSet* versions_;

  // Have we encountered a background error in paranoid mode?
  Status bg_error_;

  // Per level compaction stats.  stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct CompactionStats {
    int64_t micros;
    int64_t bytes_read;
    int64_t bytes_written;

    CompactionStats() : micros(0), bytes_read(0), bytes_written(0) { }

    void Add(const CompactionStats& c) {
      this->micros += c.micros;
      this->bytes_read += c.bytes_read;
      this->bytes_written += c.bytes_written;
    }
  };
  CompactionStats stats_[config::kNumLevels];

  // No copying allowed
  DBImpl(const DBImpl&);
  void operator=(const DBImpl&);

  const Comparator* user_comparator() const {
    return internal_comparator_.user_comparator();
  }
};

inline Status DBImpl::SwitchMemTable() {
    mutex_.AssertHeld();
    Status s;
    if (mem_->IsEmpty()) {
      Log(options_.info_log,0," EMPTY");
      return s;
    }
    imm_ = mem_;
    logmem_imm_ = current_logmem_;
    arena_for_log_imm_ = current_arena_for_log_;
    if (++logmem_in_use_ == 2){
      logmem_in_use_ = 0;
    }
    current_logmem_ = &logmem_[logmem_in_use_];
    current_arena_for_log_ = new Arena;
    imm_token_list_ = mem_token_list_;
    mem_ = new MemTable(internal_comparator_);
    mem_->Ref();
    mem_token_list_.clear();
    has_imm_.Release_Store(imm_);
    MaybeScheduleCompaction();
    return s;
}

inline void DBImpl::SetEndKey(const Slice& key) {
  // copy key to the memory allocated for end_key_
  memcpy(end_key_, key.data(), key.size());

  // initialize end_key_slice_ using the newly copied byte array and the key's size
  end_key_slice_ = Slice(end_key_, key.size());
}

// Sanitize db options.  The caller should delete result.info_log if
// it is not equal to src.info_log.
extern Options SanitizeOptions(const std::string& db,
                               const InternalKeyComparator* icmp,
                               const InternalFilterPolicy* ipolicy,
                               const Options& src);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DB_IMPL_H_
