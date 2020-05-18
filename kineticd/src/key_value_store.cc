#include "key_value_store.h"

#include <string>
#include <fcntl.h>
#include <stdlib.h>

#include "product_flags.h"
#include "glog/logging.h"
#include "leveldb/mydata.h"
#include "command_line_flags.h"

#include "smrdisk/DriveEnv.h"
#include "smrdisk/ValueFileCache.h"
#include "mem/DynamicMemory.h"

using namespace std; // NOLINT
using com::seagate::kinetic::KeyValueStore;
using com::seagate::kinetic::KeyValueStoreIterator;
using com::seagate::kinetic::IteratorStatus;
using com::seagate::kinetic::StoreOperationStatus;
using com::seagate::kinetic::KeyValueStoreIterator;
using com::seagate::kinetic::cmd::BatchSet;

extern int kTargetFileSize;

// Maximum bytes of overlaps in grandparent (i.e., level+2) before we
// stop building a single file in a level->level+1 compaction.
extern int64_t kMaxGrandParentOverlapBytes;

// Maximum number of bytes in all compacted files.  We avoid expanding
// the lower level file set of a compaction if it would make the
// total compaction cover more than this many bytes.
extern int64_t kExpandedCompactionByteSizeLimit;

const off_t SWAP_FILE_SIZE = (off_t)3*1024*1024*1024;

KeyValueStoreIterator::KeyValueStoreIterator(leveldb::DB& db,
        bool db_corrupt,
        const std::string& key) : db_(db), db_corrupt_(db_corrupt), key_(key), it_(NULL) {
    if (!db_corrupt_) {
        it_ = db_.NewIterator(leveldb::ReadOptions());
    }
}

KeyValueStoreIterator::~KeyValueStoreIterator() {
    delete it_;
}

IteratorStatus KeyValueStoreIterator::Init() {
    if (db_corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }
    it_->Seek(key_);

    return GetIteratorStatus();
}

IteratorStatus KeyValueStoreIterator::Last() {
    if (db_corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    it_->SeekToLast();
    return GetIteratorStatus();
}

IteratorStatus KeyValueStoreIterator::Next() {
    if (db_corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    it_->Next();
    return GetIteratorStatus();
}

IteratorStatus KeyValueStoreIterator::Prev() {
    if (db_corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    it_->Prev();
    return GetIteratorStatus();
}

std::string KeyValueStoreIterator::Key() {
    return it_->key().ToString();
}

const char* KeyValueStoreIterator::Value() {
    return it_->value().data();
}

IteratorStatus KeyValueStoreIterator::GetIteratorStatus() {
    if (!it_->status().ok()) {
        LOG(WARNING) << "LevelDB iterator error: " << it_->status().ToString();//NO_SPELL
        LOG(ERROR) << "IE KV Status";//NO_SPELL
        return IteratorStatus_INTERNAL_ERROR;
    }

    if (it_->Valid()) {
        return IteratorStatus_SUCCESS;
    }

    return IteratorStatus_NOT_FOUND;
}

KeyValueStore::KeyValueStore(const std::string& name, size_t table_cache_size)
    : name_(name),
      table_cache_size_(table_cache_size),
      db_(NULL),
      filter_(NULL),
      db_corrupt_(false),
      block_size(4*1024),
      sst_size(2*1048575) {}

KeyValueStore::KeyValueStore(const std::string& name,
        size_t table_cache_size,
        size_t blockSize,
        size_t sstSize)
    : name_(name),
      table_cache_size_(table_cache_size),
      db_(NULL),
      filter_(NULL),
      db_corrupt_(false),
      block_size(blockSize),
      sst_size(sstSize) {}

bool KeyValueStore::Init(bool create_if_missing) {
    // fix for asolamarr-832
    MutexLock lock(&mu_);
    // fix for asolamarr-832
    leveldb::Options options;
    options.create_if_missing = create_if_missing;
    options.table_cache_size = table_cache_size_;
    options.info_log = &db_logger_;
    options.outstanding_status_sender = &cmd_list_proxy_;
    options.block_size = block_size;
    options.sst_size = sst_size;
    options.write_buffer_size = FLAGS_write_buffer_size;
    options.value_size_threshold = FLAGS_file_store_minimum_size;
    // Enabling a bloom filter allows leveldb to quickly check for
    // the existence of a key
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    filter_ = options.filter_policy;
    kTargetFileSize = sst_size;
    kMaxGrandParentOverlapBytes = 10 * sst_size;
    db_ = NULL;
    leveldb::Status status = leveldb::DB::Open(options, name_, &db_);

    if (status.ok() || status.IsNoSpaceAvailable()) {
        smr::DynamicMemory::getInstance()->subscribe(db_);
        status = leveldb::Status::OK();
    } else {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": status = " << status.ToString() << endl;
        LOG(ERROR) << "Failed to open LevelDB database";//NO_SPELL
        Close();
    }

    db_corrupt_ = !status.ok();
    return status.ok();
}
void KeyValueStore::SetListOwnerReference(
        SendPendingStatusInterface* send_pending_status_sender) {
    cmd_list_proxy_.SetListOwnerReference(send_pending_status_sender);
}

void KeyValueStore::SetLogHandlerInterface(LogHandlerInterface* log_handler) {
    db_logger_.SetLogHandlerInterface(log_handler);
}

KeyValueStore::~KeyValueStore() {
    Close();
}

void KeyValueStore::BGSchedule() {
    MutexLock lock(&mu_);
    if (db_ != NULL) {
       db_->Flush(true);
       db_->BGSchedule();
    }
}

StoreOperationStatus KeyValueStore::Get(const std::string& key, char* value, bool ignore_value,
    bool using_bloom_filter, char* buff) {
    if (db_corrupt_) {
        return StoreOperationStatus_STORE_CORRUPT;
    }
    return TranslateStatus(db_->Get(leveldb::ReadOptions(), key, value, ignore_value,
                                     using_bloom_filter, buff));
}
StoreOperationStatus KeyValueStore::Write(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory) {
    return TranslateStatus(db_->Write(options, updates, memory));
}

StoreOperationStatus KeyValueStore::WriteBat(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory) {
    return TranslateStatus(db_->WriteBat(options, updates, memory));
}

StoreOperationStatus KeyValueStore::Put(
        const std::string& key,
        char* value,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token) {
    if (db_corrupt_) {
        LOG(ERROR) << "Store Corrupted";//NO_SPELL
        return StoreOperationStatus_STORE_CORRUPT;
    }

    leveldb::WriteOptions write_options;
    write_options.sync = guarantee_durable;
    write_options.token = token;

    return TranslateStatus(db_->Put(write_options, key, value));
}

StoreOperationStatus KeyValueStore::Delete(const std::string& key,
                                                  bool guarantee_durable,
                                                  const std::tuple<int64_t, int64_t> token) {
    if (db_corrupt_) {
        LOG(ERROR) << "Store Corrupted";//NO_SPELL
        return StoreOperationStatus_STORE_CORRUPT;
    }

    leveldb::WriteOptions write_options;
    write_options.sync = guarantee_durable;
    write_options.token = token;

    return TranslateStatus(db_->Delete(write_options, key));
}

StoreOperationStatus KeyValueStore::TranslateStatus(const leveldb::Status& status) const {
    if (status.IsNotFound()) {
        return StoreOperationStatus_NOT_FOUND;
    }

    if (status.ok()) {
        return StoreOperationStatus_SUCCESS;
    }

    if (status.IsNoSpaceAvailable()) {
        return StoreOperationStatus_NO_SPACE;
    }

    if (status.IsFrozen()) {
        return StoreOperationStatus_FROZEN;
    }
    if (status.IsSuperblockIO()) {
        return StoreOperationStatus_SUPERBLOCK_IO;
    }

    LOG(WARNING) << "LevelDB error: " << status.ToString();//NO_SPELL

    LOG(ERROR) << "IE KV Status";//NO_SPELL
    return StoreOperationStatus_INTERNAL_ERROR;
}

KeyValueStoreIterator* KeyValueStore::Find(const std::string& key) {
    return new KeyValueStoreIterator(*db_, db_corrupt_, key);
}

StoreOperationStatus KeyValueStore::Clear() {
    Close();

    StoreOperationStatus destroy_status = TranslateStatus(DestroyDB(name_, leveldb::Options()));
    if (destroy_status != StoreOperationStatus_SUCCESS) {
        LOG(ERROR) << "IE KV Status";//NO_SPELL
        return StoreOperationStatus_INTERNAL_ERROR;
    }

    this->Init(true);

    return destroy_status;
}

StoreOperationStatus KeyValueStore::DestroyDataBase() {
    Close();
    smr::DriveEnv::getInstance()->clearDisk();
    return StoreOperationStatus_SUCCESS;
}

void KeyValueStore::Close() {
    if (db_) {
        Flush(true, false);
        smr::CacheManager::clear();
        smr::DynamicMemory::getInstance()->unsubscribe(db_);
        db_->unsubscribe();
        MutexLock lock(&mu_);
        delete db_;
        db_ = NULL;
    }
    if (filter_) {
        delete filter_;
        filter_ = NULL;
    }
}

bool KeyValueStore::GetDBProperty(std::string property, std::string* value) {
    return db_->GetProperty(property, value);
}

void KeyValueStore::FillZoneMap() {
    LOG(ERROR) << "Marking all zones as full";
    db_->FillZoneMap();
}

bool KeyValueStore::TurnOnSwap() {
    return true;
}

bool KeyValueStore::TurnOffSwap() {
    return true;
}

