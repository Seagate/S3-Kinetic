#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "product_flags.h"
#include "message_processor.h"

#include "kinetic/kinetic_connection_factory.h"
#include "kinetic/incoming_value.h"
#include "pretty_print_bytes.h"
#include "pthreads_mutex_guard.h"
#include "version_info.h"
#include "p2p_request_manager.h"
#include "server.h"
#include "security_manager.h"
#include "mem/DynamicMemory.h"
#include "BatchSet.h"

using namespace com::seagate::kinetic::proto;//NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using com::seagate::kinetic::SecurityManager;

MessageProcessor::MessageProcessor(
    AuthorizerInterface& authorizer,
    SkinnyWaistInterface& skinny_waist,
    Profiler &profiler,
    ClusterVersionStoreInterface& cluster_version_store,
    const std::string& firmware_update_tmp_dir,
    uint32_t max_response_size_bytes,
    P2PRequestManagerInterface& p2p_request_manager,
    GetLogHandler& get_log_handler,
    SetupHandler& setup_handler,
    PinOpHandlerInterface& pinop_handler,
    PowerManager& power_manager,
    Limits& limits,
    STATIC_DRIVE_INFO static_drive_info,
    UserStoreInterface& user_store)
        : authorizer_(authorizer),
        skinny_waist_(skinny_waist),
        profiler_(profiler),
        cluster_version_store_(cluster_version_store),
        firmware_update_tmp_dir_(firmware_update_tmp_dir),
        max_response_size_bytes_(max_response_size_bytes),
        p2p_request_manager_(p2p_request_manager),
        get_log_handler_(get_log_handler),
        setup_handler_(setup_handler),
        pinop_handler_(pinop_handler),
        power_manager_(power_manager),
        limits_(limits),
        static_drive_info_(static_drive_info),
        user_store_(user_store) {
            CHECK(!pthread_rwlock_init(&ise_rw_lock_, NULL));
        }

MessageProcessor::~MessageProcessor() {
    CHECK(!pthread_rwlock_destroy(&ise_rw_lock_));
}

uint32_t MessageProcessor::AlgorithmToUInt32(proto::Command_Algorithm algorithm) {
    return static_cast<uint32_t>(algorithm);
}

proto::Command_Algorithm MessageProcessor::UInt32ToAlgorithm(uint32_t algorithm) {
    google::protobuf::EnumValueDescriptor const *descriptor =
            proto::Command_Algorithm_descriptor()->FindValueByNumber(algorithm);

    if (!descriptor) {
        // Not a valid algorithm so use the default
        VLOG(2) << "Attempted to convert invalid Command_Algorithm " << algorithm;//NO_SPELL

        return proto::Command_Algorithm_SHA1;
    } else {
        return static_cast<proto::Command_Algorithm>(algorithm);
    }
}

void MessageProcessor::Get(const Command& command, Command *command_response,
        NullableOutgoingValue *response_value, RequestContext& request_context,
        uint64_t userid) {
    VLOG(3) << "Received command: GET " << PrettyPrintBytes(command.body().keyvalue().key());

    bool metadata_only = command.body().keyvalue().metadataonly();

    if (metadata_only) {
        command_response->mutable_body()->mutable_keyvalue()->set_metadataonly(true);

        // SkinnyWaistInterface.Get() allows passing NULL for the output value. The implementations
        // will skip loading large values from the filestore if this is NULL
        response_value = NULL;
    }

    command_response->mutable_body()->mutable_keyvalue()->
        set_key(command.body().keyvalue().key());
    PrimaryStoreValue primary_store_value;
    Event get;
    profiler_.Begin(kSkinnyWaistGet, &get);
    switch (skinny_waist_.Get(
            userid,
            command.body().keyvalue().key(),
            &primary_store_value,
            request_context,
            response_value)) {
        case StoreOperationStatus_SUCCESS:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_SUCCESS);
            command_response->mutable_body()->mutable_keyvalue()->
                set_dbversion(primary_store_value.version);
            command_response->mutable_body()->mutable_keyvalue()->
                set_tag(primary_store_value.tag);
            command_response->mutable_body()->mutable_keyvalue()->
                set_algorithm(UInt32ToAlgorithm(primary_store_value.algorithm));
            break;
        case StoreOperationStatus_NOT_FOUND:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_FOUND);
            command_response->mutable_status()->
                set_statusmessage("Key not found");
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->
                set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_STORE_CORRUPT:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                set_statusmessage("Internal DB corruption");
            break;
        case StoreOperationStatus_DATA_CORRUPT:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_PERM_DATA_ERROR);
            command_response->mutable_status()->
                set_statusmessage("KEY FOUND, BUT COULD NOT READ");
            break;
        case StoreOperationStatus_INVALID_REQUEST:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
            command_response->mutable_status()->
                set_statusmessage("Key is empty");
            break;
        default:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
    profiler_.End(get);
}

void MessageProcessor::GetVersion(const Command& command, Command *command_response,
        RequestContext& request_context, uint64_t userid) {
    VLOG(3) << "Received command: GETVERSION " << PrettyPrintBytes(command.body().keyvalue().key());//NO_SPELL

    command_response->mutable_body()->mutable_keyvalue()->
        set_key(command.body().keyvalue().key());
    std::string version_for_key;
    switch (skinny_waist_.GetVersion(
            userid,
            command.body().keyvalue().key(),
            &version_for_key,
            request_context)) {
        case StoreOperationStatus_SUCCESS:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_SUCCESS);
            command_response->mutable_body()->mutable_keyvalue()->
                set_dbversion(version_for_key);
            break;
        case StoreOperationStatus_NOT_FOUND:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_FOUND);
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->
                set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_STORE_CORRUPT:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                    set_statusmessage("Internal DB corruption");
            break;
        case StoreOperationStatus_INVALID_REQUEST:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
            command_response->mutable_status()->
                set_statusmessage("Key is empty");
            break;
       default:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
}

void MessageProcessor::GetNext(const Command& command, Command *command_response,
        NullableOutgoingValue *response_value, RequestContext& request_context, uint64_t userid) {
    VLOG(3) << "Received command: GETNEXT " << PrettyPrintBytes(command.body().keyvalue().key());//NO_SPELL

    bool metadata_only = command.body().keyvalue().metadataonly();

    if (metadata_only) {
        command_response->mutable_body()->mutable_keyvalue()->set_metadataonly(true);

        // SkinnyWaistInterface.Get() allows passing NULL for the output value. The implementations
        // will skip loading large values from the filestore if this is NULL
        response_value = NULL;
    }
    std::string key;
    PrimaryStoreValue primary_store_value;
    switch (skinny_waist_.GetNext(
            userid,
            command.body().keyvalue().key(),
            &key,
            &primary_store_value,
            response_value,
            request_context)) {
        case StoreOperationStatus_SUCCESS:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_SUCCESS);
            command_response->mutable_body()->mutable_keyvalue()->set_key(key);
            command_response->mutable_body()->mutable_keyvalue()->
                set_dbversion(primary_store_value.version);
            command_response->mutable_body()->mutable_keyvalue()->
                set_tag(primary_store_value.tag);
            command_response->mutable_body()->mutable_keyvalue()->
                    set_algorithm(UInt32ToAlgorithm(primary_store_value.algorithm));
            break;
        case StoreOperationStatus_NOT_FOUND:
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_FOUND);
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->
                set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_STORE_CORRUPT:
        LOG(ERROR) << "IE Store Status";
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                    set_statusmessage("Internal DB corruption");
            break;
        case StoreOperationStatus_DATA_CORRUPT:
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_PERM_DATA_ERROR);
            command_response->mutable_status()->
                set_statusmessage("KEY FOUND, BUT COULD NOT READ");
            break;
        default:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
}

void MessageProcessor::GetPrevious(const Command& command, Command *command_response,
        NullableOutgoingValue *response_value, RequestContext& request_context, uint64_t userid) {
    VLOG(3) << "Received command: GETPREV " << PrettyPrintBytes(command.body().keyvalue().key());//NO_SPELL

    bool metadata_only = command.body().keyvalue().metadataonly();

    if (metadata_only) {
        command_response->mutable_body()->mutable_keyvalue()->set_metadataonly(true);

        // SkinnyWaistInterface.Get() allows passing NULL for the output value. The implementations
        // will skip loading large values from the filestore if this is NULL
        response_value = NULL;
    }
    std::string key;
    PrimaryStoreValue primary_store_value;
    switch (skinny_waist_.GetPrevious(
            userid,
            command.body().keyvalue().key(),
            &key,
            &primary_store_value,
            response_value,
            request_context)) {
        case StoreOperationStatus_SUCCESS:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_SUCCESS);
            command_response->mutable_body()->mutable_keyvalue()->set_key(key);
            command_response->mutable_body()->mutable_keyvalue()->
                set_dbversion(primary_store_value.version);
            command_response->mutable_body()->mutable_keyvalue()->
                set_tag(primary_store_value.tag);
            command_response->mutable_body()->mutable_keyvalue()->
                    set_algorithm(UInt32ToAlgorithm(primary_store_value.algorithm));
            break;
        case StoreOperationStatus_NOT_FOUND:
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_FOUND);
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->
                set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_STORE_CORRUPT:
        LOG(ERROR) << "IE Store Status";
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                    set_statusmessage("Internal DB corruption");
            break;
        case StoreOperationStatus_DATA_CORRUPT:
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_PERM_DATA_ERROR);
            command_response->mutable_status()->
                set_statusmessage("KEY FOUND, BUT COULD NOT READ");
            break;
        default:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_body()->mutable_keyvalue()->set_key(command.body().keyvalue().key());
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
}

void MessageProcessor::GetKeyRange(Command& command, Command *command_response,
        RequestContext& request_context, uint64_t userid) {
    VLOG(3) << "Received command: GETKEYRANGE " <<//NO_SPELL
        PrettyPrintBytes(command.body().range().startkey()) <<
        " -> " << PrettyPrintBytes(command.body().range().endkey());

    std::string start_key = command.body().range().startkey();
    std::string end_key = command.body().range().endkey();
    bool start_key_inclusive_flag = command.body().range().startkeyinclusive();
    bool end_key_inclusive_flag = command.body().range().endkeyinclusive();
    /// authorize the requested range
    //Requested start_key and end_key must be confined to one continuous key scope region.
    //If there are no key scope region between the requested keys, then it is considered as NOT_AUTHORIZED request
    //If the request is not confined to one continuous key scope region, then it is considered as INVALID request
    AuthorizationStatus auth_status = authorizer_.AuthorizeKeyRange(userid, Domain::kRange, start_key, end_key, start_key_inclusive_flag,
        end_key_inclusive_flag, request_context);
    if (auth_status == AuthorizationStatus_NOT_AUTHORIZED) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
                set_statusmessage("permission denied");
        return;
    } else if (auth_status == AuthorizationStatus_INVALID_REQUEST) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
                set_statusmessage("Invalid request");
        return;
    }

    std::vector<std::string> keys;
    size_t max_results = 0;
    if (command.body().range().has_maxreturned() && command.body().range().maxreturned() > 0) {
        max_results = static_cast<size_t>(command.body().range().maxreturned());
    }

    // Enforce limit on number of keys
    if (max_results > limits_.max_key_range_count()) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
                set_statusmessage("Key limit exceeded.");
        return;
    }
    StoreOperationStatus store_operation_status = StoreOperationStatus_SUCCESS;

    if (max_results > 0) {
        store_operation_status = skinny_waist_.GetKeyRange(
            userid,
            start_key,
            start_key_inclusive_flag,
            end_key,
            end_key_inclusive_flag,
            max_results,
            command.body().range().reverse(),
            &keys,
            request_context);
    }
    if (store_operation_status == StoreOperationStatus_SUCCESS) {
        command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
        for (std::vector<std::string>::iterator it = keys.begin(); it != keys.end(); ++it) {
            command_response->mutable_body()->mutable_range()->add_keys(*it);
        }
    } else if (store_operation_status == StoreOperationStatus_STORE_CORRUPT) {
        LOG(ERROR) << "IE Store Status";
        command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
        command_response->mutable_status()->
                set_statusmessage("Internal DB corruption");
    } else if (store_operation_status == StoreOperationStatus_AUTHORIZATION_FAILURE) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
            set_statusmessage("permission denied");
    } else {
        LOG(ERROR) << "IE Store Status";
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INTERNAL_ERROR);
    }
}

void MessageProcessor::Put(const Command& command, IncomingValueInterface* request_value,
                           Command *command_response, RequestContext& request_context,
                           uint64_t userid, int64_t connection_id) {
    proto::Command_KeyValue const& keyvalue = command.body().keyvalue();

    if (!keyvalue.has_tag()) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->set_statusmessage("Tag required");
        return;
    }

    command_response->mutable_body()->mutable_keyvalue();

    PrimaryStoreValue primary_store_value;
    std::string newversion = keyvalue.newversion();

    primary_store_value.version = newversion;
    primary_store_value.tag = keyvalue.tag();
    primary_store_value.algorithm = AlgorithmToUInt32(
        keyvalue.algorithm());

    // VLOG(2) << "Prepared PrimaryStoreValue " << &primary_store_value;//NO_SPELL

    Event put;
    profiler_.Begin(kSkinnyWaistPut, &put);

    #ifdef QOS_ENABLED
    bool guarantee_durable = ((keyvalue.synchronization() != proto::Command_Synchronization_FLUSH) &&
                              (!keyvalue.flush()) && (keyvalue.prioritizedqos(0) != Command_QoS_SHORT_LATENCY)); //NOLINT
    #else
    bool guarantee_durable = keyvalue.synchronization() != proto::Command_Synchronization_WRITEBACK; //NOLINT
    #endif  // QOS_ENABLED

    std::tuple<uint64_t, int64_t> token {command.header().sequence(), connection_id};//NOLINT

    switch (skinny_waist_.Put(
            userid,
            keyvalue.key(),
            keyvalue.dbversion(),
            primary_store_value,
            request_value,
            keyvalue.force(),
            guarantee_durable,
            request_context,
            token)) {
        case StoreOperationStatus_SUCCESS:
        {
            leveldb::Status status;
            #ifdef QOS_ENABLED
            if (!guarantee_durable) {
            #else
            if (keyvalue.synchronization() ==
                    proto::Command_Synchronization_FLUSH) {
            #endif // QOS_ENABLED
                // This is a flush, so we need to sync() the entire filesystem to make sure
                // all previous writes with the filesystemstore got written. We don't have
                // to worry about LevelDb writes because setting FLUSH makes us pass sync
                // to leveldb and all previous leveldb writes need to be durable for this
                // write to be durable.
//                sync();
                status = skinny_waist_.Flush();
            }
            if (status.ok()) {
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_SUCCESS);
            } else {
                command_response->mutable_status()->
                        set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                if (status.IsSuperblockIO() || status.IsCorruption()) {
                    command_response->mutable_status()->
                        set_statusmessage("Superblock is not writable");
                } else {
                    command_response->mutable_status()->
                        set_statusmessage("Failed to synchronize");
                }
            }
            break;
        }
        case StoreOperationStatus_VERSION_MISMATCH:
            VLOG(2) << "PUT version mismatch";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_VERSION_MISMATCH);
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            VLOG(2) << "PUT authorization failure";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->
                set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_FROZEN:
            VLOG(2) << "PUT frozen";
        #ifdef NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_FROZEN);
        #else
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NO_SPACE);
        #endif // NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_statusmessage("Device is in Frozen state. Only an ISE can make the device writable");//NOLINT
            break;
        case StoreOperationStatus_MEDIA_FAULT:
            VLOG(2) << "PUT media fault";
        #ifdef NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_MEDIA_FAULT);
        #else
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NO_SPACE);
        #endif // NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_statusmessage("Device can no longer write. An ISE will NOT make the device writable");//NOLINT
            break;
        case StoreOperationStatus_NO_SPACE:
            VLOG(2) << "PUT no space";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NO_SPACE);
            break;
        case StoreOperationStatus_STORE_CORRUPT:
            VLOG(2) << "PUT reports internal corruption";
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                    set_statusmessage("Internal DB corruption");
            break;
        case StoreOperationStatus_SUPERBLOCK_IO:
            VLOG(2) << "Superblock IO: Superblock is not writable";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                    set_statusmessage("Superblock is not writable");

            break;
        case StoreOperationStatus_INVALID_REQUEST:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
            command_response->mutable_status()->
                set_statusmessage("Key is empty");
            break;
        default:
            VLOG(2) << "Unexpected PUT result";
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }

    profiler_.End(put);
}

void MessageProcessor::Delete(const Command& command, Command *command_response,
        RequestContext& request_context, uint64_t userid, int64_t connection_id) {
    VLOG(3) << "Received command: DELETE " << PrettyPrintBytes(command.body().keyvalue().key());

    command_response->mutable_body()->mutable_keyvalue();

    #ifdef QOS_ENABLED
    bool guarantee_durable = ((command.body().keyvalue().synchronization() != proto::Command_Synchronization_FLUSH)
                                                                           && (!command.body().keyvalue().flush())
                                                                           && (command.body().keyvalue().prioritizedqos(0) != Command_QoS_SHORT_LATENCY)); //NOLINT
    #else
    bool guarantee_durable =
            command.body().keyvalue().synchronization() !=
                    proto::Command_Synchronization_WRITEBACK;
    #endif  // QOS_ENABLED

    std::tuple<uint64_t, int64_t> token {command.header().sequence(), connection_id};//NOLINT

    switch (skinny_waist_.Delete(
            userid,
            command.body().keyvalue().key(),
            command.body().keyvalue().dbversion(),
            command.body().keyvalue().force(),
            guarantee_durable,
            request_context,
            token)) {
        case StoreOperationStatus_SUCCESS:
        {
            leveldb::Status status;
            #ifdef QOS_ENABLED
            if (!guarantee_durable) {
            #else
            if (command.body().keyvalue().synchronization() ==
                    proto::Command_Synchronization_FLUSH) {
            #endif  // QOS_ENABLED
                // Sync needed for same reason as PUT above
//                sync();
                  status = skinny_waist_.Flush();
            }
            if (status.ok()) {
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_SUCCESS);
            } else {
                command_response->mutable_status()->
                        set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                if (status.IsSuperblockIO() || status.IsCorruption()) {
                    command_response->mutable_status()->
                        set_statusmessage("Superblock is not writable");
                } else {
                    command_response->mutable_status()->
                        set_statusmessage("Failed to synchronize");
                }
            }
            break;
        }
        case StoreOperationStatus_NOT_FOUND:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_FOUND);
            break;
        case StoreOperationStatus_VERSION_MISMATCH:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_VERSION_MISMATCH);
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->
                set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_STORE_CORRUPT:
            LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                    set_statusmessage("Internal DB corruption");
            break;
        case StoreOperationStatus_FROZEN:
            VLOG(2) << "DELETE frozen";
        #ifdef NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_FROZEN);
        #else
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NO_SPACE);
        #endif // NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_statusmessage("Device is in Frozen state. Only an ISE can make the device writable");//NOLINT
            break;
        case StoreOperationStatus_MEDIA_FAULT:
            VLOG(2) << "DELETE media fault";
        #ifdef NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_MEDIA_FAULT);
        #else
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NO_SPACE);
        #endif // NO_SPACE_REPORTING
            command_response->mutable_status()->
                set_statusmessage("Device can no longer write. An ISE will NOT make the device writable");//NOLINT
            break;
        case StoreOperationStatus_NO_SPACE:
            VLOG(2) << "DELETE no space";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NO_SPACE);
            command_response->mutable_status()-> set_statusmessage("Drive is full.  DELETE command is temporarily not accepted.");
            break;
        case StoreOperationStatus_SUPERBLOCK_IO:
            VLOG(2) << "Superblock is not writable";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                set_statusmessage("Superblock is not writable");//NOLINT
            break;
        case StoreOperationStatus_INVALID_REQUEST:
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
            command_response->mutable_status()->
                set_statusmessage("Key is empty");
            break;
        default:
        LOG(ERROR) << "IE Store Status";
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
}

void MessageProcessor::Flush(const Command& command, Command *command_response, RequestContext& request_context, uint64_t userid) {
    // To complete a flush we need to sync() the entire filesystem to make sure all previous
    // writes with the filesystemstore and all async LevelDB writes get persisted.
    // As documented, sync is "always successful"... no error handling available.
    if (!authorizer_.AuthorizeGlobal(userid, Domain::kWrite | Domain::kDelete, request_context)) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
                set_statusmessage("permission denied");
        return;
    }
    leveldb::Status status = skinny_waist_.Flush(true);
    if (status.ok()) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_SUCCESS);
    } else {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
        if (status.IsSuperblockIO() || status.IsCorruption()) {
            command_response->mutable_status()->
                set_statusmessage("Superblock is not writable");
        } else {
            command_response->mutable_status()->
                set_statusmessage("Failed to synchronize");
        }
    }
    sync();
}

bool MessageProcessor::Flush(bool toSST) {
    return skinny_waist_.Flush(toSST).ok();
}

MessageProcessor::ReadDomainStatus MessageProcessor::ReadDomain(
        const Command_Security_ACL_Scope &pb_domain, Domain *domain) {
    if (!pb_domain.permission_size()) {
        LOG(INFO) << "Domain incorrectly has no roles";
        return ReadDomainStatus_NO_ROLE;
    }

    domain->set_offset(pb_domain.offset());
    domain->set_value(pb_domain.value());
    domain->set_tls_required(pb_domain.tlsrequired());
    if (domain->offset() + domain->value().size() > limits_.max_key_size()) {
        LOG(INFO) << "Unreasonable combination of scope offset and value";
        return ReadDomainStatus_UNREASONABLE_OFFSET_VALUE;
    }
    role_t roles = 0;

    for (auto it = pb_domain.permission().begin(); it != pb_domain.permission().end(); ++it) {
        switch (*it) {
            case Command_Security_ACL_Permission_READ:
                roles |= Domain::kRead;
                break;
            case Command_Security_ACL_Permission_WRITE:
                roles |= Domain::kWrite;
                break;
            case Command_Security_ACL_Permission_DELETE:
                roles |= Domain::kDelete;
                break;
            case Command_Security_ACL_Permission_RANGE:
                roles |= Domain::kRange;
                break;
            case Command_Security_ACL_Permission_SETUP:
                roles |= Domain::kSetup;
                break;
            case Command_Security_ACL_Permission_P2POP:
                roles |= Domain::kP2Pop;
                break;
            case Command_Security_ACL_Permission_GETLOG:
                roles |= Domain::kGetLog;
                break;
            case Command_Security_ACL_Permission_SECURITY:
                roles |= Domain::kSecurity;
                break;
            case Command_Security_ACL_Permission_POWER_MANAGEMENT:
                roles |= Domain::kPower;
                break;
            default:
                LOG(INFO) << "Invalid role specified in a domain";
                return ReadDomainStatus_INVALID_ROLE;
        }
    }
    domain->set_roles(roles);
    return ReadDomainStatus_SUCCESS;
}

void MessageProcessor::Security(const Command &command, Command *command_response,
        RequestContext& request_context, uint64_t userid) {
    VLOG(3) << "==== Received command: SECURITY";

    if (!authorizer_.AuthorizeGlobal(userid, Domain::kSecurity, request_context)) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
                set_statusmessage("permission denied");
        return;
    }

    if (!command.body().has_security()) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        return;
    }

    // Validate security structure
    if (!ValidateSecurity(command, command_response)) {
        return;
    }

    Command_Security security = command.body().security();

    SecurityManager sed_manager;

    std::string oldpin;
    std::string newpin;
    std::string pin_command;

    if (security.securityoptype() == Command_Security_SecurityOpType_LOCK_PIN_SECURITYOP) {
        oldpin = security.oldlockpin();
        newpin = security.newlockpin();

        // Check and eforce pin size
        if (!IsValidPinSize(command_response, newpin)) {
            return;
        }

        pin_command = "lock";
        if (!CheckPinStatusSuccess(
                command_response,
                sed_manager.SetPin(
                    newpin,
                    oldpin,
                    PinIndex::LOCKPIN,
                    static_drive_info_.drive_sn,
                    static_drive_info_.supports_SED,
                    static_drive_info_.sector_size,
                    static_drive_info_.non_sed_pin_info_sector_num),
                pin_command.c_str(),
                true)) {
            return;
        }
        // If the new pin is empty, disable at rest data protection
        if (newpin.compare("") == 0) {
            pin_command = "disable";
            if (!CheckPinStatusSuccess(command_response,
                sed_manager.Disable(newpin), pin_command.c_str(), false)) {
                return;
            }
        } else {
            pin_command = "enable";
            if (!CheckPinStatusSuccess(command_response,
                sed_manager.Enable(newpin), pin_command.c_str(), false)) {
                return;
            }
        }
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_SUCCESS);

    } else if (security.securityoptype() == Command_Security_SecurityOpType_ERASE_PIN_SECURITYOP) {
        oldpin = security.olderasepin();
        newpin = security.newerasepin();

        // Check and eforce pin size
        if (!IsValidPinSize(command_response, newpin)) {
            return;
        }

        pin_command = "erase";
        if (!CheckPinStatusSuccess(
                command_response,
                sed_manager.SetPin(
                    newpin,
                    oldpin,
                    PinIndex::ERASEPIN,
                    static_drive_info_.drive_sn,
                    static_drive_info_.supports_SED,
                    static_drive_info_.sector_size,
                    static_drive_info_.non_sed_pin_info_sector_num),
                pin_command.c_str(),
                true)) {
            return;
        }
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_SUCCESS);

    } else if (security.securityoptype() == Command_Security_SecurityOpType_ACL_SECURITYOP) {
        // ACLs request
        VLOG(4) <<"Security() command:\n"<< command.DebugString();
        std::list<User> users;
        for (int i = 0; i < security.acl_size(); ++i) {
            Command_Security_ACL acl = security.acl(i);
            if (acl.hmacalgorithm() == Command_Security_ACL_HMACAlgorithm_INVALID_HMAC_ALGORITHM) {
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_NO_SUCH_HMAC_ALGORITHM);
                return;
            }
            std::list<Domain> domains;
            for (int j = 0; j < acl.scope_size(); ++j) {
                Domain domain;
                switch (ReadDomain(acl.scope(j), &domain)) {
                    case ReadDomainStatus_SUCCESS:
                        // Domain is valid so don't set errors or return
                        break;
                    case ReadDomainStatus_INVALID_ROLE:
                        command_response->mutable_status()->
                            set_statusmessage("Permission is invalid in acl");
                        command_response->mutable_status()->
                            set_code(Command_Status_StatusCode_INVALID_REQUEST);
                        return;
                    case ReadDomainStatus_NO_ROLE:
                        command_response->mutable_status()->
                            set_statusmessage("No permission set in acl");
                        command_response->mutable_status()->
                            set_code(Command_Status_StatusCode_INVALID_REQUEST);
                        return;
                    case ReadDomainStatus_INVALID_DOMAIN_OFFSET:
                        command_response->mutable_status()->
                            set_statusmessage("Invalid scope offset");
                        command_response->mutable_status()->
                            set_code(Command_Status_StatusCode_INVALID_REQUEST);
                        return;
                    case ReadDomainStatus_UNREASONABLE_OFFSET_VALUE:
                        command_response->mutable_status()->
                            set_statusmessage("Unreasonable combination of scope offset and value");
                        command_response->mutable_status()->
                            set_code(Command_Status_StatusCode_INVALID_REQUEST);
                        return;
                    default:
                    LOG(ERROR) << "IE Domain";
                        command_response->mutable_status()->
                        set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                        return;
                }
                domains.push_back(domain);
            }
            User user(acl.identity(), acl.key(), domains);
            if (acl.maxpriority() < Command_Priority::Command_Priority_LOWEST || acl.maxpriority() > Command_Priority::Command_Priority_HIGHEST) {
                command_response->mutable_status()->
                    set_statusmessage("Invalid maximum command priority");
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INVALID_REQUEST);
                return;
            } else {
                user.maxPriority(acl.maxpriority());
            }
            users.push_back(user);
        }

        switch (skinny_waist_.Security(userid,
                users,
                request_context)) {
            case StoreOperationStatus_SUCCESS:
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_SUCCESS);
                break;
            case StoreOperationStatus_AUTHORIZATION_FAILURE:
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
                command_response->mutable_status()->
                    set_statusmessage("permission denied");
                break;
            case StoreOperationStatus_EXCEED_LIMIT:
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INVALID_REQUEST);
                command_response->mutable_status()->set_statusmessage("Number of ids exceeds limit.");
                break;
            case StoreOperationStatus_DUPLICATE_ID:
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INVALID_REQUEST);
                command_response->mutable_status()->set_statusmessage("Duplicate user.");
                break;
            case Command_Status_StatusCode_INTERNAL_ERROR:
                command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                command_response->mutable_status()->set_statusmessage("Failed to store ids.");
                break;
            default:
                LOG(ERROR) << "IE Store Status";
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                break;
        }
    } else {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
            set_statusmessage("unknown securityOpType");
    }
}

bool MessageProcessor::CheckPinStatusSuccess(Command *command_response, PinStatus status,
    string command, bool change) {
    char error_message[100];
    switch (status) {
        case PinStatus::PIN_SUCCESS:
            return true;
        case PinStatus::AUTH_FAILURE:
            LOG(ERROR) << "AUTH_FAILURE on pin based command";//NO_SPELL
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            break;
        case PinStatus::INTERNAL_ERROR:
            LOG(ERROR) << "INTERNAL_ERROR on pin based command";//NO_SPELL
            command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }

    // Set up error message depending on command requested
    if (change) {
        sprintf(error_message, "Failed to change %s pin", command.c_str());
    } else {
        sprintf(error_message, "Failed to %s lock", command.c_str());
    }
    command_response->mutable_status()->set_statusmessage(error_message);
    return false;
}

bool MessageProcessor::ValidateSecurity(const Command &command, Command *command_response) {
    Command_Security security = command.body().security();

    if (security.securityoptype() == Command_Security_SecurityOpType_INVALID_SECURITYOP) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
            set_statusmessage("INVALID_SECURITYOP, please specify a seurityOpType");
        return false;
    }

    #ifdef ISE_AND_LOCK_DISABLED
    if (security.securityoptype() == Command_Security_SecurityOpType_LOCK_PIN_SECURITYOP) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
            set_statusmessage("Lock not supported");
        return false;
    }
    #endif

    return true;
}

bool MessageProcessor::IsValidPinSize(Command *command_response, string pin) {
    if (pin.length() <= limits_.max_pin_size()) {
        return true;
    }

    command_response->mutable_status()->
        set_code(Command_Status_StatusCode_INVALID_REQUEST);
    command_response->mutable_status()->
        set_statusmessage("Pin provided is too long");
    return false;
}

void MessageProcessor::Unimplemented(const Command &command, Command *command_response) {
    VLOG(3) << "Received unimplemented command";

    command_response->mutable_status()->
        set_code(Command_Status_StatusCode_INVALID_REQUEST);
    std::string error_message("Unimplemented command: ");
    error_message.append(Command_MessageType_Name(command.header().messagetype()));
    command_response->mutable_status()->set_statusmessage(error_message);
}

void MessageProcessor::Setup(const Command &command, IncomingValueInterface* request_value,
    Command *command_response, RequestContext& request_context, uint64_t userid, bool corrupt) {
    setup_handler_.ProcessRequest(command, request_value, command_response,
            request_context, userid, corrupt);
}

void MessageProcessor::GetLog(const proto::Command &command, Command *command_response,
        NullableOutgoingValue *response_value, RequestContext& request_context,
        uint64_t userid, bool corrupt) {
    VLOG(3) << "Received command GETLOG";//NO_SPELL

    get_log_handler_.ProcessRequest(command, command_response,
            response_value, request_context, userid, corrupt);
}

void MessageProcessor::Peer2PeerPush(const proto::Command &command, Command *command_response,
        RequestContext& request_context, uint64_t userid) {
    VLOG(3) << "Received command P2P Push";//NO_SPELL

    p2p_request_manager_.ProcessRequest(command, command_response, request_context, userid);
}

void MessageProcessor::SetClusterVersion(proto::Command *command_response) {
    int64_t current_cluster_version = cluster_version_store_.GetClusterVersion();
    command_response->mutable_header()->set_clusterversion(current_cluster_version);
}

void MessageProcessor::SetLogInfo(proto::Command *command_response,
        NullableOutgoingValue *response_value,
        std::string device_name,
        proto::Command_GetLog_Type type) {
    get_log_handler_.SetLogInfo(command_response, response_value, device_name, type);

    // Set power level
    command_response->mutable_body()->mutable_getlog()->mutable_configuration()
                    ->set_currentpowerlevel(power_manager_.GetPowerLevel());
}

//////////////////////////////////////////////////////////////////////
///MediaScan
///For each key in range (startkey -> endkey):
/// locate, evaluate and return damaged keys
///---------------------------------------
///Message Processor maintains Operation Progress / State,
///prepares Keys, timers and other related parameters
///for Skinny Waist
///---------------------------------------
///-----State Management Overview---------
///Saving Operation Staterelies on manipulating
///the key fields within the Response Command to record progress.
///Original “Command” Message is never altered, allowing for the
///preservation of the Command’s original state (i.e. start and end keys).
///-----------------------------------------
///----On Return from Skinny Waist:---------
///The “Keys” field within the Range in a Response Command contains only
///the damaged keys. For this reason, the “Keys” field is not a suitable candidate
///for determining operation progress.
///
///**This is the case because the most recent damaged key stored,
///if any, is not necessarily the last key seen by the scan.
///--------------------------------------------
void MessageProcessor::MediaScan(const Command &command,
        IncomingValueInterface* request_value,
        Command *command_response,
        NullableOutgoingValue *response_value,
        RequestContext& request_context,
        ConnectionTimeHandler* time_handler,
        uint64_t userid) {
    VLOG(3) << "Received command: MEDIASCAN " <<//NO_SPELL
        PrettyPrintBytes(command.body().range().startkey()) <<
        " -> " << PrettyPrintBytes(command.body().range().endkey());

    std::string start_key = command.body().range().startkey();
    std::string end_key = command.body().range().endkey();
    bool start_key_inclusive_flag = command.body().range().startkeyinclusive();
    bool end_key_inclusive_flag = command.body().range().endkeyinclusive();
    ///#1 authorize the requested range
    //Requested start_key and end_key must be confined to one continuous key scope region.
    //If there are no key scope region between the requested keys, then it is considered as NOT_AUTHORIZED request
    //If the request is not confined to one continuous key scope region, then it is considered as INVALID request
    AuthorizationStatus auth_status = authorizer_.AuthorizeKeyRange(userid, Domain::kRange, start_key, end_key, start_key_inclusive_flag,
        end_key_inclusive_flag, request_context);
    if (auth_status == AuthorizationStatus_NOT_AUTHORIZED) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
                set_statusmessage("permission denied");
        return;
    } else if (auth_status == AuthorizationStatus_INVALID_REQUEST) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
                set_statusmessage("Invalid request");
        return;
    }

    ///#2 Check Max Returned Field
    size_t max_results = 0;
    if (command.body().range().has_maxreturned() &&
            command.body().range().maxreturned() > 0) {
        max_results = static_cast<size_t>(command.body().range().maxreturned());
    }
    if (max_results > limits_.max_key_range_count()) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
                set_statusmessage("Key limit exceeded.");
        return;
    }

    ///#3 Check For Reverse
    if (command.body().range().reverse()) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
                set_statusmessage("Reverse Option Not Supported for MediaScan.");
        return;
    }

    ///Create quick reference to original Key arguments
    std::string original_start_key = command.body().range().startkey();
    std::string original_end_key = command.body().range().endkey();
    bool orginal_inclusive_start = command.body().range().startkeyinclusive();
    bool orginal_inclusive_end = command.body().range().endkeyinclusive();

    ///#4 If command has never run,
    /// Set up start and end key && inclusive arguments
    /// for First Time use of an Operation
    if (!time_handler->HasMadeProgress()) {
        command_response->
            mutable_body()->mutable_range()->set_startkey(original_start_key);
        command_response->
            mutable_body()->mutable_range()->set_endkey(start_key);
        command_response->
            mutable_body()->mutable_range()->
                set_endkeyinclusive(orginal_inclusive_end);
        command_response->mutable_body()->mutable_range()->set_startkeyinclusive(start_key_inclusive_flag);
    }

    ///#5 Setup keys to be used by Skinny Waist
    /// @param current_start_key -  represents the most recent key viewed
    /// this is held in the @command_response endkey.
    /// If no progress has been made, this key should match the original start
    std::string startkey_val_container;
    std::string current_start_key = command_response->mutable_body()->
        mutable_range()->endkey();

    /// #6 Get Start time from @connection_time_handler
    /// in order to measure time elapsed
    /// Set @started_ boolean = True, indicating command progress
    /// Reason: Interrupted commands have made progress, aka "started_" == True
    /// Indicates that start/end keys do not need "re-initializing" in future
    time_handler->SetTimeOpStart();
    time_handler->SetStarted();

    /// #7 Create Key Collection for Return
    std::vector<std::string> keys;
    StoreOperationStatus store_operation_status = StoreOperationStatus_SUCCESS;

    /// #8 Execute
    if (max_results > 0) {
        store_operation_status = skinny_waist_.MediaScan(
            userid,
            current_start_key,
            &startkey_val_container,
            command_response->mutable_body()->mutable_range()->startkeyinclusive(),
            end_key,
            end_key_inclusive_flag,
            max_results,
            &keys,
            request_context,
            time_handler);
    }

    ///#9 Evaluate return status of Operation
    /// Check for interrupt -or- Expired, set state accordingly, otherwise command is "finished"
    /// Save key state, reset appropriate timers before control is returned to @Connection_Handler
    /// if(store==success) && Expired, reset @time_handler's @Expired_ bool,set Expired Status code
    /// if(store==success) && interrupt, set inclusive start key == False
    /// if(store==success) && (!interrupt && !expired), reset original inclusive start, op finished
    if (store_operation_status == StoreOperationStatus_SUCCESS) {
        if (time_handler->GetExpired()) {
            time_handler->ResetExpired();///reset time handler's expired bool
            command_response->mutable_status()->set_code(Command_Status_StatusCode_EXPIRED);
        } else if (time_handler->GetInterrupt()) {
            command_response->mutable_body()->          ///-Avoid processing most recent key twice
                mutable_range()->set_startkeyinclusive(false);///-on Resume from interrupt state
            command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
        } else {
            command_response->mutable_body()->
                mutable_range()->set_startkeyinclusive(orginal_inclusive_start);
            command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
        }
    } else if (store_operation_status == StoreOperationStatus_STORE_CORRUPT) {
        LOG(ERROR) << "IE Store Status";
        command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
        command_response->mutable_status()->
                set_statusmessage("Internal DB corruption");
    } else {
        LOG(ERROR) << "IE Store Status";
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INTERNAL_ERROR);
    }

    ///#10 add keys to @command_response
    for (std::vector<std::string>::iterator it = keys.begin(); it != keys.end(); ++it) {
        command_response->mutable_body()->mutable_range()->add_keys(*it);
    }

    ///#11 Save Last Key Processed to indicate progress
    /// If the command is 100% finished , Last Key should match original endkey
    /// Otherwise, Last Key indicates how far Media Scan progressed
    command_response->mutable_body()->
        mutable_range()->set_endkey(startkey_val_container);
}

void MessageProcessor::ProcessPinMessage(const proto::Command &command,
        IncomingValueInterface* request_value,
        proto::Command *command_response,
        NullableOutgoingValue *response_value,
        RequestContext& request_context,
        const proto::Message_PINauth& pin_auth,
        Connection* connection) {
    if (request_value->size() > ((size_t)0) &&
        (command.header().messagetype() != Command_MessageType_PUT &&
            command.header().messagetype() != Command_MessageType_SETUP)) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
            set_statusmessage("Unexpected value");
    } else {
        // Allowing an ISE to run in parallel with any other operation is very bad since ISE
        // closes the underlying store, unmounts the FS, etc. To prevent this we have a RW lock.
        // ISE acquires the lock for writing and everything else acquires it for reading
        if (static_drive_info_.supports_SED == false && command.body().pinop().pinoptype() == Command_PinOperation_PinOpType_SECURE_ERASE_PINOP) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
            set_statusmessage("ISE not supported for non-SED devices");
        } else {
            pinop_handler_.ProcessRequest(command,
                    request_value,
                    command_response,
                    request_context,
                    pin_auth, connection);
        }
    }
}

void MessageProcessor::SetPowerLevel(const Command &command,
       Command *command_response,
       RequestContext& request_context,
       uint64_t userid) {
    if (!authorizer_.AuthorizeGlobal(userid, Domain::kPower, request_context)) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
                set_statusmessage("permission denied");
        return;
    }

    power_manager_.ProcessRequest(command, command_response);
}

void MessageProcessor::ProcessMessage(ConnectionRequestResponse& reqResp,
        NullableOutgoingValue *response_value,
        RequestContext& request_context,
        int64_t connection_id,
        int connFd,
        bool connIdMismatched,
        bool corrupt) {
    Command& command = *(reqResp.command());
    IncomingValueInterface* request_value = reqResp.request_value();
    Command* command_response = reqResp.response_command();
    if (command_response->status().code() == Command_Status_StatusCode_INVALID_STATUS_CODE) {
        command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    }
    uint64_t userid = reqResp.request()->hmacauth().identity();
    ConnectionTimeHandler* time_handler = reqResp.time_handler();
    int64_t current_cluster_version = cluster_version_store_.GetClusterVersion();
    if ((command_response->status().code() != Command_Status_StatusCode_SUCCESS &&
        command_response->status().code() != Command_Status_StatusCode_NO_SPACE) &&
        !ConnectionHandler::_batchSetCollection.isBatch(&command)) {
        // No need to process because the normal command was already in error
    // else if the command was already in error, is a batch command but not batchable.
    } else if ((command_response->status().code() != Command_Status_StatusCode_SUCCESS &&
                    command_response->status().code() != Command_Status_StatusCode_NO_SPACE) &&
                    ConnectionHandler::_batchSetCollection.isBatch(&command) &&
                    !ConnectionHandler::_batchSetCollection.isBatchableCommand(&command)) {
        BatchSet* batchSet = NULL;
        batchSet = ConnectionHandler::_batchSetCollection.getBatchSet(command.header().batchid(), connection_id);
        if (batchSet) {
            bool bAdded = ConnectionHandler::_batchSetCollection.addCommand(reqResp.request(),
                          (Command*)&command, request_value, connection_id, *command_response);
            if (bAdded && batchSet->isGreaterMax()) {
               command_response->mutable_status()->set_code(Command_Status_StatusCode_INVALID_BATCH);
               command_response->mutable_status()->set_statusmessage("Exceed maximum allowable batch's size");
            }
        } else {
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INVALID_BATCH);
            command_response->mutable_status()->set_statusmessage("Batch command without a batch");
        }
        if (request_value->size() > 0) {
            smr::DynamicMemory::getInstance()->deallocate(request_value->size());
        }
        request_value->FreeUserValue();
        request_value->SetBuffValueToNull();
    } else if (current_cluster_version != command.header().clusterversion()) {
        LOG(INFO) << "Received incorrect cluster version: " << command.header().clusterversion();
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_VERSION_FAILURE);
        command_response->mutable_status()->
                set_statusmessage("CLUSTER_VERSION_FAILURE");
        command_response->mutable_header()->set_clusterversion(current_cluster_version);
    } else if (request_value->size() > ((size_t)0) &&
        (command.header().messagetype() != Command_MessageType_PUT &&
            command.header().messagetype() != Command_MessageType_SETUP)) {
        command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
            set_statusmessage("Unexpected value");
    } else {
        // Allowing an ISE to run in parallel with any other operation is very bad since ISE
        // closes the underlying store, unmounts the FS, etc. To prevent this we have a RW lock.
        // ISE acquires the lock for writing and everything else acquires it for reading
//        CHECK(!pthread_rwlock_rdlock(&ise_rw_lock_));
        bool freeValue = true;
        switch (command.header().messagetype()) {
            case Command_MessageType_GETKEYRANGE:
                if (!command.body().range().has_maxreturned()) {
                    command.mutable_body()->mutable_range()->set_maxreturned(limits_.max_key_range_count());
                }
                GetKeyRange(command, command_response, request_context, userid);
                break;
            case Command_MessageType_GET:
                Get(command, command_response, response_value, request_context, userid);
                break;
            case Command_MessageType_GETVERSION:
                GetVersion(command, command_response, request_context, userid);
                break;
            case Command_MessageType_GETNEXT:
                GetNext(command, command_response, response_value, request_context, userid);
                break;
            case Command_MessageType_GETPREVIOUS:
                GetPrevious(command, command_response, response_value, request_context, userid);
                break;
            case Command_MessageType_PUT:
                if ((command_response->status().code() != Command_Status_StatusCode_NO_SPACE) ||
                    ConnectionHandler::_batchSetCollection.isBatchCommand(&command)) {
                    if (ConnectionHandler::_batchSetCollection.isBatchCommand(&command)) {
                        BatchSet* batchSet = NULL;
                        batchSet =ConnectionHandler::_batchSetCollection.getBatchSet(command.header().batchid(), connection_id);
                        if (batchSet) {
                            bool bAdded = ConnectionHandler::_batchSetCollection.addCommand(reqResp.request(),
                                                               (Command*)&command, request_value, connection_id, *command_response);
                            if (bAdded) {
                                freeValue = false;
                                reqResp.SetRequestValue(NULL);
                            }
                            if (bAdded && batchSet->isGreaterMax()) {
                                LOG(ERROR) << "Exceed maximum allowable batch's size";
                                command_response->mutable_status()-> set_code(Command_Status_StatusCode_INVALID_BATCH);
                                command_response->mutable_status()-> set_statusmessage("Exceed maximum allowable batch's size");
                            }
                        } else { //Batchset was deleted because of illegal batch,...
                            command_response->mutable_status()-> set_code(Command_Status_StatusCode_INVALID_BATCH);
                            command_response->mutable_status()-> set_statusmessage("Batch cmd without a batch");
                            LOG(ERROR) << "1. Processed PUT, freeValue = " << freeValue;
                        }
                   } else {
                        Put(command, request_value, command_response, request_context, userid, connection_id);
                        if (command_response->status().code() == Command_Status_StatusCode_SUCCESS ||
                           (command_response->status().code() == Command_Status_StatusCode_INTERNAL_ERROR &&
                           command_response->status().statusmessage().find("synchronize") != string::npos)) { //NOLINT
                           freeValue = false;
                        }
                    }
                } else if (command_response->status().code() == Command_Status_StatusCode_NO_SPACE && !command.header().has_batchid()) {
                    freeValue = true;
                } else {
                    freeValue = false;
                }
                break;
            case Command_MessageType_DELETE:
                if (ConnectionHandler::_batchSetCollection.isBatchCommand(&command)) {
                    BatchSet* batchSet = NULL;
                    batchSet =ConnectionHandler::_batchSetCollection.getBatchSet(command.header().batchid(),
                                                           connection_id);
                    if (batchSet) {
                        bool bAdded = ConnectionHandler::_batchSetCollection.addCommand(reqResp.request(),
                            (Command*)&command, request_value, connection_id, *command_response);
                        if (bAdded && batchSet->isGreaterMax()) {
                            LOG(ERROR) << "Exceed maximum allowable number of batchable commands";
                            command_response->mutable_status()-> set_code(Command_Status_StatusCode_INVALID_BATCH);
                            command_response->mutable_status()-> set_statusmessage("Exceed maximum allowable number of batchable commands");
                        }
                   } else {
                        command_response->mutable_status()-> set_code(Command_Status_StatusCode_INVALID_BATCH);
                        command_response->mutable_status()-> set_statusmessage("Batch cmd without a batch");
                    }
                } else {
                    Delete(command, command_response, request_context, userid, connection_id);
                }
                break;
            case Command_MessageType_FLUSHALLDATA:
                Flush(command, command_response, request_context, userid);
                break;
            case Command_MessageType_SETUP:
                Setup(command, request_value, command_response, request_context, userid, corrupt);
                break;
            case Command_MessageType_SECURITY:
                Security(command, command_response, request_context, userid);
                break;
            case Command_MessageType_GETLOG:
                GetLog(command, command_response, response_value, request_context, userid, corrupt);
                break;
            case Command_MessageType_PEER2PEERPUSH:
               Peer2PeerPush(command, command_response, request_context, userid);
               break;
            case Command_MessageType_NOOP:
                command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_SUCCESS);
                break;
            case Command_MessageType_MEDIASCAN:
                MediaScan(command, request_value, command_response, response_value, request_context,
                time_handler, userid);
                if (time_handler->GetInterrupt()) {
                    freeValue = false;
                }
                break;
            case Command_MessageType_START_BATCH:
            {
                if (!authorizer_.AuthorizeGlobal(userid, Domain::kWrite | Domain::kDelete, request_context)) {
                    command_response->mutable_status()->set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
                    command_response->mutable_status()->set_statusmessage("Permission denied");
                    return;
                }
                break;
            }
            case Command_MessageType_END_BATCH:
                ProcessBatch(command, connection_id, *command_response, request_context, userid);
                break;
            case Command_MessageType_ABORT_BATCH:
            {
                if (!authorizer_.AuthorizeGlobal(userid, Domain::kWrite | Domain::kDelete, request_context)) {
                    command_response->mutable_status()->
                            set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
                    command_response->mutable_status()->
                            set_statusmessage("permission denied");
                    return;
                }
                BatchSet* batchSet =
                ConnectionHandler::_batchSetCollection.getBatchSet(command.header().batchid(),
                                                                                      connection_id);
                string batchSetId = batchSet->getId();
                ConnectionHandler::_batchSetCollection.deleteBatchSet(batchSetId);
                command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
                break;
             }
            case Command_MessageType_SET_POWER_LEVEL:
                SetPowerLevel(command, command_response, request_context, userid);
                break;
            default:
                Unimplemented(command, command_response);
                break;
        }
        if (freeValue) {
            if (request_value->size() > 0) {
                smr::DynamicMemory::getInstance()->deallocate(request_value->size());
            }
            request_value->FreeUserValue();
            request_value->SetBuffValueToNull();
        }
    }
}

void MessageProcessor::ProcessBatch(const Command& command, uint64_t connId, Command& response, RequestContext& request_context, uint64_t userid) {
    if (!authorizer_.AuthorizeGlobal(userid, Domain::kWrite | Domain::kDelete, request_context)) {
        response.mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        response.mutable_status()->
                set_statusmessage("permission denied");
        return;
    }
    BatchSet* batchSet =
         ConnectionHandler::_batchSetCollection.getBatchSet(command.header().batchid(),
                 connId);
    if (batchSet == NULL) {
        response.mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_BATCH);
        stringstream ss;
        ss << std::dec << "Batch id " << command.header().batchid()
           << " does not exist";
        response.mutable_status()->set_statusmessage(ss.str());
    } else {
        uint nBatchedCmds = command.body().batch().count();
        if (batchSet->getNumBatchedCmds() != nBatchedCmds) {
            response.mutable_status()->set_code(Command_Status_StatusCode_INVALID_BATCH);
            response.mutable_status()->
                    set_statusmessage("Number of batched commands mismatched");
        } else {
            std::tuple<uint64_t, int64_t> token {command.header().sequence(), connId};//NOLINT
            this->skinny_waist_.Write(batchSet, response, token, userid, request_context);
        }
    }
}
bool MessageProcessor::SetRecordStatus(const std::string& key, bool bad) {
    VLOG(1) << "Updating internal value record";
    return skinny_waist_.SetRecordStatus(key);
}

const std::string MessageProcessor::GetKey(const std::string& key, bool next) {
    return skinny_waist_.GetKey(key, next);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
