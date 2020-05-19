#ifndef KINETIC_KEY_VALUE_STORE_INTERFACE_H_
#define KINETIC_KEY_VALUE_STORE_INTERFACE_H_

#include <string>

#include "gmock/gmock.h"

#include "store_operation_status.h"
#include "leveldb/mydata.h"
#include "leveldb/options.h"
#include "leveldb/write_batch.h"
#include "leveldb/status.h"
#include "log_handler_interface.h"
#include "send_pending_status_interface.h"

namespace smr {
    class KineticMemory;
}
using namespace smr; // NOLINT
using namespace leveldb; // NOLINT
namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {
    class BatchSet;
}
enum IteratorStatus {
    IteratorStatus_SUCCESS,
    IteratorStatus_NOT_FOUND,
    IteratorStatus_INTERNAL_ERROR,
    IteratorStatus_STORE_CORRUPT,
};

class KeyValueStoreIteratorInterface {
    public:
    virtual ~KeyValueStoreIteratorInterface() {}

    virtual IteratorStatus Init() = 0;
    virtual IteratorStatus Last() = 0;
    virtual IteratorStatus Next() = 0;
    virtual IteratorStatus Prev() = 0;
    virtual std::string Key() = 0;
    virtual const char* Value() = 0;
};

class KeyValueStoreInterface {
    public:
    // Always use the Init method for any initialization code that can fail.
    // If you put this code in the constructor then there is no easy way to
    // signal an error condition to the caller.
    virtual ~KeyValueStoreInterface();
    virtual bool Init(bool create_if_missing);
    virtual void SetListOwnerReference(SendPendingStatusInterface* send_pending_status_sender);
    virtual void SetLogHandlerInterface(LogHandlerInterface* log_handler);
    virtual StoreOperationStatus Get(const std::string& key, char* value, bool ignore_value = false,
                                     bool using_bloom_filter = false, char* bvalue = NULL) = 0;
    virtual StoreOperationStatus Put(
            const std::string& key,
            char *value,
            bool guarantee_durable,
            const std::tuple<int64_t, int64_t> token) = 0;
    virtual StoreOperationStatus Delete(const std::string& key,
                                        bool guarantee_durable,
                                        const std::tuple<int64_t, int64_t> token) = 0;
    virtual KeyValueStoreIteratorInterface* Find(const std::string& key) = 0;
    virtual StoreOperationStatus Clear() = 0;
    virtual StoreOperationStatus DestroyDataBase() = 0;
    virtual void Close();
    virtual bool IsOpen() = 0;
    virtual bool GetDBProperty(std::string property, std::string* value);
    virtual void CompactLevel(int level) = 0;
    virtual leveldb::Status Flush(bool toSST = false, bool clearMems = false, bool toClose = false) = 0;
    virtual StoreOperationStatus Write(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL) = 0;
    virtual StoreOperationStatus WriteBat(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory = NULL) = 0;
    virtual void FillZoneMap() = 0;
    virtual const std::string& GetName() = 0;
    virtual void BGSchedule() { }
    virtual bool HasSpaceForDelCommand() { return true; }

    protected:
    virtual bool TurnOnSwap() {
        return true;
    }
    virtual bool TurnOffSwap() {
        return true;
    }
};

class MockKeyValueStore : public KeyValueStoreInterface {
    public:
    MOCK_METHOD1(Init, bool(bool create_if_missing));
    MOCK_METHOD5(Get, StoreOperationStatus(const std::string& key, char *value, bool ignore_value,
        bool using_bloom_filter, char* buff));
    MOCK_METHOD4(Put, StoreOperationStatus(
            const std::string& key,
            char* value,
            bool guarantee_durable,
            const std::tuple<int64_t, int64_t> token));
    MOCK_METHOD3(Delete, StoreOperationStatus(const std::string& key,
                                              bool guarantee_durable,
                                              const std::tuple<int64_t, int64_t> token));
    MOCK_METHOD1(Find, KeyValueStoreIteratorInterface*(const std::string& key));
    MOCK_METHOD0(Clear, StoreOperationStatus());
    MOCK_METHOD0(DestroyDataBase, StoreOperationStatus());
    MOCK_METHOD0(IsOpen, bool());
    MOCK_METHOD1(CompactLevel, void(int level));
    MOCK_METHOD3(Flush, leveldb::Status(bool toSST, bool clearMems, bool toClose));
    MOCK_METHOD3(Write, StoreOperationStatus(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory));
    MOCK_METHOD3(WriteBat, StoreOperationStatus(const WriteOptions& options, WriteBatch* updates, KineticMemory* memory));
    MOCK_METHOD0(FillZoneMap, void());
    MOCK_METHOD0(GetName, const std::string&());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_KEY_VALUE_STORE_INTERFACE_H_
