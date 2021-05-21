#ifndef KINETIC_MOCK_PRIMARY_STORE_H_
#define KINETIC_MOCK_PRIMARY_STORE_H_

#include "gmock/gmock.h"

#include "primary_store_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockPrimaryStoreIterator : public PrimaryStoreIteratorInterface {
    public:
    MOCK_METHOD0(Init, IteratorStatus());
    MOCK_METHOD0(Next, IteratorStatus());
    MOCK_METHOD0(Last, IteratorStatus());
    MOCK_METHOD0(Prev, IteratorStatus());
    MOCK_METHOD0(Key, std::string());
    MOCK_METHOD1(Version, bool(std::string* version));
    MOCK_METHOD1(Value, StoreOperationStatus(NullableOutgoingValue* value));
    MOCK_METHOD1(Tag, bool(std::string* tag));
    MOCK_METHOD1(Algorithm, bool(int32_t* algorithm));
    MOCK_METHOD1(MScanValue, StoreOperationStatus(NullableOutgoingValue* value));
    MOCK_METHOD0(MScanClose, bool());
};

class MockPrimaryStore : public PrimaryStoreInterface {
    public:
    bool CloseDB() {return true;}
    MOCK_METHOD4(Get, StoreOperationStatus(
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue* value,
        char* buff));
    MOCK_METHOD5(Put, StoreOperationStatus(
        const std::string& key,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token));
    MOCK_METHOD3(Delete, StoreOperationStatus(
        const std::string& key,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token));
    MOCK_METHOD2(Clear, StoreOperationStatus(std::string pin, bool SecureRequested));
    MOCK_METHOD1(Find, PrimaryStoreIteratorInterface*(const std::string& key));
    MOCK_METHOD2(SetRecordStatus, bool(const std::string& key, bool bad));
    MOCK_METHOD0(SetPreUsedBytes, void());
    MOCK_METHOD1(InitUserDataStore, bool(bool create_if_missing));
    MOCK_METHOD0(Close, bool());
    MOCK_METHOD0(IsOpen, bool());
    MOCK_METHOD3(Flush, leveldb::Status(bool toSST, bool clearMems, bool toClose));
    MOCK_METHOD5(Write, bool(BatchSet* batchSet, Command& command_response, const std::tuple<int64_t, int64_t> token,
        int64_t user_id, RequestContext& request_context));
    MOCK_METHOD1(DoesKeyExist, StoreOperationStatus(const string& key));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_PRIMARY_STORE_H_
