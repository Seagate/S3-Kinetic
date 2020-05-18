#include "std_map_key_value_store.h"

#include <string>
#include <ostream>
#include "glog/logging.h"

using com::seagate::kinetic::StoreOperationStatus;
using com::seagate::kinetic::StdMapKeyValueStore;
using com::seagate::kinetic::StdMapKeyValueStoreIterator;
using com::seagate::kinetic::IteratorStatus;
using com::seagate::kinetic::KeyValueStoreIteratorInterface;

StdMapKeyValueStoreIterator::StdMapKeyValueStoreIterator(
        std::map<std::string, std::string>& map,
        const std::string& key)
    : map_(map), key_(key) {}

IteratorStatus StdMapKeyValueStoreIterator::Init() {
    it_ = map_.lower_bound(key_);
    return it_ == map_.end() ? IteratorStatus_NOT_FOUND : IteratorStatus_SUCCESS;
}

std::string StdMapKeyValueStoreIterator::Key() {
    return it_->first;
}

const char* StdMapKeyValueStoreIterator::Value() {
    return it_->second.data();
}

IteratorStatus StdMapKeyValueStoreIterator::Last() {
    // If the map is empty then there's no Last to go to
    if (map_.begin() == map_.end()) {
        return IteratorStatus_NOT_FOUND;
    }

    // end() points to a pretend element past the end so back up one from there
    it_ = map_.end();
    it_--;

    return IteratorStatus_SUCCESS;
}

IteratorStatus StdMapKeyValueStoreIterator::Next() {
    ++it_;
    return it_ != map_.end() ? IteratorStatus_SUCCESS : IteratorStatus_NOT_FOUND;
}

IteratorStatus StdMapKeyValueStoreIterator::Prev() {
    if (it_ == map_.begin()) {
        return IteratorStatus_NOT_FOUND;
    }
    --it_;
    return IteratorStatus_SUCCESS;
}


StoreOperationStatus StdMapKeyValueStore::Get(const std::string& key, char *value, bool ignore_value,
    bool using_bloom_filter, char* buff) {
    if (db_.count(key)) {
        LevelDBData* pData = (LevelDBData*) db_[key].data();

        char* buffer = new char[pData->computeSerializedSize()];
        pData->serialize(buffer);
        memcpy(value, (char*)&buffer, sizeof(void*));
        return StoreOperationStatus_SUCCESS;
    }
    return StoreOperationStatus_NOT_FOUND;
}

StoreOperationStatus StdMapKeyValueStore::Put(
        const std::string& key,
        char *value,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token) {
    int size;
    size = sizeof(LevelDBData);
    std::string myvalue(value, size);
    db_[key] = myvalue;

    return StoreOperationStatus_SUCCESS;
}

StoreOperationStatus StdMapKeyValueStore::Delete(const std::string& key,
                                                 bool guarantee_durable,
                                                 const std::tuple<int64_t, int64_t> token) {
    db_.erase(key);
    return StoreOperationStatus_SUCCESS;
}

KeyValueStoreIteratorInterface* StdMapKeyValueStore::Find(const std::string& key) {
    return new StdMapKeyValueStoreIterator(db_, key);
}

StoreOperationStatus StdMapKeyValueStore::Clear() {
    db_.clear();
    return StoreOperationStatus_SUCCESS;
}

StoreOperationStatus StdMapKeyValueStore::DestroyDataBase() {
    db_.clear();
    return StoreOperationStatus_SUCCESS;
}

bool StdMapKeyValueStore::GetDBProperty(std::string property, std::string* value) {
    return true;
}
