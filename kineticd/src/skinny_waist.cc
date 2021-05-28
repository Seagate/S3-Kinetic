#include "skinny_waist.h"
#include "pthreads_mutex_guard.h"
#include "mount_manager.h"
#include "glog/logging.h"
#include "openssl/sha.h"
#include "launch_monitor.h"
#include "product_flags.h"
#include "popen_wrapper.h"

#include <sys/syscall.h>

#include <sys/syscall.h>
using com::seagate::kinetic::SkinnyWaist;
using com::seagate::kinetic::StoreOperationStatus;

SkinnyWaist::SkinnyWaist(
        string primary_db_path,
        string store_partition,
        string store_mountpoint,
        string metadata_partition,
        string metadata_mountpoint,
        AuthorizerInterface& authorizer,
        UserStoreInterface& user_store,
        PrimaryStoreInterface& primary_store,
        Profiler &profiler,
        ClusterVersionStoreInterface& cluster_version_store,
        LaunchMonitorInterface& launch_monitor)
    :   primary_db_path_(primary_db_path),
        store_partition_(store_partition),
        store_mountpoint_(store_mountpoint),
        metadata_partition_(metadata_partition),
        metadata_mountpoint_(metadata_mountpoint),
        authorizer_(authorizer),
        user_store_(user_store),
        primary_store_(primary_store),
        profiler_(profiler),
        cluster_version_store_(cluster_version_store),
        launch_monitor_(launch_monitor) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&mutex_, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    put_errors_ = 0;
    /// Add Function Pointers to Map for MediaScan
    algorithm_map_[1] = &SkinnyWaist::Sha1Integrity;
    algorithm_map_[2] = &SkinnyWaist::Sha2Integrity;
    algorithm_map_[3] = &SkinnyWaist::Sha3Integrity;
    algorithm_map_[4] = &SkinnyWaist::Crc32Integrity;
    algorithm_map_[5] = &SkinnyWaist::Crc64Integrity;
}

SkinnyWaist::~SkinnyWaist() {
    CHECK(!pthread_mutex_destroy(&mutex_));
}
bool SkinnyWaist::CloseDB() {
    VLOG(1) << "Close DB";
    primary_store_.Close();
    return true;
}
UserDataStatus SkinnyWaist::InitUserDataStore(bool create_if_missing) {
    if (!launch_monitor_.OperationAllowed(LaunchStep::MOUNT_CHECK)) {
        launch_monitor_.WriteState(LaunchStep::MOUNT_CHECK, false);
        LOG(ERROR) << "Launch monitor halted mount check";
        return UserDataStatus::LOAD_HALTED;
    }

    MountManager mount_manager = MountManager();

    VLOG(1) << "Opening LevelDB DB for SMR";//NO_SPELL
    if (!launch_monitor_.OperationAllowed(LaunchStep::INIT_USERS)) {
        launch_monitor_.WriteState(LaunchStep::INIT_USERS, false);
        LOG(ERROR) << "Launch monitor halted user load";
        return UserDataStatus::LOAD_HALTED;
    }

    if (!cluster_version_store_.Init()) {
        LOG(ERROR) << "Failed to initialize the cluster version store";
        return UserDataStatus::STORE_INACCESSIBLE;
    }

    if (!mount_manager.IsMounted(metadata_partition_, metadata_mountpoint_)) {
        VLOG(1) << "Metadata partition is not yet mounted... Mounting Now";//NO_SPELL
        if (!mount_manager.MountExt4(metadata_partition_, metadata_mountpoint_)) {
            LOG(ERROR) << "Unable to mount metadata partition... Attempting to rebuild filesystem";//NO_SPELL//NOLINT

            // TODO(anyone): remove rebuilding the filesystem as part of ASOLAMARR 352
            int mkfs_result;
            std::string mkfs_log;
            RawStringProcessor mkfs_processor(&mkfs_log, &mkfs_result);
            static const int kJournalSizeMB = 32;
            std::stringstream command;
            command << "mkfs.ext4 -J size=" << kJournalSizeMB
                    << " " << metadata_partition_ << " 2>&1";
            std::string system_command = command.str();
            if (!execute_command(system_command, mkfs_processor)) {
                LOG(ERROR) << "Unable to create ext4 fs on " << metadata_partition_;//NO_SPELL
                LOG(ERROR) << mkfs_result;
                return UserDataStatus::STORE_INACCESSIBLE;
            }

            if (!mount_manager.MountExt4(metadata_partition_, metadata_mountpoint_)) {
                LOG(ERROR) << "Unable to mount metadata partition with rebuilt filesystem";//NO_SPELL//NOLINT
                return UserDataStatus::STORE_INACCESSIBLE;
            }
        }
    }

    if (!user_store_.Init()) {
        LOG(ERROR) << "User Store failed Init";//NO_SPELL
        return UserDataStatus::STORE_INACCESSIBLE; // Set corrupt if we can't load ACLs
    }

    if (!launch_monitor_.OperationAllowed(LaunchStep::INIT_USER_DATA_STORE)) {
        launch_monitor_.WriteState(LaunchStep::INIT_USER_DATA_STORE, false);
         LOG(ERROR) << "Launch monitor halted user data store";
         return UserDataStatus::LOAD_HALTED;
    }

    if (!primary_store_.InitUserDataStore()) {
        LOG(ERROR) << "Primary Store failed to Init User Data Store";//NO_SPELL
        return UserDataStatus::STORE_CORRUPT;
    }

    launch_monitor_.SuccessfulLoad();
    return UserDataStatus::SUCCESSFUL_LOAD;
}

StoreOperationStatus SkinnyWaist::Get(
        int64_t user_id,
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        RequestContext& request_context,
        NullableOutgoingValue *value, char* buff) {
        PthreadsMutexGuard guard(&mutex_);
    if (!authorizer_.AuthorizeKey(user_id, Domain::kRead, key, request_context)) {
        return StoreOperationStatus_AUTHORIZATION_FAILURE;
    }
    if (key == "") {
        return StoreOperationStatus_INVALID_REQUEST;
    }
    StoreOperationStatus status = primary_store_.Get(key, primary_store_value, value, buff);

    switch (status) {
        case StoreOperationStatus_SUCCESS:
        case StoreOperationStatus_NOT_FOUND:
        case StoreOperationStatus_STORE_CORRUPT:
        case StoreOperationStatus_DATA_CORRUPT:
            break;
        default:
            status = StoreOperationStatus_INTERNAL_ERROR;
    }
    return status;
}

StoreOperationStatus SkinnyWaist::GetVersion(
        int64_t user_id,
        const std::string& key,
        std::string* version,
        RequestContext& request_context) {
        PthreadsMutexGuard guard(&mutex_);

    if (!authorizer_.AuthorizeKey(user_id, Domain::kRead, key, request_context)) {
        return StoreOperationStatus_AUTHORIZATION_FAILURE;
    }
    if (key == "") {
        return StoreOperationStatus_INVALID_REQUEST;
    }
    PrimaryStoreValue primary_store_value;
    StoreOperationStatus ret = primary_store_.Get(key, &primary_store_value, NULL);
    if (ret == StoreOperationStatus_SUCCESS) {
        *version = primary_store_value.version;
    }

    switch (ret) {
        case StoreOperationStatus_SUCCESS:
            return ret;
        case StoreOperationStatus_NOT_FOUND:
            return ret;
        case StoreOperationStatus_STORE_CORRUPT:
            return ret;
        default:
            LOG(ERROR) << "IE store status";
            return StoreOperationStatus_INTERNAL_ERROR;
    }
}

StoreOperationStatus SkinnyWaist::GetNext(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context) {
    CHECK(actual_key);
    CHECK(primary_store_value);

    PthreadsMutexGuard guard(&mutex_);

    PrimaryStoreIteratorInterface* it = primary_store_.Find(key);

    switch (it->Init()) {
        case IteratorStatus_NOT_FOUND:
            delete it;
            return StoreOperationStatus_NOT_FOUND;
        case IteratorStatus_INTERNAL_ERROR:
            delete it;
            LOG(ERROR) << "IE iterator";
            return StoreOperationStatus_INTERNAL_ERROR;
        case IteratorStatus_STORE_CORRUPT:
            delete it;
            return StoreOperationStatus_STORE_CORRUPT;
        case IteratorStatus_SUCCESS: {
            break;
        }
    }

    if (it->Key() >= key || key == "") {
        IteratorStatus status = IteratorStatus_SUCCESS;
        if (it->Key() == key && key != "") {
            status = it->Next();
        }
        while (status == IteratorStatus_SUCCESS) {
            if (authorizer_.AuthorizeKey(user_id, Domain::kRead, it->Key(), request_context)) {
                break;
            }
            status = it->Next();
        }
        switch (status) {
            case IteratorStatus_NOT_FOUND:
                delete it;
                return StoreOperationStatus_NOT_FOUND;
            case IteratorStatus_INTERNAL_ERROR:
                delete it;
                LOG(ERROR) << "IE iterator";
                return StoreOperationStatus_INTERNAL_ERROR;
            case IteratorStatus_STORE_CORRUPT:
                delete it;
                return StoreOperationStatus_STORE_CORRUPT;
            case IteratorStatus_SUCCESS: {
                break;
            }
        }
    } else {
        delete it;
        return StoreOperationStatus_NOT_FOUND;
    }

    *actual_key = it->Key();
    if (!it->Version(&primary_store_value->version) ||
            !it->Tag(&primary_store_value->tag) ||
            !it->Algorithm(&primary_store_value->algorithm)) {
        delete it;
        LOG(ERROR) << "IE iterator";
        return StoreOperationStatus_INTERNAL_ERROR;
    }
    StoreOperationStatus status = it->Value(value);

    delete it;
    return status;
}

StoreOperationStatus SkinnyWaist::GetPrevious(
        int64_t user_id,
        const std::string& key,
        std::string* actual_key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value,
        RequestContext& request_context) {
    CHECK(actual_key);
    CHECK(primary_store_value);

    PthreadsMutexGuard guard(&mutex_);
    PrimaryStoreIteratorInterface* it = primary_store_.Find(key);

    IteratorStatus status;
    if (key == "") {
        status = it->Last();
    } else {
        status = it->Init();
    }

    if (status == IteratorStatus_NOT_FOUND) {
        status = it->Last();
    }

    if (status == IteratorStatus_SUCCESS && key != "" && it->Key() > key) {
        status = it->Prev();
    }

    if (status == IteratorStatus_SUCCESS && (it->Key() <= key || key == "")) {
        if (it->Key() == key && key != "") {
            status = it->Prev();
        }
        while (status == IteratorStatus_SUCCESS) {
            if (authorizer_.AuthorizeKey(user_id, Domain::kRead, it->Key(), request_context)) {
                break;
            }
            status = it->Prev();
        }
    }

    if (status == IteratorStatus_SUCCESS) {
        *actual_key = it->Key();
        if (!it->Version(&primary_store_value->version) ||
            !it->Tag(&primary_store_value->tag) ||
            !it->Algorithm(&primary_store_value->algorithm)) {
            status = IteratorStatus_STORE_CORRUPT;
        }
    }

    StoreOperationStatus storeStatus = StoreOperationStatus_SUCCESS;
    switch (status) {
        case IteratorStatus_NOT_FOUND:
            storeStatus = StoreOperationStatus_NOT_FOUND;
            break;
        case IteratorStatus_INTERNAL_ERROR:
            LOG(ERROR) << "IE iterator";
            storeStatus = StoreOperationStatus_INTERNAL_ERROR;
            break;
        case IteratorStatus_STORE_CORRUPT:
            storeStatus = StoreOperationStatus_STORE_CORRUPT;
            break;
        case IteratorStatus_SUCCESS: {
            storeStatus = it->Value(value);
            break;
        }
    }

    delete it;
    return storeStatus;
}

StoreOperationStatus SkinnyWaist::GetKeyRange(
        int64_t user_id,
        const std::string& start_key,
        bool include_start_key,
        const std::string& end_key,
        bool include_end_key,
        unsigned int max_results,
        bool reverse,
        std::vector<std::string>* results,
        RequestContext& request_context) {
    CHECK_GT(max_results, 0U);
    CHECK_NOTNULL(results);

    PthreadsMutexGuard guard(&mutex_);
    PrimaryStoreIteratorInterface* it;
    IteratorStatus it_status;

    std::string search_end_key = end_key;
    if (reverse && end_key != "") {
        it = primary_store_.Find(end_key);
    } else if (end_key == "") {
        it = primary_store_.Find(start_key);
        it_status = it->Last();
        if (it_status == IteratorStatus_SUCCESS) {
            search_end_key = it->Key();
        }
        if (reverse) {
            delete it;
            it = primary_store_.Find(search_end_key);
        }
    } else {
        it = primary_store_.Find(start_key);
    }

    it_status = it->Init();

    if (reverse && it_status == IteratorStatus_NOT_FOUND) {
        it_status = it->Last();
    }

    if (reverse && it_status == IteratorStatus_SUCCESS && it->Key() > search_end_key) {
        it_status = it->Prev();
    }

    switch (it_status) {
        case IteratorStatus_NOT_FOUND:
            delete it;
            return StoreOperationStatus_SUCCESS;
        case IteratorStatus_INTERNAL_ERROR:
            delete it;
            LOG(ERROR) << "IE iterator";
            return StoreOperationStatus_INTERNAL_ERROR;
        case IteratorStatus_STORE_CORRUPT:
            delete it;
            return StoreOperationStatus_STORE_CORRUPT;
        case IteratorStatus_SUCCESS: {}
            // Fall through
    }

    results->reserve(max_results);
    if (reverse) {
        if (it->Key() == search_end_key && !include_end_key) {
            it_status = it->Prev();
        }
    } else {
        if (it->Key() == start_key && !include_start_key) {
            it_status = it->Next();
        }
    }

    if (reverse) {
        while (it_status == IteratorStatus_SUCCESS &&
                results->size() < (unsigned int)max_results &&
                (it->Key() > start_key || (include_start_key && it->Key() == start_key))) {
            ///add key to result list, increment
            results->push_back(it->Key());
            it_status = it->Prev();
        }//end while reverse
    } else {
        while (it_status == IteratorStatus_SUCCESS &&
                results->size() < (unsigned int)max_results &&
                (it->Key() < search_end_key ||
                (include_end_key && it->Key() == search_end_key))) {
            results->push_back(it->Key());
            it_status = it->Next();
        }
    }

    delete it;
    if (it_status == IteratorStatus_INTERNAL_ERROR) {
        LOG(ERROR) << "IE iterator";
        return StoreOperationStatus_INTERNAL_ERROR;
    } else {
        return StoreOperationStatus_SUCCESS;
    }
}

StoreOperationStatus SkinnyWaist::Put(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token) {
    PthreadsMutexGuard guard(&mutex_);

#ifdef KDEBUG
    DLOG(INFO) << "Checking PUT authorization";
#endif
    if (!authorizer_.AuthorizeKey(user_id, Domain::kWrite, key, request_context)) {
#ifdef KDEBUG
        DLOG(INFO) << "User failed PUT authorization check";
#endif
        return StoreOperationStatus_AUTHORIZATION_FAILURE;
    }
    if (key == "") {
        return StoreOperationStatus_INVALID_REQUEST;
    }
    // Clients who don't care about version checking can gain
    // performance by setting a flag. If they set that flag
    // we skip all version checking and thus save some leveldb calls
    if (!ignore_current_version) {
        VLOG(5) << "PUT checking existing value";
        PrimaryStoreValue existing_primary_store_value;
        StoreOperationStatus get_existing_result =
            primary_store_.Get(key, &existing_primary_store_value, NULL);

        if (get_existing_result == StoreOperationStatus_STORE_CORRUPT) {
            return StoreOperationStatus_STORE_CORRUPT;
        }

        if (get_existing_result != StoreOperationStatus_SUCCESS &&
            get_existing_result != StoreOperationStatus_NOT_FOUND) {
            return StoreOperationStatus_INTERNAL_ERROR;
        }
        bool key_exists = get_existing_result == StoreOperationStatus_SUCCESS;

        if ((key_exists && existing_primary_store_value.version != current_version) ||
            (!key_exists && current_version.length())) {
            return StoreOperationStatus_VERSION_MISMATCH;
        }
    }

    switch (primary_store_.Put(key, primary_store_value, value, guarantee_durable, token)) {
        case StoreOperationStatus_SUCCESS:
            VLOG(5) << "PUT succeeded";
            put_errors_ = 0;
            return StoreOperationStatus_SUCCESS;
        case StoreOperationStatus_NO_SPACE:
            VLOG(5) << "PUT failed because drive does not have enough remaining space";
            return StoreOperationStatus_NO_SPACE;
        case StoreOperationStatus_MEDIA_FAULT:
            return StoreOperationStatus_MEDIA_FAULT;
        case StoreOperationStatus_FROZEN:
            return StoreOperationStatus_FROZEN;
        case StoreOperationStatus_STORE_CORRUPT:
            VLOG(5) << "PUT failed because db corrupt";
            return StoreOperationStatus_STORE_CORRUPT;
        case StoreOperationStatus_SUPERBLOCK_IO:
            VLOG(5) << "Superblock is not writable";
            return StoreOperationStatus_SUPERBLOCK_IO;
        default:
            VLOG(5) << "PUT resulted in internal error";

            MountManager mount_manager = MountManager();
            put_errors_++;
            if (mount_manager.CheckFileSystemReadonly(put_errors_,
                store_mountpoint_, store_partition_)) {
                LOG(ERROR) << "File system is read only need to power cycle drive";
                #if !defined(PRODUCT_LAMARRKV)
                // Restart system by tripping the porz only if it is not LAMARRKV
                #ifdef VECTOR_PORZ
                syscall(SYS_pull_PORZ);
                #endif
                #endif
            }
            LOG(ERROR) << "IE store status";
            return StoreOperationStatus_INTERNAL_ERROR;
    }
}

StoreOperationStatus SkinnyWaist::Delete(
        int64_t user_id,
        const std::string& key,
        const std::string& current_version,
        bool ignore_current_version,
        bool guarantee_durable,
        RequestContext& request_context,
        const std::tuple<int64_t, int64_t> token) {
    PthreadsMutexGuard guard(&mutex_);

    if (!authorizer_.AuthorizeKey(user_id, Domain::kDelete, key, request_context)) {
        return StoreOperationStatus_AUTHORIZATION_FAILURE;
    }
    if (key == "") {
        return StoreOperationStatus_INVALID_REQUEST;
    }

    if (!ignore_current_version) {
        PrimaryStoreValue existing_primary_store_value;
        StoreOperationStatus get_existing_result =
            primary_store_.Get(key, &existing_primary_store_value, NULL);

        if (get_existing_result == StoreOperationStatus_STORE_CORRUPT) {
            return StoreOperationStatus_STORE_CORRUPT;
        }

        if (get_existing_result != StoreOperationStatus_SUCCESS &&
                get_existing_result != StoreOperationStatus_NOT_FOUND) {
            return StoreOperationStatus_INTERNAL_ERROR;
        }
        bool key_exists = get_existing_result == StoreOperationStatus_SUCCESS;
        if (!key_exists) {
            return StoreOperationStatus_NOT_FOUND;
        }

        if (existing_primary_store_value.version != current_version) {
            return StoreOperationStatus_VERSION_MISMATCH;
        }
    }

    switch (primary_store_.Delete(key, guarantee_durable, token)) {
        case StoreOperationStatus_SUCCESS:
            return StoreOperationStatus_SUCCESS;
        case StoreOperationStatus_NO_SPACE:
            LOG(ERROR) << "Failed to persist key/value in dbase. No space available.";//NO_SPELL
            return StoreOperationStatus_NO_SPACE;
        case StoreOperationStatus_FROZEN:
            LOG(ERROR) << "Failed to persist key/value in dbase. No space available.";//NO_SPELL
            return StoreOperationStatus_FROZEN;
        case StoreOperationStatus_SUPERBLOCK_IO:
            LOG(ERROR) << "Superblock IO: Superblock is not writable.";//NO_SPELL
            return StoreOperationStatus_SUPERBLOCK_IO;
        default:
            LOG(ERROR) << "IE store status";
            return StoreOperationStatus_INTERNAL_ERROR;
    }
}

StoreOperationStatus SkinnyWaist::InstantSecureErase(std::string pin) {
    PthreadsMutexGuard guard(&mutex_);
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << endl;

    // No authorization check needed, check performed in SetupHandler

    StoreOperationStatus status = primary_store_.Clear(pin, true);

    if (status == StoreOperationStatus_SUCCESS) {
        launch_monitor_.SuccessfulLoad();

        if (!user_store_.Clear()) {
            LOG(ERROR) << "IE store status";
            return StoreOperationStatus_INTERNAL_ERROR;
        }

        if (!user_store_.CreateDemoUser()) {
            LOG(ERROR) << "IE Could not create demo user";
            return StoreOperationStatus_INTERNAL_ERROR;
        }
    } else if (status == StoreOperationStatus_ISE_FAILED_VAILD_DB) {
        launch_monitor_.SuccessfulLoad();
    }
    return status;
}

StoreOperationStatus SkinnyWaist::Erase(std::string pin) {
    PthreadsMutexGuard guard(&mutex_);
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << endl;

    // No authorization check needed, check performed in SetupHandler

    StoreOperationStatus status = primary_store_.Clear(pin, false);

    if (status == StoreOperationStatus_SUCCESS) {
        launch_monitor_.SuccessfulLoad();

        if (!user_store_.Clear()) {
            LOG(ERROR) << "IE store status";
            return StoreOperationStatus_INTERNAL_ERROR;
        }

        if (!user_store_.CreateDemoUser()) {
            LOG(ERROR) << "IE Could not create demo user";
            return StoreOperationStatus_INTERNAL_ERROR;
        }
    } else if (status == StoreOperationStatus_ISE_FAILED_VAILD_DB) {
        launch_monitor_.SuccessfulLoad();
    }
    return status;
}

StoreOperationStatus SkinnyWaist::Security(int64_t user_id,
        const std::list<User> &users,
        RequestContext& request_context) {
    PthreadsMutexGuard guard(&mutex_);
    StoreOperationStatus status = StoreOperationStatus_SUCCESS;
    UserStoreInterface::Status userStoreStatus = user_store_.Put(users);

    switch (userStoreStatus) {
        case UserStoreInterface::Status::SUCCESS:
             break;
        case UserStoreInterface::Status::EXCEED_LIMIT:
             status = StoreOperationStatus_EXCEED_LIMIT;
             break;
        case UserStoreInterface::Status::DUPLICATE_ID:
             status = StoreOperationStatus_DUPLICATE_ID;
             break;
        case UserStoreInterface::Status::FAIL_TO_STORE:
        default:
             status = StoreOperationStatus_INTERNAL_ERROR;
             break;
    }
    return status;
}

bool SkinnyWaist::SetRecordStatus(const std::string& key, bool bad) {
    LOG(INFO) << "Updating internal value record";
    return primary_store_.SetRecordStatus(key);
}

/////////////////////////////////////////////////////////
/// MediaScan
/// Caller Class: @message_processor.cc
/// Callee Classes: @primary_store, @outgoing_value, @file_system_store.cc
/// -------------------------------------------
/// For each key in range (startkey -> endkey):
/// locate, evaluate and return damaged keys
/// -------------------------------------------
/// @param[in] user_id
/// @param[in] start_key - current start key
/// @param[in] start_key_contain - mutable start key for saving state, store last key visited
/// @param[in] include_start_key - bool indicating whether start key it to be included in scan
/// @param[in] end_key - original end key
/// @param[in] include_end_key - bool indicating whether end key it to be included in scan
/// @param[in] max_results - int indicating max amount of keys to be returned
/// @param[in] results - vector passed from @message_processor, store returned keys
/// @param[in] request_context
/// @param[in] timer - ConnectionTimeHandler from connection struct, manages interrupts && timeouts
StoreOperationStatus SkinnyWaist::MediaScan(
    int64_t user_id,
    const std::string& start_key,
    std::string* start_key_contain,
    bool include_start_key,
    const std::string& end_key,
    bool include_end_key,
    unsigned int max_results,
    std::vector<std::string>* results,
    RequestContext& request_context,
    ConnectionTimeHandler* timer) {

    CHECK_GT(max_results, 0U);
    CHECK_NOTNULL(results);

    ///Define Necessary Variables
    /// @it - primary store iterator
    /// @value_container - used to hold 'value 'associated with current iter key
    ///     ----(from either file_store or primary_store_value)
    ///
    /// @primary_store_value - holds 'Tag','Algorithm' & sometimes 'value' from current iter key
    /// @prev_key_temp -  temporary variable used to save the most recent key on each iteration
    /// @SHA1_VAL - placeholder for readability of proto defined SHA1 Algorithm value
    ///     ----(avoids the "magic number syndrom")
    PrimaryStoreIteratorInterface* it;
    IteratorStatus it_status;
    std::string search_end_key = end_key;
    NullableOutgoingValue value_container;
    PrimaryStoreValue primary_store_value;
    std::string prev_key_temp;
    bool tag_cmp_result = true;

    ///Find First Key
    /// If a First Key is not found, return empty list to user
    /// First Key does not necessarily mean the provided start key.
    /// If start key !exist the next lexicographical key within
    /// Range of current op is suitable.
    it = primary_store_.Find(start_key);
    if (end_key == "") {
        it_status = it->Last();
        if (it_status == IteratorStatus_SUCCESS) {
            search_end_key = it->Key();
        }
    }

    it_status = it->Init();

    switch (it_status) {
        case IteratorStatus_NOT_FOUND:
            delete it;
            LOG(ERROR) << "IE iterator, start key not found";
            return StoreOperationStatus_SUCCESS;
        case IteratorStatus_INTERNAL_ERROR:
            delete it;
            LOG(ERROR) << "IE iterator";
            return StoreOperationStatus_INTERNAL_ERROR;
        case IteratorStatus_STORE_CORRUPT:
            delete it;
            return StoreOperationStatus_STORE_CORRUPT;
        case IteratorStatus_SUCCESS: {}
            /// Fall through
    }

    ///reserve max space in memory for vector
    results->reserve(max_results);

    ///If request did not indicate inclusive startkey
    ///set the key to the next in line
    if (it->Key() == start_key && !include_start_key) {
        it_status = it->Next();
    }
    /// START Loop until Iterator Error or End Key Is Reached
    /// Break out and Return in Event of Timeout or TimeQuanta
    while (it_status == IteratorStatus_SUCCESS &&
            results->size() < (unsigned int)max_results &&
            (it->Key() < search_end_key || (include_end_key && it->Key() == search_end_key))) {
        ///#1 Get Algo and Tag
        if (!it->Tag(&primary_store_value.tag) ||
            !it->Algorithm(&primary_store_value.algorithm)) {
            delete it;
            LOG(ERROR) << "IE iterator";
            return StoreOperationStatus_INTERNAL_ERROR;
        }
        ///#2 Get the value associated with the key
        StoreOperationStatus valueStatus = it->MScanValue(&value_container);
        ///#3 Add unreadable key values to damaged collecion
        if (valueStatus != StoreOperationStatus_SUCCESS) {
            results->push_back(it->Key());
        } else {
            ///#4 If value found, get value's string representation
            std::string valueString;
            int error;
            value_container.ToString(&valueString, &error);
            ///#5 Find function associated w/ primary_store_value's defined algorithm
            /// If function found, execute & observe tag_cmp_result result
            /// If !found, algorithm not supported, cannot verify integrity, goto next key
            auto algo_iter_ = algorithm_map_.find(primary_store_value.algorithm);
            if (algo_iter_ != algorithm_map_.end()) {
                /// map pointer @algo_iter_->second points at a function
                /// dereference && call with appropriate parameters
                /// ex) second == &Sha1Integrity(std::string value_str, std::string tag_str)
                tag_cmp_result = (this->*(algo_iter_->second))(valueString, primary_store_value.tag); //NOLINT
                if (!tag_cmp_result) {
                    results->push_back(it->Key());
                }
            }
        } ///end else

        ///#6 Check for Time Out,
        ///then check for Interrupt,
        ///else continue iteration
        if (timer->IsTimeout()) {
            *start_key_contain = it->Key();///store last key processed
            delete it;
            timer->SetExpired();
            return StoreOperationStatus_SUCCESS;
        } else if (timer->ShouldBeInterrupted()) {
            *start_key_contain = it->Key();///store last key processed
            delete it;
            timer->SetInterrupt();
            return StoreOperationStatus_SUCCESS;
        } else {
            prev_key_temp = it->Key();
            it_status = it->Next();
            value_container.clear_value(); ///clear for re-use
        }
    }///end while loop

    ///#7 store last key processed and clean up iterator
    *start_key_contain = prev_key_temp;
    delete it;
    if (it_status == IteratorStatus_INTERNAL_ERROR) {
        LOG(ERROR) << "IE iterator";
        return StoreOperationStatus_INTERNAL_ERROR;
    } else {
        return StoreOperationStatus_SUCCESS;
    }
}

const std::string SkinnyWaist::GetKey(const std::string& key, bool next) {
    PrimaryStoreIteratorInterface* it = primary_store_.Find(key);
    it->Init();
    if (next) {
        it->Next();
    } else {
        it->Prev();
    }
    return it->Key();
}

bool SkinnyWaist::Sha1Integrity(std::string value_str, std::string tag_str) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(value_str.data()), value_str.size(), hash);
    std::string hashStr = string(hash, hash + sizeof hash / sizeof hash[0]);
    int cmp_result; //memory compare result indication
    cmp_result = memcmp(hashStr.c_str(), tag_str.c_str(), sizeof(hashStr));//NOLINT
    if (cmp_result != 0) { //comparision failure (no match)
        return false;
    }
    return true;
}

bool SkinnyWaist::Sha2Integrity(std::string value_str, std::string tag_str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(value_str.data()), value_str.size(), hash);
    std::string hashStr = string(hash, hash + sizeof hash / sizeof hash[0]);
    int cmp_result; //memory compare result indication
    cmp_result = memcmp(hashStr.c_str(), tag_str.c_str(), sizeof(hashStr));//NOLINT
    if (cmp_result != 0) { //comparision failure (no match)
        return false;
    }
    return true;
}

bool SkinnyWaist::Sha3Integrity(std::string value_str, std::string tag_str) {
    /// Not Yet Implemented / Supported. Return True incase a key specifies
    /// this algorithm in it's tag
    return true;
}

bool SkinnyWaist::Crc32Integrity(std::string value_str, std::string tag_str) {
    /// Not Yet Implemented / Supported. Return True incase a key specifies
    /// this algorithm in it's tag
    return true;
}

bool SkinnyWaist::Crc64Integrity(std::string value_str, std::string tag_str) {
    /// Not Yet Implemented / Supported. Return True incase a key specifies
    /// this algorithm in it's tag
    return true;
}
