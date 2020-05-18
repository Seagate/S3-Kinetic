#ifndef KINETIC_STD_MAP_KEY_VALUE_STORE_H_
#define KINETIC_STD_MAP_KEY_VALUE_STORE_H_

#include <map>
#include <string>

#include "kinetic/common.h"

#include "key_value_store_interface.h"

/*
 * This is a simple key-value store implementation backed by an std::map
 */

namespace com {
namespace seagate {
namespace kinetic {

class StdMapKeyValueStoreIterator : public KeyValueStoreIteratorInterface {
    public:
    StdMapKeyValueStoreIterator(std::map<std::string, std::string>& map, const std::string& key);

    virtual IteratorStatus Init();
    virtual std::string Key();
    virtual const char* Value();
    virtual IteratorStatus Last();
    virtual IteratorStatus Next();
    virtual IteratorStatus Prev();

    private:
    std::map<std::string, std::string>& map_;
    std::map<std::string, std::string>::iterator it_;
    const std::string key_;
};

class StdMapKeyValueStore : public KeyValueStoreInterface {
    public:
    StdMapKeyValueStore() = default;
    StoreOperationStatus Get(const std::string& key, char *value, bool ignore_value = false,
        bool using_bloom_filter = false);

    StoreOperationStatus Put(const std::string& key,
            char *value,
            bool guarantee_durable,
            const std::tuple<int64_t, int64_t> token);
    StoreOperationStatus Delete(const std::string& key,
                                bool guarantee_durable,
                                const std::tuple<int64_t, int64_t> token);
    KeyValueStoreIteratorInterface* Find(const std::string& key);
    StoreOperationStatus Clear();
    StoreOperationStatus DestroyDataBase();
    bool GetDBProperty(std::string property, std::string* value);
    bool IsOpen() { return false; }
    void CompactLevel(int level) {}
    leveldb::Status Flush(bool toSST = false, bool clearMems = false, bool toClose = false) { return leveldb::Status::OK(); }
    StoreOperationStatus Write(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL) {
        return StoreOperationStatus_SUCCESS;
    }
    StoreOperationStatus WriteBat(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL) {
        return StoreOperationStatus_SUCCESS;
    }
    void FillZoneMap() {}
    const std::string& GetName() { return name_; }

    private:
    std::map<std::string, std::string> db_;
    const std::string name_;
    DISALLOW_COPY_AND_ASSIGN(StdMapKeyValueStore);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_STD_MAP_KEY_VALUE_STORE_H_
