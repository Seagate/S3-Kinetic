#ifndef KINETIC_SKINNY_WAIST_INTERFACE_H_
#define KINETIC_SKINNY_WAIST_INTERFACE_H_
#include "connection_time_handler.h"
#include "leveldb/status.h"

enum class UserDataStatus {
    STORE_INACCESSIBLE,
    STORE_CORRUPT,
    LOAD_HALTED,
    MOUNT_FAILED,
    SUCCESSFUL_LOAD
};

using namespace leveldb; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {
    class BatchSet;
}

using std::string;
using com::seagate::kinetic::cmd::BatchSet;
/**
* Implementations must be threadsafe
*/
class KVObject;
class Key;

class SkinnyWaistInterface {
    public:
    virtual ~SkinnyWaistInterface() {}

    virtual StoreOperationStatus Get(
        int64_t user_id,
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        RequestContext& request_context,
        NullableOutgoingValue *value = NULL, char* buff = NULL) = 0;
    virtual StoreOperationStatus GetVersion(
        int64_t user_id,
        const std::string& key,
        std::string* version,
        RequestContext& request_context) = 0;
    virtual StoreOperationStatus GetNext(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context) = 0;
    virtual StoreOperationStatus GetPrevious(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context) = 0;
    virtual StoreOperationStatus GetKeyRange(
        int64_t user_id,
        const std::string& start_key,
        bool include_start_key,
        const std::string& end_key,
        bool include_end_key,
        unsigned int max_results,
        bool reverse,
        std::vector<std::string>* results,
        RequestContext& request_context) = 0;
    virtual StoreOperationStatus Put(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token) = 0;
    virtual StoreOperationStatus Delete(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token) = 0;
    virtual StoreOperationStatus InstantSecureErase(std::string pin) = 0;
    virtual StoreOperationStatus Erase(std::string pin) = 0;
    virtual StoreOperationStatus Security(int64_t user_id,
            const std::list<User> &users,
            RequestContext& request_context) = 0;
    virtual StoreOperationStatus MediaScan(
            int64_t user_id,
            const std::string& start_key,
            std::string* start_key_contain, //current start key
            bool include_start_key,
            const std::string& end_key, //should always be the same end key
            bool include_end_key,
            unsigned int max_results,
            std::vector<std::string>* results,
            RequestContext& request_context,
            ConnectionTimeHandler* timer) = 0;
    virtual bool SetRecordStatus(const std::string& key, bool bad = true) = 0;
    virtual const std::string GetKey(const std::string& key, bool next) = 0;
    virtual UserDataStatus InitUserDataStore(bool create_if_missing = false) = 0;
    virtual bool CloseDB() = 0;
    virtual bool IsDBOpen() = 0;
    virtual bool Write(BatchSet* batchSet, Command& command_response, const std::tuple<int64_t, int64_t> token,
        int64_t user_id, RequestContext& request_context) = 0;
    virtual leveldb::Status Flush(bool toSST = false) = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SKINNY_WAIST_INTERFACE_H_
