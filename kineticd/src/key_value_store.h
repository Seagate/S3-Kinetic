#ifndef KINETIC_KEY_VALUE_STORE_H_
#define KINETIC_KEY_VALUE_STORE_H_

#include <string>
#include <iostream>

#include "leveldb/db.h"
#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"
#include "kinetic/common.h"
#include "db_logger.h"
#include "pending_cmd_list_proxy.h"
#include "leveldb/mydata.h"
#include "key_value_store_interface.h"

/*
 * A LevelDB implementation of a key-value store
 */
using com::seagate::kinetic::cmd::BatchSet;

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {
    class BatchSet;
}

class KeyValueStoreIterator : public KeyValueStoreIteratorInterface {
    public:
    KeyValueStoreIterator(leveldb::DB& db, bool db_corrupt, const std::string& key);
    ~KeyValueStoreIterator();
    virtual IteratorStatus Init();
    virtual IteratorStatus Last();
    virtual IteratorStatus Next();
    virtual IteratorStatus Prev();
    virtual std::string Key();
    virtual const char* Value();

    private:
    IteratorStatus GetIteratorStatus();

    leveldb::DB& db_;
    bool db_corrupt_;
    const std::string key_;
    leveldb::Iterator* it_;
};

class KeyValueStore : public KeyValueStoreInterface {
    public:
    KeyValueStore(const std::string& name, size_t table_cache_size);
    KeyValueStore(const std::string& name,
            size_t table_cache_size,
            size_t blockSize,
            size_t sstSize);

    ~KeyValueStore();
    bool Init(bool create_if_missing);
    void SetListOwnerReference(SendPendingStatusInterface* send_pending_status_sender);
    void BGSchedule();
    void SetLogHandlerInterface(LogHandlerInterface* log_handler);
    StoreOperationStatus Get(const std::string& key, char* value, bool ignore_value = false,
                             bool using_bloom_filter = false);
    StoreOperationStatus Put(
            const std::string& key,
            char* value,
            bool guarantee_durable,
            const std::tuple<int64_t, int64_t> token);
    StoreOperationStatus Delete(const std::string& key,
                                bool guarantee_durable,
                                const std::tuple<int64_t, int64_t> token);
    KeyValueStoreIterator* Find(const std::string& key);
    StoreOperationStatus Clear();
    StoreOperationStatus DestroyDataBase();
    void Close();
    bool GetDBProperty(std::string property, std::string* value);
    void SetStoreMountPoint(std::string store_mountpoint) {
        store_mountpoint_ = store_mountpoint;
    }
    bool IsOpen() {
        return (db_ != NULL);
    }
    void CompactLevel(int level) {
    }
    leveldb::Status Flush(bool toSST = false, bool clearMems = false, bool toClose = false) {
        leveldb::Status s;
        MutexLock lock(&mu_);
        if (db_) {
            s = db_->Flush(toSST, clearMems, toClose);
        }
        return s;
    }
    void FillZoneMap();

    StoreOperationStatus Write(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL);

    StoreOperationStatus WriteBat(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL);

    const std::string& GetName() {
        return name_;
    }
    bool HasSpaceForDelCommand() {
        return db_->HasSpaceForDelCommand();
    }

    protected:
    bool TurnOnSwap();
    bool TurnOffSwap();

    private:
    StoreOperationStatus TranslateStatus(const leveldb::Status& status) const;

    const std::string name_;
    const size_t table_cache_size_;
    leveldb::DB* db_;
    const leveldb::FilterPolicy* filter_;
    bool db_corrupt_;
    DbLogger db_logger_;
    size_t block_size;
    size_t sst_size;
    std::string store_mountpoint_;
    PendingCmdListProxy cmd_list_proxy_;
    Mutex mu_;

    //We will need this later when OOM and timeout are sorted out.
    //const leveldb::FilterPolicy* filter_;

    DISALLOW_COPY_AND_ASSIGN(KeyValueStore);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_KEY_VALUE_STORE_H_
