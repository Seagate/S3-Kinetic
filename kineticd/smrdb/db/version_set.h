// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// The representation of a DBImpl consists of a set of Versions.  The
// newest version is called "current".  Older versions may be kept
// around to provide a consistent view to live iterators.
//
// Each Version keeps track of a set of Table files per level.  The
// entire set of versions is maintained in a VersionSet.
//
// Version,VersionSet are thread-compatible, but require external
// synchronization on all accesses.

#ifndef STORAGE_LEVELDB_DB_VERSION_SET_H_
#define STORAGE_LEVELDB_DB_VERSION_SET_H_

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <unordered_set>
#include <float.h>
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "common/Listener.h"
#include "smrdisk/ManifestWritableFile.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "util/mutexlock.h"

using namespace std;
using namespace com::seagate::common;
using namespace smr;

namespace leveldb {

namespace log { class Writer; }

class Compaction;
class Iterator;
class MemTable;
class TableBuilder;
class TableCache;
class Version;
class VersionSet;
class WritableFile;
namespace {
enum SaverState {
  kNotFound,
  kFound,
  kDeleted,
  kCorrupt,
};

struct Saver {
  SaverState state;
  const Comparator* ucmp;
  Slice user_key;
//  std::string* value;
  char* value;
  uint32_t size;
  uint64_t seqNum;
};

}

// Return the last index i such that files[i]->smallest <= key.
// Return files.size() if there is no such file.
// REQUIRES: "files" contains a sorted list of non-overlapping files.
extern int FindFile(const InternalKeyComparator& icmp,
                    const std::vector<FileMetaData*>& files,
                    const Slice& key);

// Returns true iff some file in "files" overlaps the user key range
// [*smallest,*largest].
// smallest==NULL represents a key smaller than all keys in the DB.
// largest==NULL represents a key largest than all keys in the DB.
// REQUIRES: If disjoint_sorted_files, files[] contains disjoint ranges
//           in sorted order.
extern bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key);

class Version {
 public:
  // Append to *iters a sequence of iterators that will
  // yield the contents of this Version when merged together.
  // REQUIRES: This version has been saved (see VersionSet::SaveTo)
  void AddIterators(const ReadOptions&, std::vector<Iterator*>* iters);

  // Lookup the value for key.  If found, store it in *val and
  // return OK.  Else return a non-OK status.  Fills *stats.
  // REQUIRES: lock is not held
  struct GetStats {
    FileMetaData* seek_file;
    int seek_file_level;
  };
  Status Get(const ReadOptions&, const LookupKey& key, char* val,
             GetStats* stats, bool using_bloom_filter, uint64_t* seqNum = NULL);

  // Adds "stats" into the current state.  Returns true if a new
  // compaction may need to be triggered, false otherwise.
  // REQUIRES: lock is held
  bool UpdateStats(const GetStats& stats);

  // Record a sample of bytes read at the specified internal key.
  // Samples are taken approximately once every config::kReadBytesPeriod
  // bytes.  Returns true if a new compaction may need to be triggered.
  // REQUIRES: lock is held
  bool RecordReadSample(Slice key);

  // Reference count management (so Versions do not disappear out from
  // under live iterators)
  void Unref();
  FileMetaData* FindFileToCompact(InternalKey& intputLargest_0, int level);

  void GetOverlappingInputs(
      int level,
      const InternalKey* begin,         // NULL means before all keys
      const InternalKey* end,           // NULL means after all keys
      std::vector<FileMetaData*>* inputs,
      uint64_t byte_size_limit = std::numeric_limits<uint64_t>::max()); // stop adding additional inputs if cum. size would breach limit

  // Returns true iff some file in the specified level overlaps
  // some part of [*smallest_user_key,*largest_user_key].
  // smallest_user_key==NULL represents a key smaller than all keys in the DB.
  // largest_user_key==NULL represents a key largest than all keys in the DB.
  bool OverlapInLevel(int level,
                      const Slice* smallest_user_key,
                      const Slice* largest_user_key);

  // Return the level at which we should place a new memtable compaction
  // result that covers the range [smallest_user_key,largest_user_key].
  int PickLevelForMemTableOutput(const Slice& smallest_user_key,
                                 const Slice& largest_user_key);

  int NumFiles(int level) const { return files_[level].size(); }

  int TotalNumFiles() {
      int total = 0;
      for (int i=0; i <7 ; ++i) {
          total += NumFiles(i);
      }
      return total;
  }

  // Return a human readable string that describes this version's contents.
  std::string DebugString() const;
  void AddDeletableValueFiles(set<uint64_t>& deletableValueFiles) {
      MutexLock lock(&mutex_);
      for (set<uint64_t>::iterator it = deletableValueFiles.begin(); it != deletableValueFiles.end(); ++it) {
          deletedValueFiles_.insert(*it);
      }
  }
 private:
  friend class Compaction;
  friend class VersionSet;

  class LevelFileNumIterator;
  Iterator* NewConcatenatingIterator(const ReadOptions&, int level) const;

  // Call func(arg, level, f) for every file that overlaps user_key in
  // order from newest to oldest.  If an invocation of func returns
  // false, makes no more calls.
  //
  // REQUIRES: user portion of internal_key == user_key.
  void ForEachOverlapping(Slice user_key, Slice internal_key,
                          void* arg,
                          bool (*func)(void*, int, FileMetaData*));

  VersionSet* vset_;            // VersionSet to which this Version belongs
  Version* next_;               // Next version in linked list
  Version* prev_;               // Previous version in linked list
  int refs_;                    // Number of live refs to this version

  // List of files per level
  std::vector<FileMetaData*> files_[config::kNumLevels];
  set<uint64_t> deletedValueFiles_;
  uint64_t levelBytes_[config::kNumLevels];

  // Next file to compact based on seek stats.
  FileMetaData* file_to_compact_;
  int file_to_compact_level_;

  // Level that should be compacted next and its compaction score.
  // Score < 1 means compaction is not strictly needed.  These fields
  // are initialized by Finalize().
  double compaction_score_;
  int compaction_level_;
  float manifest_compaction_score_;
  size_t last_manifest_size_;
  port::Mutex mutex_;

  explicit Version(VersionSet* vset)
      : vset_(vset), next_(this), prev_(this), refs_(0),
        file_to_compact_(NULL),
        file_to_compact_level_(-1),
        compaction_score_(-1),
        compaction_level_(-1),
        manifest_compaction_score_(-1),
        last_manifest_size_(0) {
      for (unsigned i = 0; i < config::kNumLevels; ++i) {
          levelBytes_[i] = 0;
      }
  }

  ~Version();

  // No copying allowed
  Version(const Version&);
  void operator=(const Version&);

  FileMetaData* FindFileAfterKey(InternalKey& key, vector<FileMetaData*>& files);
  void Ref() {
    mutex_.Lock();
    ++refs_;
    mutex_.Unlock();
  }

};

class VersionSet {
 public:
  static const int MANIFEST_COMPACT_PERIOD = 10000;
  static const int MAX_RECORD_TO_ENCODE_AT_ONE_TIME = 128;

  VersionSet(const std::string& dbname,
             const Options* options,
             TableCache* table_cache,
             const InternalKeyComparator*);
  virtual ~VersionSet();

  //virtual void notify() {}
  // Apply *edit to the current version to form a new descriptor that
  // is both saved to persistent state and installed as the new
  // current version.  Will release *mu while actually writing to the file.
  // REQUIRES: *mu is held on entry.
  // REQUIRES: no other thread concurrently calls LogAndApply()
  Status LogAndApply(VersionEdit* edit, port::Mutex* mu)
      EXCLUSIVE_LOCKS_REQUIRED(mu);
  // Recover the last saved descriptor from persistent storage.
  Status Recover();

  // Return the current version.
  Version* current() {
      MutexLock lock(&mutex_);
      current_->Ref();
      return current_;
  }

  // Return the current manifest file number
  uint64_t ManifestFileNumber() const { return manifest_file_number_; }

  // Allocate and return a new file number
  uint64_t NewFileNumber() { return next_file_number_++; }

  // Arrange to reuse "file_number" unless a newer file number has
  // already been allocated.
  // REQUIRES: "file_number" was returned by a call to NewFileNumber().
  void ReuseFileNumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

  // Return the number of Table files at the specified level.
  int NumLevelFiles(int level) const;

  // Return the combined file size of all files at the specified level.
  int64_t NumLevelBytes(int level) const;

  // Return the last sequence number.
  uint64_t LastSequence() const { return last_sequence_; }

  // Set the last sequence number to s.
  void SetLastSequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_ = s;
  }

  // Mark the specified file number as used.
  void MarkFileNumberUsed(uint64_t number);

  // Return the current log file number.
  uint64_t LogNumber() const { return log_number_; }

  // Return the log file number for the log file that is currently
  // being compacted, or zero if there is no such log file.
  uint64_t PrevLogNumber() const { return prev_log_number_; }

  // Pick level and inputs for a new compaction.
  // Returns NULL if there is no compaction to be done.
  // Otherwise returns a pointer to a heap-allocated object that
  // describes the compaction.  Caller should delete the result.
  Compaction* PickCompaction();

  // Return a compaction object for compacting the range [begin,end] in
  // the specified level.  Returns NULL if there is nothing in that
  // level that overlaps the specified range.  Caller should delete
  // the result.
  Compaction* CompactRange(
      int level,
      const InternalKey* begin,
      const InternalKey* end);

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t MaxNextLevelOverlappingBytes();

  // Create an iterator that reads over the compaction inputs for "*c".
  // The caller should delete the iterator when no longer needed.
  Iterator* MakeInputIterator(Compaction* c);

  // Returns true iff some level needs a compaction.
  bool NeedsCompaction() const {
    Version* v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != NULL) || (v->manifest_compaction_score_ >= 1);
  }

  void ForceCompaction();
  Status CompactManifest();
  virtual void newManifestSegments() {
      ++nNewManifestSegments_;
      ComputeManifestCompactionScore();
  }

  // Add all files listed in any live version to *live.
  // May also mutate some internal state.
  void AddLiveFiles(std::unordered_set<uint64_t>* live);

  // Return the approximate offset in the database of the data for
  // "key" as of version "v".
  uint64_t ApproximateOffsetOf(Version* v, const InternalKey& key);

  // Return a human-readable short (single-line) summary of the number
  // of files per level.  Uses *scratch as backing store.
  struct LevelSummaryStorage {
    char buffer[100];
  };
  const char* LevelSummary(LevelSummaryStorage* scratch) const;

  void AddFailedCompactionLevel(int level, uint64_t spaceNeeds) {
      excludedCompactionLevels_.insert(level);
      spaceNeedsForCompactFailures_[level] = spaceNeeds;
      Version* version = this->current();
      Finalize(version);
      version->Unref();
  }
  void RemoveFailedCompactionLevel(int level) {
      if (level < 0) {
          return;
      }
      excludedCompactionLevels_.erase(level);
      spaceNeedsForCompactFailures_[level] = 0;
      Version* version = this->current();
      Finalize(version);
      version->Unref();
  }
  void PrintCompactStatus();
  bool IsNoSpaceForAllLevelCompaction() const {
      return (excludedCompactionLevels_.size() + 1 == config::kNumLevels);
  }
  string GetPrevCompactPointer(int level) const {
      return prev_compact_pointer_[level];
  }
  void RevertCompactPointer(int level) {
      compact_pointer_[level] = prev_compact_pointer_[level];
  }
  void SetExpandLevel(int level) {
      noExpandCompactionLevels_.erase(level);
  }
  bool CanLevelExpand(int level) {
      set<int>::iterator it = noExpandCompactionLevels_.find(level);
      return (it == noExpandCompactionLevels_.end());
  }
  void SetNoExpandLevel(int level) {
      noExpandCompactionLevels_.insert(level);
  }
  void SetCompactPointer(int level, InternalKey key) {
      string sKey = key.Encode().ToString();
      if (compact_pointer_[level] != sKey) {
          this->RemoveFailedCompactionLevel(level);
      }
      this->compact_pointer_[level] = sKey;
  }
  double L1CompactionScore() const {
      return l1CompactionScore_;
  }
 void SetManifestFileNumber(uint64_t n) {
     manifest_file_number_ = n;
 }
 void SetPrevLogNumber(uint64_t n) {
     prev_log_number_ = n;
 }
 void SetLogNumber(uint64_t n) {
     log_number_ = n;
 }
 void NotifyDefragmentComplete();
 void printDeletableValueFiles();

 void GetDeletableValueFiles(set<uint64_t>& deletableFiles) {
    MutexLock lock(&mutex_);
     for (set<uint64_t>::iterator it = deletableValueFiles_.begin(); it != deletableValueFiles_.end(); ++it) {
         deletableFiles.insert(*it);
     }
 }
 void RemoveDeletableValueFiles(set<uint64_t>& deletableFiles) {
    MutexLock lock(&mutex_);
     for (set<uint64_t>::iterator it = deletableFiles.begin(); it != deletableFiles.end(); ++it) {
         deletableValueFiles_.erase(*it);
     }
 }
void Remove(Version* version) {
    MutexLock lock(&mutex_);
    version->prev_->next_ = version->next_;
    version->next_->prev_ = version->prev_;
}
void UpdateDeletableValueFiles(Version* version);
Status DeallocateDeletableValueFiles();
void SubtractObsoletedValueFiles(set<uint64_t>& obsoleteFiles);

bool HasDeletableValueFiles() {
    MutexLock lock(&mutex_);
    return (deletableValueFiles_.size() > 0);
}

Env* GetEnv() { return env_;}

 private:
  class Builder;

  friend class Compaction;
  friend class Version;
  void Finalize(Version* v);
  int NumberOfVersions();

  void GetRange(int level,
		const std::vector<FileMetaData*>& inputs,
                InternalKey* smallest,
                InternalKey* largest);

  // Level is first input's level and second input is from level+1
  void GetRange2(int level,
		 const std::vector<FileMetaData*>& inputs1,
                 const std::vector<FileMetaData*>& inputs2,
                 InternalKey* smallest,
                 InternalKey* largest);

  void SetupOtherInputs(Compaction* c);

  // Save current contents to *log
  Status WriteSnapshot(log::Writer* log);

  void AppendVersion(Version* v);
  void ComputeManifestCompactionScore();

  Env* const env_;
  const std::string dbname_;
  const Options* const options_;
  TableCache* const table_cache_;
  const InternalKeyComparator icmp_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted
  set<int> excludedCompactionLevels_;
  uint64_t spaceNeedsForCompactFailures_[leveldb::config::kNumLevels - 1];
  set<int> noExpandCompactionLevels_;
  double l1CompactionScore_;
  uint32_t l0MaxSeen_;
 public:
  // Opened lazily
  ManifestWritableFile* descriptor_file_;
 private:
  log::Writer* descriptor_log_;
  Version dummy_versions_;  // Head of circular doubly-linked list of versions.
  Version* current_;        // == dummy_versions_.prev_

  // Per-level key at which the next compaction at that level should start.
  // Either an empty string, or a valid InternalKey.
  std::string compact_pointer_[config::kNumLevels];
  std::string prev_compact_pointer_[config::kNumLevels];
  int nNewManifestSegments_;
  set<uint64_t> deletableValueFiles_;
  port::Mutex mutex_;

  // No copying allowed
  VersionSet(const VersionSet&);
  void operator=(const VersionSet&);
};

// A Compaction encapsulates information about a compaction.
class Compaction {
 public:
  ~Compaction();

  // Return the level that is being compacted.  Inputs from "level"
  // and "level+1" will be merged to produce a set of "level+1" files.
  int level() const { return level_; }

  // Return the object that holds the edits to the descriptor done
  // by this compaction.
  VersionEdit* edit() { return &edit_; }

  // "which" must be either 0 or 1
  int num_input_files(int which) const { return inputs_[which].size(); }

  // Return the ith input file at "level()+which" ("which" must be 0 or 1).
  FileMetaData* input(int which, int i) const { return inputs_[which][i]; }

  InternalKey Input0LargestKey() const { return input0_largest_; }

  void SetInput0Largest(InternalKey largest) { input0_largest_ = largest; }

  // Maximum size of files to build during this compaction.
  uint64_t MaxOutputFileSize() const { return max_output_file_size_; }

  // Is this a trivial compaction that can be implemented by just
  // moving a single input file to the next level (no merging or splitting)
  bool IsTrivialMove() const;

  // Add all inputs to this compaction as delete operations to *edit.
  void AddInputDeletions(VersionEdit* edit);

  // Returns true if the information we have available guarantees that
  // the compaction is producing data in "level+1" for which no data exists
  // in levels greater than "level+1".
  bool IsBaseLevelForKey(const Slice& user_key);

  // Returns true iff we should stop building the current output
  // before processing "internal_key".
  bool ShouldStopBefore(const Slice& internal_key);

  // Release the input version for the compaction, once the compaction
  // is successful.
  void ReleaseInputs();

  void GetLevelLargest();

  void ClearInputs() {
      for (int i = 0; i < 2; ++i) {
          inputs_[i].clear();
      }
  }
  void AddInput(FileMetaData* meta, int level) {
      inputs_[level].push_back(meta);
  }
 private:
  friend class Version;
  friend class VersionSet;

  explicit Compaction(int level, const Options* options);

  int level_;
  uint64_t max_output_file_size_;
  Version* input_version_;
  VersionEdit edit_;

  // Each compaction reads inputs from "level_" and "level_+1"
  std::vector<FileMetaData*> inputs_[2];      // The two sets of inputs
  const Options* options_;
  InternalKey input0_largest_;
  InternalKey level_largest_[config::kNumLevels];

  // State used to check for number of of overlapping grandparent files
  // (parent == level_ + 1, grandparent == level_ + 2)
  std::vector<FileMetaData*> grandparents_;
  size_t grandparent_index_;  // Index in grandparent_starts_
  bool seen_key_;             // Some output key has been seen
  int64_t overlapped_bytes_;  // Bytes of overlap between current output
                              // and grandparent files

  // State for implementing IsBaseLevelForKey

  // level_ptrs_ holds indices into input_version_->levels_: our state
  // is that we are positioned at one of the file ranges for each
  // higher level than the ones involved in this compaction (i.e. for
  // all L >= level_ + 2).
  size_t level_ptrs_[config::kNumLevels];
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_SET_H_
