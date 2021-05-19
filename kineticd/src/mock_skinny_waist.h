#ifndef KINETIC_MOCK_SKINNY_WAIST_H_
#define KINETIC_MOCK_SKINNY_WAIST_H_

#include "gmock/gmock.h"

#include "skinny_waist_interface.h"
#include "request_context.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockSkinnyWaist : public SkinnyWaistInterface {
    public:
    MockSkinnyWaist() {}
    bool CloseDB() {return true;}
    MOCK_METHOD6(Get, StoreOperationStatus(
        int64_t user_id,
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        RequestContext& request_context,
        NullableOutgoingValue *value,
        char* bvalue));
    MOCK_METHOD4(GetVersion, StoreOperationStatus(
        int64_t user_id,
        const std::string& key,
        std::string* version,
        RequestContext& request_context));
    MOCK_METHOD6(GetNext, StoreOperationStatus(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context));
    MOCK_METHOD6(GetPrevious, StoreOperationStatus(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context));
    MOCK_METHOD9(GetKeyRange, StoreOperationStatus(
        int64_t user_id,
        const std::string& start_key,
        bool include_start_key,
        const std::string& end_key,
        bool include_end_key,
        unsigned int max_results,
        bool reverse,
        std::vector<std::string>* results,
        RequestContext& request_context));
    MOCK_METHOD9(Put, StoreOperationStatus(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token));
    MOCK_METHOD7(Delete, StoreOperationStatus(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token));
    MOCK_METHOD1(InstantSecureErase, StoreOperationStatus(std::string pin));
    MOCK_METHOD1(Erase, StoreOperationStatus(std::string pin));
    MOCK_METHOD3(Security, StoreOperationStatus(int64_t user_id, const std::list<User> &users,
            RequestContext& request_context));
    MOCK_METHOD2(SetRecordStatus, bool(const std::string& key, bool bad));
    MOCK_METHOD2(GetKey, const std::string(const std::string& key, bool next));
    MOCK_METHOD1(InitUserDataStore, UserDataStatus(bool create_if_missing));
    MOCK_METHOD0(IsDBOpen, bool());
    MOCK_METHOD10(MediaScan, StoreOperationStatus(int64_t user_id,
            const std::string& start_key,
            std::string* start_key_contain,
            bool include_start_key,
            const std::string& end_key,
            bool include_end_key,
            unsigned int max_results,
            std::vector<std::string>* results,
            RequestContext& request_context,
            ConnectionTimeHandler* timer));
    MOCK_METHOD5(Write, bool(BatchSet* batchSet, Command& command_response, const std::tuple<int64_t, int64_t> token,
        int64_t user_id, RequestContext& request_context));
    MOCK_METHOD1(Flush, leveldb::Status(bool toSST));

    MOCK_METHOD2(NPut, StoreOperationStatus(KVObject* obj, RequestContext& reqContext));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_SKINNY_WAIST_H_
