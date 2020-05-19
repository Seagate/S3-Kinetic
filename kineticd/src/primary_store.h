#ifndef KINETIC_PRIMARY_STORE_H_
#define KINETIC_PRIMARY_STORE_H_

#include <string>
#include <fstream>

#include "leveldb/db.h"
#include "kinetic/common.h"

#include "device_information_interface.h"
#include "file_system_store.h"
#include "key_value_store_interface.h"
#include "primary_store_interface.h"
#include "profiler.h"
#include "internal_value_record.pb.h"
#include "store_operation_status.h"
#include "instant_secure_eraser.h"
#include "cluster_version_store.h"
#include "kernel_mem_mgr.h"
#include "CommandValidator.h"
//#include "smrdisk/Disk.h"

namespace com {
namespace seagate {
namespace kinetic {

class ConnectionHandler;

using std::string;

class PrimaryStoreIterator : public PrimaryStoreIteratorInterface {
    public:
    PrimaryStoreIterator(bool corrupt, FileSystemStoreInterface& file_system_store,
        KeyValueStoreIteratorInterface* iterator, const std::string& key_value_store_name);
    virtual ~PrimaryStoreIterator();

    virtual IteratorStatus Init();
    virtual IteratorStatus Next();
    virtual IteratorStatus Last();
    virtual IteratorStatus Prev();

    virtual std::string Key();
    virtual bool Version(std::string *version);
    virtual StoreOperationStatus Value(NullableOutgoingValue *value);
    virtual StoreOperationStatus MScanValue(NullableOutgoingValue *value);
    virtual bool Tag(std::string *tag);
    virtual bool Algorithm(int32_t *algorithm);

    private:
    bool getInternalValueRecord(proto::InternalValueRecord& record, const char* caller);

    private:
    bool corrupt_;
    FileSystemStoreInterface& file_system_store_;
    KeyValueStoreIteratorInterface* it_;
    const std::string& key_value_store_name_;
};

class PrimaryStore : public PrimaryStoreInterface {
 public:
    PrimaryStore(FileSystemStoreInterface &file_system_store,
        KeyValueStoreInterface& key_value_store,
        ClusterVersionStoreInterface& cluster_version_store,
        DeviceInformationInterface& device_information,
        Profiler& profiler,
        size_t file_system_store_minimum_size,
        InstantSecureEraserInterface& instant_secure_eraser,
        const std::string &preused_file_path);
    bool InitUserDataStore(bool create_if_missing = false);
    bool Close();
    bool IsOpen() {
        return key_value_store_.IsOpen();
    }
    StoreOperationStatus Get(const std::string& key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value, char* buff = NULL);
    StoreOperationStatus Put(
        const std::string& key,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token);
    StoreOperationStatus Delete(const std::string& key,
                                bool guarantee_durable,
                                const std::tuple<int64_t, int64_t> token);
    StoreOperationStatus Clear(std::string pin);
    PrimaryStoreIterator* Find(const std::string& key);
    bool SetRecordStatus(const std::string& key, bool bad = true);
    static const uint64_t kMinFreeSpace; // = smr::Disk::NO_SPACE_THRESHOLD;

    void SetPreUsedBytes();
    virtual leveldb::Status Flush(bool toSST = false, bool clearMems = false, bool toClose = false) {
        return key_value_store_.Flush(toSST, clearMems, toClose);
    }
    bool Write(BatchSet* batchSet, Command& commandResponse, const std::tuple<int64_t, int64_t> token,
        int64_t user_id, RequestContext& request_context);
    KeyValueStoreInterface& GetKeyValueStore() {
        return key_value_store_;
    }
    virtual StoreOperationStatus DoesKeyExist(const string& key) {
        PrimaryStoreValue existing_primary_store_value;
        return Get(key, &existing_primary_store_value, NULL, NULL);
    }
    void SetCommandValidator(CommandValidator* validator) {
        commandValidator_ = validator;
    }
    void SetConnectionHandler(ConnectionHandler* connHandler) {
        connectionHandler_ = connHandler;
    }

 private:
    enum BooleanOrError {
        kTrue,
        kFalse,
        kError
    };
    BooleanOrError FileExists(const std::string &key, std::string* packed_value = NULL);
    bool HasDiskSpace(BatchSet* batchSet, Command& commandResponse);
    // Atrributes
    FileSystemStoreInterface& file_system_store_;
    KeyValueStoreInterface& key_value_store_;
    ClusterVersionStoreInterface& cluster_version_store_;
    DeviceInformationInterface& device_information_;
    Profiler& profiler_;
    const size_t file_system_store_minimum_size_;
    InstantSecureEraserInterface& instant_secure_eraser_;
    const std::string preused_file_path_;
    bool corrupt_;
    CommandValidator* commandValidator_;
    ConnectionHandler* connectionHandler_;

    DISALLOW_COPY_AND_ASSIGN(PrimaryStore);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PRIMARY_STORE_H_
