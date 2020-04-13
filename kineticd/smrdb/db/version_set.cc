// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/version_set.h"

#include <float.h>
#include <algorithm>
#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <set>
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "leveldb/env.h"
#include "leveldb/table_builder.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"
#include "smrdisk/DriveEnv.h"
#include "smrdisk/ManifestWritableFile.h"
#include "smrdisk/ManifestSequentialFile.h"
#include "smrdisk/ValueFileCache.h"

using namespace std;
using namespace smr;

int kTargetFileSize = 1048576;
// Maximum bytes of overlaps in grandparent (i.e., level+2) before we
// stop building a single file in a level->level+1 compaction.
int64_t kMaxGrandParentOverlapBytes = 10 * kTargetFileSize;

// Maximum number of bytes in all compacted files.  We avoid expanding
// the lower level file set of a compaction if it would make the
// total compaction cover more than this many bytes.
// Paul: use dynamic MaxExpandedCompactionByteSizeForLevel instead
// int64_t kExpandedCompactionByteSizeLimit = 25 * kTargetFileSize;

namespace leveldb {


static double MaxBytesForLevel(int level) {
  // Note: the result for level zero is not really used since we set
  // the level-0 compaction threshold based on number of files.
  //double result = 10 * 1048576.0;  // Result for both level-0 and level-1
  double result = 2 * kTargetFileSize;  // Result for both level-0 and level-1
  while (level > 1) {
    result *= 10;
    level--;
  }
  return result;
}

static uint64_t MaxFileSizeForLevel(int level) {
  // Use maximum file size for merge-compactions during idle
  if (level > 3) {
    return kTargetFileSize*16;
  } else {
    return kTargetFileSize * (1 << level);  // We do vary per level to reduce number of files.
  }
}

static uint64_t MaxExpandedCompactionByteSizeForLevel(int level) {
  return MaxFileSizeForLevel(level+2);
}

static int64_t TotalFileSize(const std::vector<FileMetaData*>& files) {
  int64_t sum = 0;
  for (size_t i = 0; i < files.size(); i++) {
    sum += files[i]->file_size;
  }
  return sum;
}

namespace {
std::string IntSetToString(const std::set<uint64_t>& s) {
  std::string result = "{";
  for (std::set<uint64_t>::const_iterator it = s.begin();
       it != s.end();
       ++it) {
    result += (result.size() > 1) ? "," : "";
    result += NumberToString(*it);
  }
  result += "}";
  return result;
}
}  // namespace

Version::~Version() {
  assert(refs_ == 0);
  // Drop references to files
  for (int level = 0; level < config::kNumLevels; level++) {
    for (size_t i = 0; i < files_[level].size(); i++) {
      FileMetaData* f = files_[level][i];
      assert(f->refs > 0);
      f->refs--;
      if (f->refs <= 0) {
        delete f;
      }
    }
  }
}

// Find the largest index whose smallest key <= 'key'
// By finding the earliest index whose smallest key > 'key'
// and returning the previous index.
int FindFile(const InternalKeyComparator& icmp,
             const std::vector<FileMetaData*>& files,
             const Slice& key) {
  uint32_t left = 0;
  uint32_t right = files.size();
  while (left < right) {
    int32_t mid = (left + right) / 2;
    const FileMetaData* f = files[mid];
    if (icmp.InternalKeyComparator::Compare(f->smallest.Encode(), key) > 0) {
      // Key at "mid.smallest" is > "target".  Therefore all
      // files after "mid" are uninteresting.
      right = mid;
    } else {
      // Key at "mid.smallest" is <= "target".  Therefore all files
      // at or before "mid" are uninteresting.
      left = mid + 1;
    }
  }
  return (right-1);
}

// f->largest accessed only for L0 or test list
static bool AfterFile(const Comparator* ucmp,
                      const Slice* user_key, const FileMetaData* f) {
  // NULL user_key occurs before all keys and is therefore never after *f
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->largest.user_key()) > 0);
}

static bool BeforeFile(const Comparator* ucmp,
                       const Slice* user_key, const FileMetaData* f) {
  // NULL user_key occurs after all keys and is therefore never before *f
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->smallest.user_key()) < 0);
}

bool SomeFileOverlapsRange(
	const InternalKeyComparator& icmp,
	bool disjoint_sorted_files,
	const std::vector<FileMetaData*>& files,
	const Slice* smallest_user_key,
	const Slice* largest_user_key) {
  const Comparator* ucmp = icmp.user_comparator();
  if (!disjoint_sorted_files) {
    // Need to check against all files
    for (size_t i = 0; i < files.size(); i++) {
      const FileMetaData* f = files[i];
      if (AfterFile(ucmp, smallest_user_key, f) ||
          BeforeFile(ucmp, largest_user_key, f)) {
        // No overlap
      } else {
        return true;  // Overlap
      }
    }
    return false;
  }

  if (files.size() == 0){
    return false;
  }
  // Binary search over file list
  uint32_t index = 0;
  if (smallest_user_key == NULL && largest_user_key == NULL) {
    return true; //Overlap
  }
  else if (smallest_user_key == NULL && largest_user_key != NULL) {
    const Slice file_start = files[0]->smallest.user_key();
    return (ucmp->Compare(*largest_user_key, file_start) >= 0);
  }
  else if (smallest_user_key != NULL && largest_user_key == NULL) {
    index = files.size() - 1;
  }
  else {
    // Find the last possible index for largest_user_key
    InternalKey large(*largest_user_key, 0,kValueTypeForSeek);
    index = FindFile(icmp, files, large.Encode());
  }

  if (index >= files.size()) {
    // end of range is before all files, so no overlap.
    return false;
  }

  return !AfterFile(ucmp, smallest_user_key, files[index]);
}

// An internal iterator.  For a given version/level pair, yields
// information about the files in the level.  For a given entry, key()
// is the smallest key (not accessed anywhere yet; so can be either smallest
// or largest) that occurs in the file, and value() is an
// 16-byte value containing the file number and file size, both
// encoded using EncodeFixed64.
class Version::LevelFileNumIterator : public Iterator {
 public:

  LevelFileNumIterator(const InternalKeyComparator& icmp,
                       const std::vector<FileMetaData*>* flist)
      : icmp_(icmp),
        flist_(flist),
        index_(flist->size()) {        // Marks as invalid
  }

/*
  LevelFileNumIterator(const InternalKeyComparator& icmp,
                       const std::vector<FileMetaData*>* flist, TableCache* tableCache, const ReadOptions& options)
      : icmp_(icmp),
        flist_(flist),
        index_(flist->size()),
        tableCache_(tableCache),
        options_(options) {        // Marks as invalid
  }
*/
  virtual bool Valid() const {
    return index_ < flist_->size();
  }

  virtual void Seek(const Slice& target) {
    index_ = FindFile(icmp_, *flist_, target);
    // Since FindFile now uses the smallest instead of largest
    // start with the first file even if the target was found to be
    // < the first file's smallest
    if (index_ == -1){
        index_ = 0;
    }
  }
  virtual void SeekToFirst() { index_ = 0; }
  virtual void SeekToLast() {
    index_ = flist_->empty() ? 0 : flist_->size() - 1;
  }
  virtual void Next() {
    assert(Valid());
    index_++;
  }
  virtual void Prev() {
    assert(Valid());
    if (index_ == 0) {
      index_ = flist_->size();  // Marks as invalid
    } else {
      index_--;
    }
  }
  Slice key() const {
    assert(Valid());
    return (*flist_)[index_]->smallest.Encode();
  }
  Slice value() const {
    assert(Valid());
    EncodeFixed64(value_buf_, (*flist_)[index_]->number);
    EncodeFixed64(value_buf_+ 8, (*flist_)[index_]->file_size);
    return Slice(value_buf_, sizeof(value_buf_));
  }
  virtual Status status() const { return Status::OK(); }
 private:
  const InternalKeyComparator icmp_;
  const std::vector<FileMetaData*>* const flist_;
  uint32_t index_;
  // Backing store for value().  Holds the file number and size.
  mutable char value_buf_[16];
};

static Iterator* GetFileIterator(void* arg,
                                 const ReadOptions& options,
                                 const Slice& file_value,
                                 char** buff = NULL) {
  TableCache* cache = reinterpret_cast<TableCache*>(arg);
  if (file_value.size() != 16) {
    return NewErrorIterator(
        Status::Corruption("FileReader invoked with unexpected value"));
  } else {
    return cache->NewIterator(options,
                              DecodeFixed64(file_value.data()),
                              DecodeFixed64(file_value.data() + 8),
			      DecodeFixed32(file_value.data() + 16));
  }
}

Iterator* Version::NewConcatenatingIterator(const ReadOptions& options,
                                            int level) const {
  return NewTwoLevelIterator(
      new LevelFileNumIterator(vset_->icmp_, &files_[level]),
      &GetFileIterator, vset_->table_cache_, options);
}

void Version::AddIterators(const ReadOptions& options,
                           std::vector<Iterator*>* iters) {
  // Merge all level zero files together since they may overlap
  for (size_t i = 0; i < files_[0].size(); i++) {
    iters->push_back(
        vset_->table_cache_->NewIterator(
            options, files_[0][i]->number, files_[0][i]->file_size, 0));
  }

  // For levels > 0, we can use a concatenating iterator that sequentially
  // walks through the non-overlapping files in the level, opening them
  // lazily.
  for (int level = 1; level < config::kNumLevels; level++) {
    if (!files_[level].empty()) {
      iters->push_back(NewConcatenatingIterator(options, level));
    }
  }
}

static void SaveValue(void* arg, const Slice& ikey, const Slice& v) {
  Saver* s = reinterpret_cast<Saver*>(arg);
  ParsedInternalKey parsed_key;
  if (!ParseInternalKey(ikey, &parsed_key)) {
    s->state = kCorrupt;
  } else {
    if (s->ucmp->Compare(parsed_key.user_key, s->user_key) == 0) {
      s->state = (parsed_key.type == kTypeValue) ? kFound : kDeleted;
      if (s->state == kFound) {

        char * pData = (char *)v.data();
	memcpy(s->value, (char*)&pData, sizeof(void*));

        s->size = v.size();
        s->seqNum = parsed_key.sequence;
      }
    }
  }
}

static bool NewestFirst(FileMetaData* a, FileMetaData* b) {
  return a->number > b->number;
}

void Version::ForEachOverlapping(Slice user_key, Slice internal_key,
                                 void* arg,
                                 bool (*func)(void*, int, FileMetaData*)) {
  // TODO(sanjay): Change Version::Get() to use this function.
  const Comparator* ucmp = vset_->icmp_.user_comparator();

  // Search level-0 in order from newest to oldest.
  std::vector<FileMetaData*> tmp;
  tmp.reserve(files_[0].size());
  for (uint32_t i = 0; i < files_[0].size(); i++) {
    // f->largest accessed only for L0
    FileMetaData* f = files_[0][i];
    if (ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
        ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
      tmp.push_back(f);
    }
  }
  if (!tmp.empty()) {

    std::sort(tmp.begin(), tmp.end(), NewestFirst);
    for (uint32_t i = 0; i < tmp.size(); i++) {
      if (!(*func)(arg, 0, tmp[i])) {
        return;
      }
    }
  }

  // Search other levels.
  for (int level = 1; level < config::kNumLevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

    // Binary search to find last index whose smallest key <= internal_key.
    uint32_t index = FindFile(vset_->icmp_, files_[level], internal_key);
    if (index < num_files) {
      FileMetaData* f = files_[level][index];
      if (ucmp->Compare(user_key, f->smallest.user_key()) < 0) {
        // All of "f" is past any data for user_key
      } else {
        if (!(*func)(arg, level, f)) {
          return;
        }
      }
    }
  }
}

Status Version::Get(const ReadOptions& options,
                    const LookupKey& k,
                    char* value,
                    GetStats* stats, bool using_bloom_filter, uint64_t* seqNum) {
  Slice ikey = k.internal_key();
  Slice user_key = k.user_key();
  const Comparator* ucmp = vset_->icmp_.user_comparator();
  Status s;
  stats->seek_file = NULL;
  stats->seek_file_level = -1;
  FileMetaData* last_file_read = NULL;
  int last_file_read_level = -1;

  // We can search level-by-level since entries never hop across
  // levels.  Therefore we are guaranteed that if we find data
  // in an smaller level, later levels are irrelevant.
  std::vector<FileMetaData*> tmp;
  FileMetaData* tmp2;
  uint32_t index;
  for (int level = 0; level < config::kNumLevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

    // Get the list of files to search in this level
    FileMetaData* const* files = &files_[level][0];
    if (level == 0) {
      // Level-0 files may overlap each other.  Find all files that
      // overlap user_key and process them in order from newest to oldest.
      tmp.reserve(num_files);
      for (uint32_t i = 0; i < num_files; i++) {
    // f->largest accessed only for L0
        FileMetaData* f = files[i];
        if (ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
            ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
          tmp.push_back(f);
        }
      }
      if (tmp.empty()) continue;

      std::sort(tmp.begin(), tmp.end(), NewestFirst);
      files = &tmp[0];
      num_files = tmp.size();
    } else {
      // Binary search to find last index whose smallest key <= ikey.
      //InternalKey search_key(user_key, 0, static_cast<ValueType>(0));
      InternalKey search_key(user_key, 0, kValueTypeForSeek);
      index = FindFile(vset_->icmp_, files_[level], search_key.Encode());
      if (index >= num_files) {
        files = NULL;
        num_files = 0;
      } else {
        tmp2 = files[index];
        files = &tmp2;
        num_files = 1;
      }
    }

    for (uint32_t i = 0; i < num_files; ++i) {
      if (last_file_read != NULL && stats->seek_file == NULL) {
        // We have had more than one seek for this read.  Charge the 1st file.
        stats->seek_file = last_file_read;
        stats->seek_file_level = last_file_read_level;
      }

      FileMetaData* f = files[i];
      last_file_read = f;
      last_file_read_level = level;

      Saver saver;
      saver.state = kNotFound;
      saver.ucmp = ucmp;
      saver.user_key = user_key;
      saver.value = value;
      saver.seqNum = 0;

      s = vset_->table_cache_->Get(options, f->number, f->file_size, f->level,
                                   ikey, &saver, SaveValue, using_bloom_filter);
      if (seqNum) {
          *seqNum = saver.seqNum;
      }
      if (!s.ok()) {
        return s;
      }
      switch (saver.state) {
        case kNotFound:
          break;      // Keep searching in other files
        case kFound:
          return s;
        case kDeleted: {
          s = Status::NotFound(Slice());  // Use empty error message for speed
          return s;
        }
        case kCorrupt:
          s = Status::Corruption("corrupted key for ", user_key);
          return s;
      }
    }
  }
  return Status::NotFound(Slice());  // Use an empty error message for speed
}

bool Version::UpdateStats(const GetStats& stats) {
  FileMetaData* f = stats.seek_file;
  if (f != NULL) {
    f->allowed_seeks--;
    if (f->allowed_seeks <= 0 && file_to_compact_ == NULL) {
      file_to_compact_ = f;
      file_to_compact_level_ = stats.seek_file_level;
      return true;
    }
  }
  return false;
}

bool Version::RecordReadSample(Slice internal_key) {
  ParsedInternalKey ikey;
  if (!ParseInternalKey(internal_key, &ikey)) {
    return false;
  }

  struct State {
    GetStats stats;  // Holds first matching file
    int matches;

    static bool Match(void* arg, int level, FileMetaData* f) {
      State* state = reinterpret_cast<State*>(arg);
      state->matches++;
      if (state->matches == 1) {
        // Remember first match.
        state->stats.seek_file = f;
        state->stats.seek_file_level = level;
      }
      // We can stop iterating once we have a second match.
      return state->matches < 2;
    }
  };

  State state;
  state.matches = 0;
  ForEachOverlapping(ikey.user_key, internal_key, &state, &State::Match);

  // Must have at least two matches since we want to merge across
  // files. But what if we have a single file that contains many
  // overwrites and deletions?  Should we have another mechanism for
  // finding such files?
  if (state.matches >= 2) {
    // 1MB cost is about 1 seek (see comment in Builder::Apply).
    return UpdateStats(state.stats);
  }
  return false;
}

void Version::Unref() {
  mutex_.Lock();
  assert(this != &vset_->dummy_versions_);
  assert(refs_ >= 1);
  --refs_;
  if (vset_->NumberOfVersions() == 1 && refs_ == 1 && TotalNumFiles() == 0) {
    vset_->UpdateDeletableValueFiles(this);
    if (vset_->HasDeletableValueFiles()) {
      vset_->DeallocateDeletableValueFiles();
      if (this->next_ ==  &vset_->dummy_versions_) {
        for (set<uint64_t>::iterator it = deletedValueFiles_.begin(); it != deletedValueFiles_.end(); ++it) {
          deletedValueFiles_.erase(*it);
        }
      }
      vset_->GetEnv()->Sync();
    }
  }
  if (refs_ == 0) {
    vset_->UpdateDeletableValueFiles(this);
    vset_->Remove(this);
    mutex_.Unlock();
    delete this;
  } else {
      mutex_.Unlock();
  }
}

bool Version::OverlapInLevel(int level,
                             const Slice* smallest_user_key,
                             const Slice* largest_user_key) {
  const std::vector<FileMetaData*>& files = files_[level];
  const Comparator* ucmp = vset_->icmp_.user_comparator();
  if (level == 0) {
    // Need to check against all files
    for (size_t i = 0; i < files.size(); i++) {
      const FileMetaData* f = files[i];
      if (AfterFile(ucmp, smallest_user_key, f) ||
          BeforeFile(ucmp, largest_user_key, f)) {
        // No overlap
      } else {
        return true;  // Overlap
      }
    }
    return false;
  }

  // For all other levels
  if (files.size() == 0){
    return false;
  }
  uint32_t index = 0;

  // NULL smallest user_key occurs before all keys
  // NULL largest user_key occurs after all keys
  if (smallest_user_key == NULL && largest_user_key == NULL) {
    // If there is any file in the level and both keys are NULL, then
    // there is overlap.
    return true;
  }
  else if (smallest_user_key == NULL && largest_user_key != NULL) {
    // If only the smallest is NULL, there is overlap if
    // largest >= the first file's smallest.
    const Slice file_start = files[0]->smallest.user_key();
    return (ucmp->Compare(*largest_user_key, file_start) >= 0);
  }
  else if (smallest_user_key != NULL && largest_user_key == NULL) {
    // If only the largest is NULL, there is overlap if
    // smallest <= the last file's largest. The actual
    // check happens below.
    index = files.size() - 1;
  }
  else {
    // Find the last possible index whose smallest <= largest_user_key
    // There is overlap if smallest <= the index's largest.
    InternalKey large(*largest_user_key, 0,kValueTypeForSeek);
    index = FindFile(vset_->icmp_, files, large.Encode());
  }

  if (index >= files.size()) {
    // end of range is before all files, so no overlap.
    return false;
  }

  FileMetaData* f = files[index];
  Iterator* iter = vset_->table_cache_->NewIterator(
		      ReadOptions(), f->number, f->file_size, f->level);
  iter->SeekToLast();
  InternalKey largest;
  largest.DecodeFrom(iter->key());
  const Slice file_limit = largest.user_key();
  bool overlap = (ucmp->Compare(*smallest_user_key, file_limit) <= 0);
  delete iter;
  return overlap;
}

int Version::PickLevelForMemTableOutput(
    const Slice& smallest_user_key,
    const Slice& largest_user_key) {
  int level = 0;
  if (!OverlapInLevel(0, &smallest_user_key, &largest_user_key)) {
    // Push to next level if there is no overlap in next level,
    // and the #bytes overlapping in the level after that are limited.
    InternalKey start(smallest_user_key, kMaxSequenceNumber, kValueTypeForSeek);
    InternalKey limit(largest_user_key, 0, static_cast<ValueType>(0));
    std::vector<FileMetaData*> overlaps;
    while (level < config::kMaxMemCompactLevel) {
      if (OverlapInLevel(level + 1, &smallest_user_key, &largest_user_key)) {
        break;
      }
      if (level + 2 < config::kNumLevels) {
        // Check that file does not overlap too many grandparent bytes.
        GetOverlappingInputs(level + 2, &start, &limit, &overlaps);

        const int64_t sum = TotalFileSize(overlaps);
        if (sum > kMaxGrandParentOverlapBytes) {
          break;
        }
      }
      level++;
    }
  }
  if (level == 1) {
      if (vset_->L1CompactionScore() > 2*config::kL0_StopWritesCompactionScore) {
          level = 0;
      }
  }
  return level;
}
FileMetaData* Version::FindFileAfterKey(InternalKey& key, vector<FileMetaData*>& files) {
    if (files.size() == 0) {
        return NULL;
    }
    uint32_t left = 0;
    uint32_t right = files.size();
    Slice sKey = key.Encode();
    int32_t mid = 0;
    while (left < right) {
       mid = (left + right) / 2;
       const FileMetaData* f = files[mid];
       if (this->vset_->icmp_.Compare(f->smallest.Encode(), sKey) > 0) {
          // Key at "mid.smallest" is > "target".  Therefore all
      // files after "mid" are uninteresting.
      right = mid;
    } else {
      // Key at "mid.smallest" is <= "target".  Therefore all files
      // at or before "mid" are uninteresting.
      left = mid + 1;
    }
  }
  if (right == files.size()) {
      right = files.size() - 1;
  }
  cout << __FILE__ << ": right = " << right << ", mid = " << mid << ", left = " << left << ", sizes = " << files.size() << endl;

  if (right >= 0 && right < files.size()) {
      return files[right];
  } else {
      return NULL;
  }
}

FileMetaData* Version::FindFileToCompact(InternalKey& intputLargest_0, int level) {
    if (level <= 0 || level >= config::kNumLevels) {
        return NULL;
    }
    return FindFileAfterKey(intputLargest_0, files_[level]);
}
// Store in "*inputs" all files in "level" that overlap [begin,end]
void Version::GetOverlappingInputs(
    int level,
    const InternalKey* begin,
    const InternalKey* end,
    std::vector<FileMetaData*>* inputs,
    uint64_t byte_size_limit) {

  assert(level >= 0);
  assert(level < config::kNumLevels);
  inputs->clear();
  Slice user_begin, user_end;
  if (begin != NULL) {
    user_begin = begin->user_key();
  }
  if (end != NULL) {
    user_end = end->user_key();
  }
  const Comparator* user_cmp = vset_->icmp_.user_comparator();
//*****DEBUG
///  if (user_cmp->Compare(user_begin, Slice("03000000000000000054")) >= 0) {
//    cout << " GetOverlappingInputs " << endl;
//    cout << "--- All file at level " << level << endl;
//    cout << "User begin: " << begin->DebugString() << endl;
//    for (size_t i = 0; i < files_[level].size(); ) {
//      FileMetaData* f = files_[level][i++];
//      cout << "SMALLEST " << f->smallest.DebugString() << endl;

//      Iterator* iter = vset_->table_cache_->NewIterator(
//              ReadOptions(), f->number, f->file_size);
//      cout << "1. FILE NUMBER " << f->number << endl;
//      iter->SeekToFirst();
//      while (iter->Valid()) {
//          InternalKey iKey;
//	  iKey.DecodeFrom(iter->key());
//	  cout << iKey.DebugString() << endl;
//	  iter->Next();
//      }
//      delete iter;
//    }
//    cout << "--- End All file at level " << level << endl;
//  }
//******DEBUG
  if (level == 0) {
    for (size_t i = 0; i < files_[level].size(); ) {
    // f->largest accessed only for L0
      FileMetaData* f = files_[level][i++];
      const Slice file_start = f->smallest.user_key();
      const Slice file_limit = f->largest.user_key();
      if (begin != NULL && user_cmp->Compare(file_limit, user_begin) < 0) {
      // "f" is completely before specified range; skip it
      } else if (end != NULL && user_cmp->Compare(file_start, user_end) > 0) {
      // "f" is completely after specified range; skip it
      } else {
        inputs->push_back(f);
        if (level == 0) {
          // Level-0 files may overlap each other.  So check if the newly
          // added file has expanded the range.  If so, restart search.
          if (begin != NULL && user_cmp->Compare(file_start, user_begin) < 0) {
            user_begin = file_start;
            inputs->clear();
            i = 0;
          } else if (end != NULL && user_cmp->Compare(file_limit, user_end) > 0) {
            user_end = file_limit;
            inputs->clear();
            i = 0;
          }
        }
      }
    }
  }
  else {
    uint32_t index = 0;

    if (begin != NULL){
      // Binary search to find last index whose smallest key <= begin_ikey.
      InternalKey begin_ikey(user_begin, 0,kValueTypeForSeek);
      index = FindFile(vset_->icmp_, files_[level], begin_ikey.Encode());
      if (index >= files_[level].size()) {
          if (files_[level].size() > 0){
              index = 0;
          }
          else {
              return;
          }
      }
      // User_begin could be after index's largest; check the iterator and push conditionally
      // This is fine because it is for only one file and only during compaction checks;
      // index access for a couple of files to avoid a file comapaction is not a problem.
      FileMetaData* f = files_[level][index];
      Iterator* iter = vset_->table_cache_->NewIterator(
              ReadOptions(), f->number, f->file_size, f->level);
//*****DEBUG
//      cout << "FILE NUMBER " << f->number << endl;
//      iter->SeekToFirst();
//      while (iter->Valid()) {
//          InternalKey iKey;
//	  iKey.DecodeFrom(iter->key());
//	  cout << iKey.DebugString() << endl;
//	  iter->Next();
//      }
//*****DEBUG
      iter->SeekToLast();
      InternalKey largest;
      largest.DecodeFrom(iter->key());
      const Slice file_limit = largest.user_key();
      const Slice file_start = f->smallest.user_key();
      if (user_cmp->Compare(user_begin, file_limit) <= 0) {
          if (end == NULL) {
              inputs->push_back(f);
          }
          else if (user_cmp->Compare(user_end, file_start) >= 0) {
              inputs->push_back(f);
          }
      }
      delete iter;
      index++;
    }

    uint64_t total_size = 0;
    for (size_t i = 0; i < inputs->size(); i++) {
      total_size += inputs->at(i)->file_size;
    }
    for (size_t i = index; i < files_[level].size(); i++){
      FileMetaData* f = files_[level][i];
      total_size += f->file_size;
      const Slice file_start = f->smallest.user_key();
      if ((end != NULL && user_cmp->Compare(file_start, user_end) > 0) || total_size > byte_size_limit) {
          return;
      }
      inputs->push_back(f);
    }
  }
}

std::string Version::DebugString() const {
  std::string r;
  try {
    for (int level = 0; level < config::kNumLevels; level++) {
      // E.g.,
      //   --- level 1 ---
      //   17:123['a' .. 'd']
      //   20:43['e' .. 'g']
      r.append("--- level ");
      AppendNumberTo(&r, level);
      r.append(" ---\n");
      const std::vector<FileMetaData*>& files = files_[level];
      for (size_t i = 0; i < files.size(); i++) {
        r.push_back(' ');
        AppendNumberTo(&r, files[i]->number);
        r.push_back(':');
        AppendNumberTo(&r, files[i]->file_size);
        r.append("[");
        r.append(files[i]->smallest.DebugString());
        r.append(" .. ");
        if (level > 0) {
  	r.append("]\n");
        }
        else {
      // f->largest accessed only for L0
          r.append(files[i]->largest.DebugString());
          r.append("]\n");
        }
      }
    }
  } catch (const std::bad_alloc&) {
    r = "Version.DebugString() is too large to be generated!";
  }
  return r;
}

// A helper class so we can efficiently apply a whole sequence
// of edits to a particular state without creating intermediate
// Versions that contain full copies of the intermediate state.
class VersionSet::Builder {
 private:
  // Helper to sort by v->files_[file_number].smallest
  struct BySmallestKey {
    const InternalKeyComparator* internal_comparator;

    bool operator()(FileMetaData* f1, FileMetaData* f2) const {
      int r = internal_comparator->Compare(f1->smallest, f2->smallest);
      if (r != 0) {
        return (r < 0);
      } else {
        // Break ties by file number
        return (f1->number < f2->number);
      }
    }
  };

  typedef std::set<FileMetaData*, BySmallestKey> FileSet;
  typedef std::map<uint64_t, FileMetaData*> FileMap;

  struct LevelState {
    std::set<uint64_t> deleted_files;
    FileSet* added_files;
    FileMap* added_files_index;
  };

  VersionSet* vset_;
  Version* base_;
  LevelState levels_[config::kNumLevels];
  set<uint64_t> deletedValueFiles_;

 public:
  // Initialize a builder with the files from *base and other info from *vset
  Builder(VersionSet* vset, Version* base)
      : vset_(vset),
        base_(base) {
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      levels_[level].added_files = new FileSet(cmp);
      levels_[level].added_files_index = new FileMap();
    }
  }

  ~Builder() {
    for (int level = 0; level < config::kNumLevels; level++) {
      const FileSet* added = levels_[level].added_files;
      std::vector<FileMetaData*> to_unref;
      to_unref.reserve(added->size());
      for (FileSet::const_iterator it = added->begin();
          it != added->end(); ++it) {
        to_unref.push_back(*it);
      }
      delete added;

      // The added files index contains the same FileMetaData instances as
      // the added_files set, so only the index (and not the contents) should
      // be freed
      delete levels_[level].added_files_index;

      for (uint32_t i = 0; i < to_unref.size(); i++) {
        FileMetaData* f = to_unref[i];
        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
    }
    base_->Unref();
  }

  // Apply all of the edits in *edit to the current state.
  void Apply(VersionEdit* edit) {
    // Update compaction pointers
    for (size_t i = 0; i < edit->compact_pointers_.size(); i++) {
      const int level = edit->compact_pointers_[i].first;
      vset_->compact_pointer_[level] =
          edit->compact_pointers_[i].second.Encode().ToString();
    }

    // Delete files
    const VersionEdit::DeletedFileSet& del = edit->deleted_files_;
    for (VersionEdit::DeletedFileSet::const_iterator iter = del.begin();
         iter != del.end();
         ++iter) {
      const int level = iter->first;
      const uint64_t number = iter->second;
      levels_[level].deleted_files.insert(number);

      // If there's an added file from earlier in the MANIFEST with the same number, it can be
      // discarded since now we know it's deleted. This greatly reduces memory usage during recovery
      FileMap::iterator it = levels_[level].added_files_index->find(number);
      if (it != levels_[level].added_files_index->end()) {
        FileMetaData* f = it->second;
        levels_[level].added_files->erase(f);
        levels_[level].added_files_index->erase(it);

        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
    }

    // Add new files
    for (size_t i = 0; i < edit->new_files_.size(); i++) {
      const int level = edit->new_files_[i].first;
      FileMetaData* f = new FileMetaData(edit->new_files_[i].second);
      f->refs = 1;

      // We arrange to automatically compact this file after
      // a certain number of seeks.  Let's assume:
      //   (1) One seek costs 10ms
      //   (2) Writing or reading 1MB costs 10ms (100MB/s)
      //   (3) A compaction of 1MB does 25MB of IO:
      //         1MB read from this level
      //         10-12MB read from next level (boundaries may be misaligned)
      //         10-12MB written to next level
      // This implies that 25 seeks cost the same as the compaction
      // of 1MB of data.  I.e., one seek costs approximately the
      // same as the compaction of 40KB of data.  We are a little
      // conservative and allow approximately one seek for every 16KB
      // of data before triggering a compaction.
      f->allowed_seeks = (f->file_size / 16384);
      if (f->allowed_seeks < 100) f->allowed_seeks = 100;

      levels_[level].deleted_files.erase(f->number);
      levels_[level].added_files->insert(f);
      levels_[level].added_files_index->insert(std::pair<uint64_t, FileMetaData*>(f->number, f));
    }
    // Delete value files

    for (set<uint64_t>::iterator it = edit->deletedValueFiles_.begin(); it != edit->deletedValueFiles_.end(); ++it) {
        deletedValueFiles_.insert(*it);
    }
  }

  // Save the current state in *v.
  void SaveTo(Version* v) {
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      // Merge the set of added files with the set of pre-existing files.
      // Drop any deleted files.  Store the result in *v.
      const std::vector<FileMetaData*>& base_files = base_->files_[level];
      std::vector<FileMetaData*>::const_iterator base_iter = base_files.begin();
      std::vector<FileMetaData*>::const_iterator base_end = base_files.end();
      const FileSet* added = levels_[level].added_files;
      v->files_[level].reserve(base_files.size() + added->size());
      for (FileSet::const_iterator added_iter = added->begin();
           added_iter != added->end();
           ++added_iter) {
        // Add all smaller files listed in base_
        for (std::vector<FileMetaData*>::const_iterator bpos
                 = std::upper_bound(base_iter, base_end, *added_iter, cmp);
             base_iter != bpos;
             ++base_iter) {
          MaybeAddFile(v, level, *base_iter);
        }
        MaybeAddFile(v, level, *added_iter);
      }

      // Add remaining base files
      for (; base_iter != base_end; ++base_iter) {
        MaybeAddFile(v, level, *base_iter);
      }

#ifdef KDEBUG
      // Make sure there is no overlap in levels > 0
      // Checking order is good enough
      if (level > 0) {
        for (uint32_t i = 1; i < v->files_[level].size(); i++) {
          const InternalKey& prev_begin = v->files_[level][i-1]->smallest;
          const InternalKey& this_begin = v->files_[level][i]->smallest;
          if (vset_->icmp_.Compare(prev_begin, this_begin) >= 0) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    prev_begin.DebugString().c_str(),
                    this_begin.DebugString().c_str());
            abort();
          }
        }
      }
#endif
    }
    // Handle value files
    v->AddDeletableValueFiles(deletedValueFiles_);
  }

  void MaybeAddFile(Version* v, int level, FileMetaData* f) {
    if (levels_[level].deleted_files.count(f->number) > 0) {
      // File is deleted: do nothing
      // Note that this can still happen even with the optimization
      // that prunes the in-memory sets of added files: this
      // path will still be hit if the file was added in an earlier
      // version
   } else {
      std::vector<FileMetaData*>* files = &v->files_[level];
      if (level > 0 && !files->empty()) {
        // Must not overlap
	// Checking for smallest should be good enough if Compaction works fine.
        assert(vset_->icmp_.Compare((*files)[files->size()-1]->smallest,
                                    f->smallest) < 0);
      }
      f->refs++;
      files->push_back(f);
      v->levelBytes_[level] += f->file_size;
    }
  }
};

VersionSet::VersionSet(const std::string& dbname,
                       const Options* options,
                       TableCache* table_cache,
                       const InternalKeyComparator* cmp)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      next_file_number_(2),
      manifest_file_number_(0),  // Filled by Recover()
      last_sequence_(0),
      log_number_(0),
      prev_log_number_(0),
      descriptor_file_(NULL),
      descriptor_log_(NULL),
      dummy_versions_(this),
      current_(NULL) {
    l1CompactionScore_ = 0;
    nNewManifestSegments_ = 0;
    AppendVersion(new Version(this));
}

VersionSet::~VersionSet() {
  current_->Unref();
  assert(dummy_versions_.next_ == &dummy_versions_);  // List must be empty
  if (descriptor_file_) {
//       descriptor_file_->unsubscribe(this);
  }
  delete descriptor_log_;
  delete descriptor_file_;
}

void VersionSet::AppendVersion(Version* v) {
  // Make "v" current
  assert(v->refs_ == 0);
  assert(v != current_);
  Version* oldCurrent = NULL;
  if (current_ != NULL) {
    v->manifest_compaction_score_ = this->current_->manifest_compaction_score_;
    oldCurrent = current_;
  }
  v->Ref();
  mutex_.Lock();
  current_ = v;

  // Append to linked list
  v->prev_ = dummy_versions_.prev_;
  v->next_ = &dummy_versions_;
  v->prev_->next_ = v;
  v->next_->prev_ = v;
  mutex_.Unlock();
  if (oldCurrent) {
      oldCurrent->Unref();
  }
}

Status VersionSet::LogAndApply(VersionEdit* edit, port::Mutex* mu) {
  mu->AssertHeld();
  if (edit->has_log_number_) {
    assert(edit->log_number_ >= log_number_);
    assert(edit->log_number_ < next_file_number_);
  } else {
    edit->SetLogNumber(log_number_);
  }

  if (!edit->has_prev_log_number_) {
    edit->SetPrevLogNumber(prev_log_number_);
  }

  edit->SetNextFile(next_file_number_);
  edit->SetLastSequence(last_sequence_);
#ifdef KDEBUG
    uint64_t start, end;
    start = DriveEnv::getInstance()->NowMicros();
#endif
  Version* v = new Version(this);
  {
    Builder builder(this, current());
    builder.Apply(edit);

#ifdef KDEBUG
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----TIME TO APPLY IN LOGANDAPPLY " << (end-start) << endl;
    start = end;
#endif

    builder.SaveTo(v);
#ifdef KDEBUG
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----TIME TO SAVE IN LOGANDAPPLY " << (end-start) << endl;
    start = end;
#endif


  }
  Finalize(v);
#ifdef KDEBUG
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----TIME TO FINALIZE IN LOGANDAPPLY " << (end-start) << endl;
    start = end;
#endif

  // Initialize new descriptor log file if necessary by creating
  // a temporary file that contains a snapshot of the current version.
  std::string new_manifest_file;
  Status s;
  if (descriptor_log_ == NULL) {
    // No reason to unlock *mu here since we only hit this path in the
    // first call to LogAndApply (when opening the database).
    assert(descriptor_file_ == NULL);
    new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
    edit->SetNextFile(next_file_number_);
    WritableFile* tmpFile;
    Log(options_->info_log, 5, "Creating new manifest %s", new_manifest_file.c_str());
    s = env_->NewWritableFile(new_manifest_file, &tmpFile);
    descriptor_file_ = (ManifestWritableFile*)tmpFile;
    if (s.ok()) {
      descriptor_log_ = new log::Writer(descriptor_file_, env_, dbname_);
      s = WriteSnapshot(descriptor_log_);
      v->manifest_compaction_score_ = -1;
    }
  }

    // Write new record to MANIFEST log
    if (s.ok()) {
        std::string record;
        edit->EncodeCounters(&record);
        edit->EncodeCompactPointers(&record);
        edit->EncodeDeletedFiles(&record);
        descriptor_log_->AddRecord(record);
        // Store new files to MANIFEST
        size_t offset = 0;
        while (s.ok() && offset < edit->new_files_.size()) {
          std::string record;
          size_t nCount = edit->EncodeNewFiles(&record, offset, MAX_RECORD_TO_ENCODE_AT_ONE_TIME);
          s = descriptor_log_->AddRecord(record);
          offset += nCount;
        };
#ifdef KDEBUG
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----TIME TO ADDRECORD IN LOGANDAPPLY " << (end-start) << endl;
    start = end;
#endif

      if (s.ok()) {
        s = descriptor_file_->Sync();
        if (s.ok()) {
            Log(options_->info_log, 5, "MANIFEST write: %s\n", s.ToString().c_str());
            s = env_->Sync();
            if (!s.ok()) {
                Log(options_->info_log, 0, "Rolling back MANIFEST");
                descriptor_file_->rollback();
            }
        } else {
            Log(options_->info_log, 0, "Failed to sync manifest");
        }
      }
#ifdef KDEBUG
    end = DriveEnv::getInstance()->NowMicros();
    cout << " ----TIME TO SYNC MANIFEST IN LOGANDAPPLY " << (end-start) << endl;
    start = end;
#endif

    }

    // If we just created a new descriptor file, install it by writing a
    // new CURRENT file that points to it.
    if (s.ok() && !new_manifest_file.empty()) {
      s = SetCurrentFile(env_, dbname_, manifest_file_number_);
      if (s.ok()) {
          s = env_->Sync();
      }
    }

  // Install the new version
  if (s.ok()) {
    AppendVersion(v);
    log_number_ = edit->log_number_;
    prev_log_number_ = edit->prev_log_number_;
//    ComputeManifestCompactionScore();
  } else {
    delete v;
    if (!new_manifest_file.empty()) {
       Log(options_->info_log, 5, "Failed to create new manifest %s", new_manifest_file.c_str());
      delete descriptor_log_;
      delete descriptor_file_;
      descriptor_log_ = NULL;
      descriptor_file_ = NULL;
      env_->DeleteFile(new_manifest_file);
    }
  }
#ifdef KDEBUG
    end = DriveEnv::getInstance()->NowMicros();
    cout << " TIME TO UPDATE MANIFEST IN LOGANDAPPLY " << (end-start) << endl;
#endif

  return s;
}

Status VersionSet::Recover() {
  struct LogReporter : public log::Reader::Reporter {
    Status* status;
    virtual void Corruption(size_t bytes, const Status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  // Read "CURRENT" file, which contains a pointer to the current manifest file
  std::string sCurrent;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_), &sCurrent);
  if (!s.ok()) {
    return s;
  }
  if (sCurrent.empty() || sCurrent[sCurrent.size()-1] != '\n') {
    return Status::Corruption("CURRENT file does not end with newline");
  }
  sCurrent.resize(sCurrent.size() - 1);

  std::string dscname = dbname_ + "/" + sCurrent;
  SequentialFile* file;
  s = env_->NewSequentialFile(dscname, &file);
  if (!s.ok()) {
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  Builder builder(this, current());

  {
    LogReporter reporter;
    reporter.status = &s;
    log::Reader reader(file, &reporter, true/*checksum*/, 0/*initial_offset*/);
    Slice record;
    std::string scratch;
    while (reader.ManifestReadRecord(&record, &scratch) && s.ok()) {
      VersionEdit edit;
      s = edit.DecodeFrom(record);
      if (s.ok()) {
        if (edit.has_comparator_ &&
            edit.comparator_ != icmp_.user_comparator()->Name()) {
          s = Status::InvalidArgument(
              edit.comparator_ + " does not match existing comparator ",
              icmp_.user_comparator()->Name());
        }
      }

      if (s.ok()) {
        builder.Apply(&edit);
      }

      if (edit.has_log_number_) {
        log_number = edit.log_number_;
        have_log_number = true;
      }

      if (edit.has_prev_log_number_) {
        prev_log_number = edit.prev_log_number_;
        have_prev_log_number = true;
      }

      if (edit.has_next_file_number_) {
        next_file = edit.next_file_number_;
        have_next_file = true;

      }

      if (edit.has_last_sequence_) {
        last_sequence = edit.last_sequence_;
        have_last_sequence = true;
      }
    }
  }
  delete file;
  file = NULL;

  if (s.ok()) {
    if (!have_next_file) {
      s = Status::Corruption("no meta-nextfile entry in descriptor");
    } else if (!have_log_number) {
      s = Status::Corruption("no meta-lognumber entry in descriptor");
    } else if (!have_last_sequence) {
      s = Status::Corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }

    MarkFileNumberUsed(prev_log_number);
    MarkFileNumberUsed(log_number);
  }

  if (s.ok()) {
    Version* v = new Version(this);
    builder.SaveTo(v);
    // Install recovered version
    Finalize(v);
    AppendVersion(v);
    manifest_file_number_ = next_file;
    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    log_number_ = log_number;
    prev_log_number_ = prev_log_number;
  }
  return s;
}

void VersionSet::MarkFileNumberUsed(uint64_t number) {
  if (next_file_number_ <= number) {
    next_file_number_ = number + 1;
  }
}

void VersionSet::Finalize(Version* v) {
  static int nCompacts = 0;
  static const int MAX_COMPACTS = 2;

  double l0Score = 0;
  double l1Score = 0;
  // Precomputed best level for next compaction
  int best_level = -1;
  double best_score = -1;
  int l0NumFiles = 0;
  double score = 0;
  uint64_t totalBytes = 1;
  uint64_t availDiskBytes = 1;
      // Compute avail disk space
  uint64_t usedBytes = 0;
  this->env_->GetCapacity(&totalBytes, &usedBytes);
  uint64_t spaceLeft = totalBytes - usedBytes;

  for (int level = 0; level < config::kNumLevels-1; ++level) {
      if (excludedCompactionLevels_.find(level) != excludedCompactionLevels_.end()) {
          if (level == 1) {
              // We still want to update level 1 compaction score
              const uint64_t level_bytes = v->levelBytes_[level];
              score = static_cast<double>(level_bytes) / MaxBytesForLevel(level);
              // Multiply score by priority factor, [6-0] for levels [0-6] respectively
              l1CompactionScore_ = (config::kNumLevels - 1 - level)*score;
          }
          continue;
      }
    if (level == 0) {
      // We treat level-0 specially by bounding the number of files
      // instead of number of bytes for two reasons:
      //
      // (1) With larger write-buffer sizes, it is nice not to do too
      // many level-0 compactions.
      //
      // (2) The files in level-0 are merged on every read and
      // therefore we wish to avoid too many files when the individual
      // file size is small (perhaps because of a small write-buffer
      // setting, or very high compression ratios, or lots of
      // overwrites/deletions).
      //
      // Paul: Actually disable level 0 compactions when there are less than
      //       kL0_CompactionTrigger L0 tables, the cumulative size of L0 tables
      //       is below kTargetFileSize and kinetic_idle is not set.
      //       This prevents continues 1@0 1@1 compactions when L0 sst tables
      //       are often only a few KB (value-out).
      l0NumFiles = v->files_[0].size();
      if (l0NumFiles > l0MaxSeen_) {
        l0MaxSeen_ = l0NumFiles ;
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__
           << " MAX NUM OF FILES AT L0 " << l0MaxSeen_ ;
           leveldb::Status::IOError(ss.str());
      }
      if (l0NumFiles >= config::kL0_CompactionTrigger || v->levelBytes_[0] > kTargetFileSize) {
        score = l0NumFiles / static_cast<double>(config::kL0_CompactionTrigger);
        // Multiply score by priority factor, [6-0] for levels [0-6] respectively
        score *= (config::kNumLevels - 1);
        l0Score = score;
      }
    } else {
      // Compute the ratio of current size to size limit.
      const uint64_t level_bytes = v->levelBytes_[level];
      score = static_cast<double>(level_bytes) / MaxFileSizeForLevel(level);
      // Multiply score by priority factor, [6-0] for levels [0-6] respectively
      score *= (config::kNumLevels - 1 - level);
      if (level == 1) {
        l1Score = score;
        l1CompactionScore_ = score;
      }
    }

    if (score > 0 && score > best_score) {
      best_level = level;
      best_score = score;
    }
  }
  // Best low levels are always selected to compact
  if (best_level <= 1) {
      // Reset nCompacts for low levels (0 and 1)
      nCompacts = -1;  // no count down for low levels
  } else if (nCompacts == -1) {
      // New best high level (2 or higher) after the last
      // best low level compaction, reset nCompacts
      nCompacts = MAX_COMPACTS;
  } else if (nCompacts > 0) {
      // Still high level compaction. Allow high levels to continue compacting
      // until nCompacts counting down to 0
      --nCompacts;
  } else {
      // After a continuous MAX_COMPACTS with high levels, we always
      // check level 0 stop trigger point to yield compaction to level 0 or 1

      // if level 0 is at stop trigger
      //     select level 0 or 1 to compact
      // else
      //     reset nCompacts to compact the best high level
      if (l0NumFiles >= config::kL0_StopWritesTrigger) {
          if (l1Score > l0Score) {
              best_score = l1Score;
              best_level = 1;
          } else {
              best_score = l0Score;
              best_level = 0;
          }
          nCompacts = -1;
      } else {
          nCompacts = MAX_COMPACTS;
      }
  }

  v->compaction_level_ = best_level;
  v->compaction_score_ = best_score;
  v->last_manifest_size_ = current_->last_manifest_size_;
}

void VersionSet::ForceCompaction() {
    Version* version = this->current();
    if (version->compaction_level_ < 0 || version->compaction_score_ < 1) {
        for (int i = 0 ; i < config::kNumLevels - 1; i++) {
            if (version->files_[i].size() != 0) {
                version->compaction_level_ = i;
                version->compaction_score_ = 1;
                break;
            }
        }
    }
    version->Unref();
}

Status VersionSet::CompactManifest() {
    if (!this->env_->isSuperblockSyncable()) {
        current_->manifest_compaction_score_ = -1;
        this->nNewManifestSegments_ = 0;
        return Status::OK();
    }
    Log(options_->info_log, 0, "==== Compacting MANIFEST...\n");
    Log(options_->info_log, 0,"==== current_->manifest_compaction_score_ = %f\n", current_->manifest_compaction_score_);
    uint64_t oldManifestFileNum = manifest_file_number_;
    uint64_t newManifestFileNum = NewFileNumber();
    std::string oldManifestFileName = DescriptorFileName(dbname_, manifest_file_number_);
    std::string newManifestFileName = DescriptorFileName(dbname_, newManifestFileNum);
    ManifestWritableFile* newDescriptorFile = NULL;
    WritableFile* tmpFile = NULL;
    log::Writer* newDescriptorLog = NULL;
    Status status = env_->NewWritableFile(newManifestFileName, &tmpFile);
    newDescriptorFile = (ManifestWritableFile*)tmpFile;
    if (status.ok()) {
       newDescriptorLog = new log::Writer(newDescriptorFile, env_, dbname_);
       status = WriteSnapshot(newDescriptorLog);
   }
   if (status.ok()) {
       VersionEdit edit;
       edit.SetPrevLogNumber(prev_log_number_);
       edit.SetLogNumber(log_number_);
       edit.SetNextFile(next_file_number_);
       edit.SetLastSequence(last_sequence_);
       std::string record;
       edit.EncodeTo(&record);
       status = newDescriptorLog->AddRecord(record);
   }
   if (status.ok()) {
       status = newDescriptorFile->Sync();
       if (status.ok()) {
           status = env_->Sync();
       }
   }

   if (status.ok()) {
       status = SetCurrentFile(env_, dbname_, newManifestFileNum);
   }
   if (status.ok()) {
       manifest_file_number_ = newManifestFileNum;
       WritableFile* oldDescriptorFile = descriptor_file_;
       log::Writer* oldDescriptorLog = descriptor_log_;
       newDescriptorFile->subscribe(descriptor_file_->getSubscribers());
       descriptor_file_ = newDescriptorFile;
       descriptor_log_ = newDescriptorLog;
       delete oldDescriptorLog;
       delete oldDescriptorFile;
       env_->DeleteFile(oldManifestFileName);
       current_->manifest_compaction_score_ = -1;
       this->nNewManifestSegments_ = 0;
//       ComputeManifestCompactionScore();
    } else {
       // delete the new MANIFEST file
       delete newDescriptorFile;
       delete newDescriptorLog;
       env_->DeleteFile(newManifestFileName);
    }
    Log(options_->info_log, 0, "==== Complete Compacting MANIFEST\n");
    return status;
}

Status VersionSet::WriteSnapshot(log::Writer* log) {
  // TODO: Break up into multiple records to reduce memory usage on recovery?

  // Save metadata
  VersionEdit edit;
  edit.SetComparatorName(icmp_.user_comparator()->Name());

  // Save compaction pointers
  for (int level = 0; level < config::kNumLevels; level++) {
    if (!compact_pointer_[level].empty()) {
      InternalKey key;
      key.DecodeFrom(compact_pointer_[level]);
      edit.SetCompactPointer(level, key);
    }
  }
  // Store counters, compact pointers to MANIFEST file
  std::string record;
  edit.EncodeCounters(&record);
  edit.EncodeCompactPointers(&record);
  Status s = log->AddRecord(record);
  // Store new current version file meta data to MANIFEST
  int nRecordsCollected = 0;
  edit.Clear();
  for (int level = 0; s.ok() && level < config::kNumLevels; level++) {
    const std::vector<FileMetaData*>& files = current_->files_[level];
    for (size_t i = 0; s.ok() && i < files.size(); i++) {
        if (nRecordsCollected == VersionSet::MAX_RECORD_TO_ENCODE_AT_ONE_TIME) {
        	record.clear();
            edit.EncodeNewFiles(&record);
            s = log->AddRecord(record);
            edit.Clear();
            nRecordsCollected = 0;
        }
        const FileMetaData* f = files[i];
        edit.AddFile(level, f->number, f->file_size, f->smallest, f->largest);
        nRecordsCollected++;
    }
  }
  if (s.ok() && nRecordsCollected > 0) {
      record.clear();
      edit.EncodeNewFiles(&record);
      s = log->AddRecord(record);
  }
  return s;
}

int VersionSet::NumLevelFiles(int level) const {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return current_->files_[level].size();
}

const char* VersionSet::LevelSummary(LevelSummaryStorage* scratch) const {
  // Update code if kNumLevels changes
  assert(config::kNumLevels == 7);
  snprintf(scratch->buffer, sizeof(scratch->buffer),
           "files[ %d %d %d %d %d %d %d ]",
           int(current_->files_[0].size()),
           int(current_->files_[1].size()),
           int(current_->files_[2].size()),
           int(current_->files_[3].size()),
           int(current_->files_[4].size()),
           int(current_->files_[5].size()),
           int(current_->files_[6].size()));
  return scratch->buffer;
}

uint64_t VersionSet::ApproximateOffsetOf(Version* v, const InternalKey& ikey) {
  uint64_t result = 0;
  const std::vector<FileMetaData*>& files = v->files_[0];
  for (size_t i = 0; i < files.size(); i++) {
    // f->largest accessed only for L0
      if (icmp_.Compare(files[i]->largest, ikey) <= 0) {
        // Entire file is before "ikey", so just add the file size
        result += files[i]->file_size;
      } else if (icmp_.Compare(files[i]->smallest, ikey) > 0) {
        // Entire file is after "ikey", so ignore
      } else {
        // "ikey" falls in the range for this table.  Add the
        // approximate offset of "ikey" within the table.
        Table* tableptr;
        Iterator* iter = table_cache_->NewIterator(
            ReadOptions(), files[i]->number, files[i]->file_size, files[i]->level, &tableptr);
        if (tableptr != NULL) {
          result += tableptr->ApproximateOffsetOf(ikey.Encode());
        }
        delete iter;
      }
  }

  for (int level = 1; level < config::kNumLevels; level++) {
    const std::vector<FileMetaData*>& files = v->files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      if (icmp_.Compare(files[i]->smallest, ikey) > 0) {
        // Entire file iSkipInCurrents after "ikey", so ignore
        // Files other than level 0 are sorted by meta->smallest, so
        // no further files in this level will contain data for
        // "ikey".
        break;
      } else if (i+1 < files.size() && icmp_.Compare(files[i+1]->smallest, ikey) <= 0) {
        // Entire file is before "ikey", so just add the file size
        result += files[i]->file_size;
      } else {
        // "ikey" falls in the range for this table.  Add the
        // approximate offset of "ikey" within the table.
        Table* tableptr;
        Iterator* iter = table_cache_->NewIterator(
            ReadOptions(), files[i]->number, files[i]->file_size, files[i]->level, &tableptr);
  	iter->SeekToLast();
  	InternalKey largest;
  	largest.DecodeFrom(iter->key());
	if (icmp_.Compare(largest, ikey) <= 0){
	  result += files[i]->file_size;
	} else if (tableptr != NULL) {
          result += tableptr->ApproximateOffsetOf(ikey.Encode());
        }
        delete iter;
      }
    }
  }
  return result;
}

void VersionSet::AddLiveFiles(std::unordered_set<uint64_t>* live) {
  for (Version* v = dummy_versions_.next_;
       v != &dummy_versions_;
       v = v->next_) {
    for (int level = 0; level < config::kNumLevels; level++) {
      if (level == 9) {
          continue;
      }
      const std::vector<FileMetaData*>& files = v->files_[level];
      for (size_t i = 0; i < files.size(); i++) {
        live->insert(files[i]->number);
      }
    }
  }
}

int64_t VersionSet::NumLevelBytes(int level) const {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return current_->levelBytes_[level];
}

int64_t VersionSet::MaxNextLevelOverlappingBytes() {
  int64_t result = 0;
  std::vector<FileMetaData*> overlaps;
  for (int level = 1; level < config::kNumLevels - 1; level++) {
    for (size_t i = 0; i < current_->files_[level].size(); i++) {
      const FileMetaData* f = current_->files_[level][i];
      // This function is called only in a test function and not in regular use;
      // So it is ok to access the largest by reading the index here.
      Iterator* iter = table_cache_->NewIterator(
		      ReadOptions(), f->number, f->file_size, f->level);
      iter->SeekToLast();
      InternalKey largest;
      largest.DecodeFrom(iter->key());
      current_->GetOverlappingInputs(level+1, &f->smallest, &largest,
                                     &overlaps);
      delete iter;
      const int64_t sum = TotalFileSize(overlaps);
      if (sum > result) {
        result = sum;
      }
    }
  }
  return result;
}

// Stores the minimal range that covers all entries in inputs in
// *smallest, *largest.
// REQUIRES: inputs is not empty
void VersionSet::GetRange(int level,
			  const std::vector<FileMetaData*>& inputs,
                          InternalKey* smallest,
                          InternalKey* largest) {
  assert(!inputs.empty());
  smallest->Clear();
  largest->Clear();
  if (level == 0) {
    // f->largest accessed only for L0
    for (size_t i = 0; i < inputs.size(); i++) {
      FileMetaData* f = inputs[i];
      if (i == 0) {
        *smallest = f->smallest;
        *largest = f->largest;
      } else {
        if (icmp_.Compare(f->smallest, *smallest) < 0) {
          *smallest = f->smallest;
        }
        if (icmp_.Compare(f->largest, *largest) > 0) {
          *largest = f->largest;
        }
      }
    }
  }
  else {
    // Since inputs is ordered at other levels
    *smallest = inputs[0]->smallest;

    FileMetaData* f = inputs[inputs.size() - 1];
    Iterator* iter = table_cache_->NewIterator(
		      ReadOptions(), f->number, f->file_size, f->level);
    iter->SeekToLast();
    largest->DecodeFrom(iter->key());
    delete iter;
  }
}

// Stores the minimal range that covers all entries in inputs1 and inputs2
// in *smallest, *largest.
// REQUIRES: inputs is not empty
// level is first input's level and second input is level+1
void VersionSet::GetRange2(int level,
			   const std::vector<FileMetaData*>& inputs1,
                           const std::vector<FileMetaData*>& inputs2,
                           InternalKey* smallest,
                           InternalKey* largest) {
// Do not combine the inputs as they are from different levels
  GetRange(level, inputs1, smallest, largest);
  if (inputs2.size() > 0){
    InternalKey smallest2, largest2;
    GetRange(level+1, inputs2, &smallest2, &largest2);
    if (icmp_.Compare(smallest2, *smallest) < 0){
	  *smallest = smallest2;
    }
    if (icmp_.Compare(largest2, *largest) > 0) {
	  *largest = largest2;
    }
  }
}

Iterator* VersionSet::MakeInputIterator(Compaction* c) {
  ReadOptions options;
  options.verify_checksums = options_->paranoid_checks;
  options.fill_cache = false;

  // Level-0 files have to be merged together.  For other levels,
  // we will make a concatenating iterator per level.
  // TODO(opt): use concatenating iterator for level-0 if there is no overlap
  const int space = (c->level() == 0 ? c->inputs_[0].size() + 1 : 2);
  Iterator** list = new Iterator*[space];
  int num = 0;
  for (int which = 0; which < 2; which++) {
    if (!c->inputs_[which].empty()) {
      if (c->level() + which == 0) {
        const std::vector<FileMetaData*>& files = c->inputs_[which];
        for (size_t i = 0; i < files.size(); i++) {
          list[num++] = table_cache_->NewIterator(
              options, files[i]->number, files[i]->file_size, files[i]->level);
        }
      } else {
        // Create concatenating iterator for the files from this level
        list[num++] = NewTwoLevelIterator(
            new Version::LevelFileNumIterator(icmp_, &c->inputs_[which]),
            &GetFileIterator, table_cache_, options);
      }
    }
  }
  assert(num <= space);
  Iterator* result = NewMergingIterator(&icmp_, list, num);
  delete[] list;
  return result;
}
/***
 * The MANIFEST is considerred for compacting at size >= 100 MB.  Above 100 MB, the MANIFEST
 * is considerred for compaction only if its current size is 10 MB bigger than its size after
 * the last MANIFEST compaction.
 */
void VersionSet::ComputeManifestCompactionScore() {
	// Set manifest compaction score to -1 to disable it.
	//current_->manifest_compaction_score_ = -1;

    const float NEW_SEG_THRESHOLD = 1500.0;
    current_->manifest_compaction_score_ = this->nNewManifestSegments_ / NEW_SEG_THRESHOLD;
/*	SAVE the following for periodic manifest compaction
	const size_t START_TO_COMPACT_SIZE = 90*1024*1024; // 90 MB
	const size_t MANIF_SIZE_DIFF = 10*1024*1024; // 10 MB
    std::string manifest_file_name = DescriptorFileName(dbname_, manifest_file_number_);
	struct stat statBuf;
	int status = stat(manifest_file_name.data(), &statBuf);
	if (status == -1) {
		Log(options_->info_log, "Failed to retrieve MANIFEST meta data.\n");
		return;
	}
	if (statBuf.st_size < START_TO_COMPACT_SIZE) {
		current_->last_manifest_size_ = statBuf.st_size;
		return;
	} else if (current_->manifest_compaction_score_ < 0 || current_->last_manifest_size_ == 0) {
		current_->last_manifest_size_ = (size_t)statBuf.st_size;
	}
	float compact_score_by_sst_files = (next_file_number_ - ManifestFileNumber()) / (float)MANIFEST_COMPACT_PERIOD;
	float compact_score_by_manifest_size = statBuf.st_size/(float)(current_->last_manifest_size_ + MANIF_SIZE_DIFF);
	current_->manifest_compaction_score_ = std::max(compact_score_by_sst_files, compact_score_by_manifest_size);
*/
}
Compaction* VersionSet::PickCompaction() {
    if (env_->numberOfGoodSuperblocks() > 1 && descriptor_file_
            && descriptor_file_->getPhysicalSize() / Disk::ZONE_SIZE > Disk::N_CONV_MANIFEST_ZONES / 2) {
        CompactManifest();
        return NULL;
    }

  Compaction* c;
  int level;
  c = NULL;
  // We prefer compactions triggered by too much data in a level over
  // the compactions triggered by seeks.
  const bool size_compaction  = current_->compaction_score_ >= 1 &&
                                current_->compaction_score_ >= current_->manifest_compaction_score_;
  bool seek_compaction = current_->file_to_compact_ != NULL;

  if (env_->numberOfGoodSuperblocks() <= 1) {
      seek_compaction = false;
  }
  if (size_compaction) {
    level = current_->compaction_level_;
    assert(level >= 0);
    assert(level+1 < config::kNumLevels);
    c = new Compaction(level, options_);

    // Pick the first file that comes after compact_pointer_[level]; smallest
    // check is just as good as largest.
    for (size_t i = 0; i < current_->files_[level].size(); i++) {
      FileMetaData* f = current_->files_[level][i];
      if (compact_pointer_[level].empty() ||
          icmp_.Compare(f->smallest.Encode(), compact_pointer_[level]) >= 0) {
        c->inputs_[0].push_back(f);
        break;
      }
    }
    if (c->inputs_[0].empty()) {
      // Wrap-around to the beginning of the key space
      c->inputs_[0].push_back(current_->files_[level][0]);
    }
    prev_compact_pointer_[level] = c->input(0,0)->smallest.Encode().ToString();
  } else if (current_->manifest_compaction_score_ >= 1) {
      CompactManifest();
      return NULL;
  } else if (seek_compaction) {
    level = current_->file_to_compact_level_;
    c = new Compaction(level, options_);
    c->inputs_[0].push_back(current_->file_to_compact_);
  } else {
    return NULL;
  }

  c->input_version_ = current();

  // Files in level 0 may overlap each other, so pick up all overlapping ones
  if (level == 0 && CanLevelExpand(level)) {
    InternalKey smallest, largest;
    GetRange(level, c->inputs_[0], &smallest, &largest);
    // Note that the next call will discard the file we placed in
    // c->inputs_[0] earlier and replace it with an overlapping set
    // which will include the picked file.
    current_->GetOverlappingInputs(0, &smallest, &largest, &c->inputs_[0]);
    assert(!c->inputs_[0].empty());
  }
  SetupOtherInputs(c);
  return c;
}

void VersionSet::SetupOtherInputs(Compaction* c) {
  const int level = c->level();
  InternalKey smallest, largest;
  GetRange(level, c->inputs_[0], &smallest, &largest);
  current_->GetOverlappingInputs(level+1, &smallest, &largest, &c->inputs_[1]);

  // Get entire range covered by compaction
  InternalKey all_start, all_limit;
  GetRange2(level, c->inputs_[0], c->inputs_[1], &all_start, &all_limit);

  // See if we can grow the number of inputs in "level" without
  // changing the number of "level+1" files we pick up.
  if (CanLevelExpand(level)) {
    std::vector<FileMetaData*> expanded0;

    // No level+1 file has been picked for compaction. Attempt a merging compaction.
    // Pick level files up to MaxFileSizeForLevel(c->level() + 1) size without overlapping a level+1 file
    if(c->inputs_[1].empty()) {
      std::vector<FileMetaData*> tmp1;
      if (current_->NumFiles(level + 1) > 0) {
        current_->GetOverlappingInputs(level + 1, &all_start, NULL, &tmp1, 1);
      }
      if (tmp1.size()) {
        InternalKey tmp1_start;
        InternalKey tmp1_end;
        GetRange(level + 1, tmp1, &tmp1_start, &tmp1_end);
        current_->GetOverlappingInputs(level, &all_start, &tmp1_start, &expanded0, MaxFileSizeForLevel(c->level() + 1));
      } else {
        current_->GetOverlappingInputs(level, &all_start, NULL, &expanded0, MaxFileSizeForLevel(c->level() + 1));
      }
    }
    // Attempt to expand considering only overlapping inputs
    else {
      current_->GetOverlappingInputs(level, &all_start, &all_limit, &expanded0);
    }
    const int64_t inputs0_size = TotalFileSize(c->inputs_[0]);
    const int64_t inputs1_size = TotalFileSize(c->inputs_[1]);
    const int64_t expanded0_size = TotalFileSize(expanded0);
    if (expanded0.size() > c->inputs_[0].size() &&
        inputs1_size + expanded0_size < MaxExpandedCompactionByteSizeForLevel(c->level())) {
      InternalKey new_start, new_limit;
      GetRange(level, expanded0, &new_start, &new_limit);
      std::vector<FileMetaData*> expanded1;
      current_->GetOverlappingInputs(level+1, &new_start, &new_limit,
                                     &expanded1);
      if (expanded1.size() == c->inputs_[1].size()) {
        Log(options_->info_log, 5,
            "Expanding@%d %d+%d (%ld+%ld bytes) to %d+%d (%ld+%ld bytes)\n",
            level,
            int(c->inputs_[0].size()),
            int(c->inputs_[1].size()),
            long(inputs0_size), long(inputs1_size),
            int(expanded0.size()),
            int(expanded1.size()),
            long(expanded0_size), long(inputs1_size));
        smallest = new_start;
        largest = new_limit;
        c->inputs_[0] = expanded0;
        c->inputs_[1] = expanded1;
        GetRange2(level, c->inputs_[0], c->inputs_[1], &all_start, &all_limit);
      }
    }
  }

  // Compute the set of grandparent files that overlap this compaction
  // (parent == level+1; grandparent == level+2)
  if (CanLevelExpand(level) && level + 2 < config::kNumLevels) {
    current_->GetOverlappingInputs(level + 2, &all_start, &all_limit,
                                   &c->grandparents_);
  }
  if (level == config::kNumLevels - 2 && c->IsTrivialMove()) {
      Log(options_->info_log, 5, "Handle trivial move from level %d to level %d", level, level + 1);
      InternalKey levelSmallest, levelLargest;
      //current_->GetNewestFile(c->inputs_[1], level + 1);
      FileMetaData* fMetaData = current_->FindFileToCompact(largest, level + 1);
      if (fMetaData) {
          c->inputs_[1].push_back(fMetaData);
      }
      if (c->inputs_[1].size() > 0) {
          this->GetRange(level+1, c->inputs_[1], &levelSmallest, &levelLargest);
          if (icmp_.Compare(levelSmallest, smallest) < 0) {
              smallest = levelSmallest;
          }
          if (icmp_.Compare(levelLargest, largest) > 0) {
              largest = levelLargest;
          }
      }
  }

  if (false) {
    Log(options_->info_log, 5, "Compacting %d '%s' .. '%s'",
        level,
        smallest.DebugString().c_str(),
        largest.DebugString().c_str());
  }

  // Update the place where we will do the next compaction for this level.
  // We update this immediately instead of waiting for the VersionEdit
  // to be applied so that if the compaction fails, we will try a different
  // key range next time.
  compact_pointer_[level] = largest.Encode().ToString();
  c->edit_.SetCompactPointer(level, largest);
  c->SetInput0Largest(largest);
}

void VersionSet::PrintCompactStatus() {
    stringstream ss;
    if (noExpandCompactionLevels_.size() > 0) {
        ss << "No expanding levels: ";
        set<int>::iterator noExpandIt = noExpandCompactionLevels_.begin();
        for (; noExpandIt != this->noExpandCompactionLevels_.end(); ++noExpandIt) {
            ss << *noExpandIt << " ";
        }
        Log(options_->info_log, 5,"%s", ss.str().c_str());
        ss.str("");
    }
    if (excludedCompactionLevels_.size() > 0) {
        ss << "Excluded level failures: ";
        set<int>::iterator it = excludedCompactionLevels_.begin();
        for (; it != excludedCompactionLevels_.end(); ++it) {
            ss << "(" << *it << ", " << spaceNeedsForCompactFailures_[*it] << ") ";
        }
        Log(options_->info_log, 5,"%s", ss.str().c_str());
        ss.str("");
    }
}

Compaction* VersionSet::CompactRange(
    int level,
    const InternalKey* begin,
    const InternalKey* end) {
  std::vector<FileMetaData*> inputs;
  current_->GetOverlappingInputs(level, begin, end, &inputs);
  if (inputs.empty()) {
    return NULL;
  }

  // Avoid compacting too much in one shot in case the range is large.
  // But we cannot do this for level-0 since level-0 files can overlap
  // and we must not pick one file and drop another older file if the
  // two files overlap.
  if (level > 0) {
    const uint64_t limit = MaxFileSizeForLevel(level);
    uint64_t total = 0;
    for (size_t i = 0; i < inputs.size(); i++) {
      uint64_t s = inputs[i]->file_size;
      total += s;
      if (total >= limit) {
        inputs.resize(i + 1);
        break;
      }
    }
  }

  Compaction* c = new Compaction(level, options_);
  c->input_version_ = current();
  c->inputs_[0] = inputs;
  SetupOtherInputs(c);
  return c;
}

void VersionSet::NotifyDefragmentComplete() {
    if (excludedCompactionLevels_.size() == 0) {
        return;
    }
    uint64_t freeSizeForSST = (env_->GetNumberFreeZones() - 1)*Zone::ZONE_SIZE;
    int nArrSize = sizeof(spaceNeedsForCompactFailures_)/sizeof(*spaceNeedsForCompactFailures_);
    for (int i = 0; i < nArrSize; ++i) {
        if (excludedCompactionLevels_.find(i) != excludedCompactionLevels_.end()) {
            if (spaceNeedsForCompactFailures_[i] < freeSizeForSST) {
                RemoveFailedCompactionLevel(i);
            }
        }
    }
}
void VersionSet::SubtractObsoletedValueFiles(set<uint64_t>& obsoleteFiles) {
    MutexLock lock(&mutex_);
    if (deletableValueFiles_.size()) {
        for (set<uint64_t>::iterator it = deletableValueFiles_.begin(); it != deletableValueFiles_.end(); ++it) {
            obsoleteFiles.erase(*it);
        }
    }
    Version* versionPtr = dummy_versions_.next_;

    while (versionPtr != &dummy_versions_) {
        for (set<uint64_t>::iterator it = versionPtr->deletedValueFiles_.begin(); it != versionPtr->deletedValueFiles_.end(); ++it) {
            obsoleteFiles.erase(*it);
        }
        versionPtr = versionPtr->next_;
    }
}
int VersionSet::NumberOfVersions() {
    int nCount = 0;
    Version* versionPtr = &dummy_versions_;
    while (versionPtr->next_ != &dummy_versions_) {
        ++nCount;
        versionPtr = versionPtr->next_;
    }
    return nCount;
}
void VersionSet::UpdateDeletableValueFiles(Version* version) {
    MutexLock lock(&mutex_);
    if (!version || version->deletedValueFiles_.size() == 0) {
        return;
    }
     if (version->prev_ == &dummy_versions_) {
         for (set<uint64_t>::iterator it = version->deletedValueFiles_.begin(); it != version->deletedValueFiles_.end(); ++it) {
             deletableValueFiles_.insert(*it);
         }
     } else {
         // Add deletableValueFile to older version
         version->prev_->AddDeletableValueFiles(version->deletedValueFiles_);
     }
 }
Status VersionSet::DeallocateDeletableValueFiles() {
    MutexLock lock(&mutex_);
    Status status;
    set<uint64_t>::iterator it = deletableValueFiles_.begin();
    while (it != deletableValueFiles_.end() && status.ok()) {
        if (!env_->IsBlockedFile(*it) && !CacheManager::cache(dbname_)->isWritable(*it)) {
            CacheManager::cache(dbname_)->removeReadable(*it);
            status = env_->DeleteFile(*it, kValueFile);
            if (status.ok() || status.IsNotFound()) {
                it = deletableValueFiles_.erase(it);
                status = Status::OK();
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
    return status;
}
void VersionSet::printDeletableValueFiles() {
    MutexLock lock(&mutex_);
    if (deletableValueFiles_.size()) {
        stringstream ss;
        ss << "#versions = " << this->NumberOfVersions();
        ss << ", #deletable value files__ = " << deletableValueFiles_.size() << ": ";
        for (set<uint64_t>::iterator it = deletableValueFiles_.begin(); it != deletableValueFiles_.end(); ++it) {
            ss << *it << " ";
        }
        ss << endl;
        Log(options_->info_log, 0, " %s", ss.str().c_str());
    }
}

Compaction::Compaction(int level, const Options* options)
    : level_(level),
      max_output_file_size_(MaxFileSizeForLevel(level+1)),
      input_version_(NULL),
      grandparent_index_(0),
      seen_key_(false),
      overlapped_bytes_(0),
      options_(options) {
  for (int i = 0; i < config::kNumLevels; i++) {
    level_ptrs_[i] = 0;
  }
}

Compaction::~Compaction() {
  if (input_version_ != NULL) {
    input_version_->Unref();
  }
}

bool Compaction::IsTrivialMove() const {
  // Avoid a move if there is lots of overlapping grandparent data.
  // Otherwise, the move could create a parent file that will require
  // a very expensive merge later on.
  return (num_input_files(0) == 1 &&
          num_input_files(1) == 0 &&
          TotalFileSize(grandparents_) <= kMaxGrandParentOverlapBytes);
}

void Compaction::AddInputDeletions(VersionEdit* edit) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < inputs_[which].size(); i++) {
      edit->DeleteFile(level_ + which, inputs_[which][i]->number);
    }
  }
}

void Compaction::GetLevelLargest() {
  for (int lvl = level_ + 2; lvl < config::kNumLevels; lvl++) {
    InternalKey smallest;
    if (input_version_->files_[lvl].size() > 0){
      input_version_->vset_->GetRange(lvl, input_version_->files_[lvl], &smallest, &level_largest_[lvl]);
    }
  }
}

bool Compaction::IsBaseLevelForKey(const Slice& user_key) {
  // Maybe use binary search to find right entry instead of linear search?
  const Comparator* user_cmp = input_version_->vset_->icmp_.user_comparator();
  for (int lvl = level_ + 2; lvl < config::kNumLevels; lvl++) {
    const std::vector<FileMetaData*>& files = input_version_->files_[lvl];
    for (; files.size() > 0 && level_ptrs_[lvl] < files.size() - 1; ) {
      // Use the next file's smallest instead of the current's largest
      // If false is returned delete marker will move down; if true,
      // will be removed. So sending a cautious false is better.
      FileMetaData* f = files[level_ptrs_[lvl]];
      FileMetaData* f_next = files[level_ptrs_[lvl] + 1];
      if (user_cmp->Compare(user_key, f_next->smallest.user_key()) < 0) {
        // We've advanced far enough
        if (user_cmp->Compare(user_key, f->smallest.user_key()) >= 0) {
          // Key may fall in this file's range, so may not be base level
          LevelLogEvent(options_->info_log, lvl);
          return false;
        }
        break;
      }
      level_ptrs_[lvl]++;
    }
    if (level_ptrs_[lvl] == files.size() - 1){
      FileMetaData* f = files[level_ptrs_[lvl]];
      if (user_cmp->Compare(user_key, level_largest_[lvl].user_key()) <= 0) {
        // We've advanced far enough
        if (user_cmp->Compare(user_key, f->smallest.user_key()) >= 0) {
          // Key may fall in this file's range, so may not be base level
          LevelLogEvent(options_->info_log, lvl);
          return false;
        }
      } else {
         level_ptrs_[lvl]++;
      }
    }
  }
  return true;
}

bool Compaction::ShouldStopBefore(const Slice& internal_key) {
  // Scan to find earliest grandparent file that contains key.
  const InternalKeyComparator* icmp = &input_version_->vset_->icmp_;
  // Use the next file's smallest instead of current's largest.
  // Skipping the last file is fine as the function is only an estimation to stop
  // current compaction output and to start a new one.
  while (grandparent_index_ + 1 < grandparents_.size() &&
      icmp->Compare(internal_key,
                    grandparents_[grandparent_index_ + 1]->smallest.Encode()) > 0) {
    if (seen_key_) {
      overlapped_bytes_ += grandparents_[grandparent_index_]->file_size;
    }
    grandparent_index_++;
  }
  seen_key_ = true;

  if (overlapped_bytes_ > kMaxGrandParentOverlapBytes) {
    // Too much overlap for current output; start new output
    overlapped_bytes_ = 0;
    return true;
  } else {
    return false;
  }
}

void Compaction::ReleaseInputs() {
  if (input_version_ != NULL) {
    input_version_->Unref();
    input_version_ = NULL;
  }
}

}  // namespace leveldb
