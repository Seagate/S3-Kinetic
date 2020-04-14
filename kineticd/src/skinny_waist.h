#ifndef KINETIC_SKINNY_WAIST_H_
#define KINETIC_SKINNY_WAIST_H_

#include <vector>
#include <string.h>
#include <map>
#include "kinetic/common.h"

#include "authorizer.h"
#include "primary_store_interface.h"
#include "profiler.h"
#include "skinny_waist_interface.h"
#include "request_context.h"
#include "cluster_version_store.h"
#include "user_store_interface.h"
#include "launch_monitor.h"
#include "leveldb/status.h"

using namespace leveldb; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingValueInterface;
using std::string;

class SkinnyWaist : public SkinnyWaistInterface {
    public:
    SkinnyWaist(
        string primary_db_path,
        string store_partition,
        string store_mountpoint,
        string metadata_partition,
        string metadata_mountpoint,
        AuthorizerInterface& authorizer,
        UserStoreInterface& user_store,
        PrimaryStoreInterface& primary_store,
        Profiler& profiler,
        ClusterVersionStoreInterface& cluster_version_store,
        LaunchMonitorInterface& launch_monitor);
    ~SkinnyWaist();

    UserDataStatus InitUserDataStore(bool create_if_missing = false);

    bool CloseDB();
    bool IsDBOpen() {
        return primary_store_.IsOpen();
    }

    StoreOperationStatus Get(
        int64_t user_id,
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        RequestContext& request_context,
        NullableOutgoingValue *value = NULL,
	char* buff = NULL);
    StoreOperationStatus GetVersion(int64_t user_id,
            const std::string& key,
            std::string* version,
            RequestContext& request_context);
    StoreOperationStatus GetNext(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context);
    StoreOperationStatus GetPrevious(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context);
    StoreOperationStatus GetKeyRange(
        int64_t user_id,
        const std::string& start_key,
        bool include_start_key,
        const std::string& end_key,
        bool include_end_key,
        unsigned int max_results,
        bool reverse,
        std::vector<std::string>* results,
        RequestContext& request_context);
    StoreOperationStatus Put(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token);
    StoreOperationStatus Delete(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token);
    StoreOperationStatus InstantSecureErase(std::string pin);
    StoreOperationStatus Security(int64_t user_id,
            const std::list<User> &users,
            RequestContext& request_context);
    bool SetRecordStatus(const std::string& key, bool bad = true);
    const std::string GetKey(const std::string& key, bool next);
    StoreOperationStatus MediaScan(
        int64_t user_id,
        const std::string& start_key,
        std::string* start_key_contain, //current start key
        bool include_start_key,
        const std::string& end_key, //should always be the same end key
        bool include_end_key,
        unsigned int max_results,
        std::vector<std::string>* results,
        RequestContext& request_context,
        ConnectionTimeHandler* timer);
    virtual leveldb::Status Flush(bool toSST = false) {
        return primary_store_.Flush(toSST);
    }
    bool Write(BatchSet* batchSet, Command& command_response, const std::tuple<int64_t, int64_t> token,
        int64_t user_id, RequestContext& request_context) {
        return primary_store_.Write(batchSet, command_response, token, user_id, request_context);
    }

    private:
    /// Functions For MediaScan Integrity Checks
    bool Sha1Integrity(std::string value_str, std::string tag_str);
    bool Sha2Integrity(std::string value_str, std::string tag_str);
    bool Sha3Integrity(std::string value_str, std::string tag_str);
    bool Crc32Integrity(std::string value_str, std::string tag_str);
    bool Crc64Integrity(std::string value_str, std::string tag_str);

    string primary_db_path_;
    string store_partition_;
    string store_mountpoint_;
    string metadata_partition_;
    string metadata_mountpoint_;

    // Threadsafe; AuthorizerInterface implementations must be threadsafe
    AuthorizerInterface& authorizer_;

    // Threadsafe; UserStoreInterface implementations must be threadsafe
    UserStoreInterface& user_store_;

    // Threadsafe; PrimaryStoreInterface implementations must be threadsafe
    PrimaryStoreInterface& primary_store_;

    // Threadsafe
    Profiler& profiler_;

    // Threadsafe
    ClusterVersionStoreInterface& cluster_version_store_;

    LaunchMonitorInterface& launch_monitor_;

    // Counter for put errors
    int put_errors_;

    /////////////////////////////////////////////////////////
    /// algorithm_map_
    /// Contains: Member Function Pointers
    /// Used By: @MediaScan
    /// -------------------------------------------
    /// Algorithm Map of function pointers for Media Scan Integrity Checks
    /// Each Function requires *two* string parameters; string value associated with a key & the tag
    /// Functions return a Boolean. Indicates match/no match on computed vs. supplied tag compare
    ///
    /// Function Pointers maped to *protocol* defined algorithm value
    /// I.E.)
    ///   -Sha1 in proto == 1, therefore,
    ///     map key @ 1 == Sha1Integrity()
    ///   -Sha2 in proto == 2, therefore,
    ///     2 == Sha2Integrity()
    ///   etc.
    /// -------------------------------------------
    std::map<int, bool (SkinnyWaist::*)(std::string, std::string)> algorithm_map_;

    pthread_mutex_t mutex_;

    DISALLOW_COPY_AND_ASSIGN(SkinnyWaist);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SKINNY_WAIST_H_
