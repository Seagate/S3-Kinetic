#ifndef KINETIC_MESSAGE_PROCESSOR_H_
#define KINETIC_MESSAGE_PROCESSOR_H_

#include "kinetic/common.h"

#include "domain.h"
#include "kinetic.pb.h"
#include "message_processor_interface.h"
#include "profiler.h"
#include "skinny_waist.h"
#include "device_information_interface.h"
#include "statistics_manager.h"
#include "log_ring_buffer.h"
#include "cluster_version_store.h"
#include "p2p_request_manager.h"
#include "network_interfaces.h"
#include "getlog_handler.h"
#include "setup_handler.h"
#include "pinop_handler.h"
#include "power_manager.h"
#include "limits.h"
#include "request_context.h"
#include "security_manager.h"
#include "drive_info.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_MessageType;
using proto::Command_Security_ACL_Scope;
using com::seagate::kinetic::STATIC_DRIVE_INFO;

class MessageProcessor : public MessageProcessorInterface {
    public:
    explicit MessageProcessor(
        AuthorizerInterface& authorizer,
        SkinnyWaistInterface& skinny_waist,
        Profiler &profiler,
        ClusterVersionStoreInterface& cluster_version_store,
        const std::string& firmware_update_tmp_dir,
        uint32_t max_response_size_bytes,
        P2PRequestManagerInterface& p2p_request_manager,
        GetLogHandler& get_log_handler,
        SetupHandler& setup_handler,
        PinOpHandler& pinop_handler,
        PowerManager& power_manager,
        Limits& limits,
        STATIC_DRIVE_INFO static_drive_info,
        UserStoreInterface& user_store);
    ~MessageProcessor();
     void ProcessMessage(ConnectionRequestResponse& reqResp,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            int64_t connection_id,
            int connFd = -1,
            bool connIdMismatched = false,
            bool corrupt = false);
    void ProcessPinMessage(const proto::Command &command,
            IncomingValueInterface* request_value,
            proto::Command *command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            const proto::Message_PINauth& pin_auth,
            Connection* connection);
    bool SetRecordStatus(const std::string& key, bool bad = true);
    const std::string GetKey(const std::string& key, bool next);
    void SetClusterVersion(proto::Command *command_response);
    void SetLogInfo(proto::Command *command_response,
            NullableOutgoingValue *response_value,
            std::string device_name,
            proto::Command_GetLog_Type type);
    bool Flush(bool toSST);

    private:
    void ProcessBatch(const Command& command, uint64_t connId, Command& response, RequestContext& request_context, uint64_t userid);

    // Threadsafe; AuthorizerInterface implementations must be threadsafe
    AuthorizerInterface& authorizer_;

    // Threadsafe; SkinnyWaistInterface implementations must be threadsafe
    SkinnyWaistInterface& skinny_waist_;

    // Threadsafe; Profiler synchronizes access
    Profiler &profiler_;

    // Threadsafe; ClusterVersionStore synchronizes access
    ClusterVersionStoreInterface& cluster_version_store_;

    // Threadsafe; it's a const string
    const std::string firmware_update_tmp_dir_;

    // Threadsafe; it's a const unsigned int
    const uint32_t max_response_size_bytes_;

    // Threadsafe; P2PRequestManager is threadsafe
    P2PRequestManagerInterface& p2p_request_manager_;

    GetLogHandler& get_log_handler_;

    SetupHandler& setup_handler_;

    PinOpHandler& pinop_handler_;

    PowerManager& power_manager_;

    Limits& limits_;
    STATIC_DRIVE_INFO static_drive_info_;

    UserStoreInterface& user_store_;

    pthread_rwlock_t ise_rw_lock_;

    typedef enum {
        ReadDomainStatus_SUCCESS,
        ReadDomainStatus_INVALID_DOMAIN_OFFSET,
        ReadDomainStatus_INVALID_ROLE,
        ReadDomainStatus_NO_ROLE,
        ReadDomainStatus_UNREASONABLE_OFFSET_VALUE
    } ReadDomainStatus;

    uint32_t AlgorithmToUInt32(proto::Command_Algorithm algorithm);
    proto::Command_Algorithm UInt32ToAlgorithm(uint32_t algorithm);
    ReadDomainStatus ReadDomain(const Command_Security_ACL_Scope &pb_domain, Domain *domain);
    void Get(const proto::Command &command, proto::Command *command_response,
             NullableOutgoingValue *response_value, RequestContext& request_context,
             uint64_t userid);
    void GetVersion(const proto::Command &command, proto::Command *command_response,
                    RequestContext& request_context, uint64_t userid);
    void GetNext(const proto::Command &command, proto::Command *command_response,
                 NullableOutgoingValue *response_value, RequestContext& request_context,
                 uint64_t userid);
    void GetPrevious(const proto::Command &command, proto::Command *command_response,
                     NullableOutgoingValue *response_value, RequestContext& request_context,
                     uint64_t userid);
    void GetKeyRange(proto::Command &command, proto::Command *command_response,
                     RequestContext& request_context, uint64_t userid);
    void Put(const proto::Command &command, IncomingValueInterface* request_value,
             proto::Command *command_response, RequestContext& request_context,
             uint64_t userid, int64_t connection_id);
    void Delete(const proto::Command &command, proto::Command *command_response,
                RequestContext& request_context, uint64_t userid, int64_t connection_id);
    void Flush(const proto::Command &command, proto::Command *command_response, RequestContext& request_context, uint64_t userid);
    void Setup(const proto::Command &command, IncomingValueInterface* request_value,
               proto::Command *command_response, RequestContext& request_context,
               uint64_t userid, bool corrupt);
    void Security(const proto::Command &command, proto::Command *command_response,
                  RequestContext& request_context, uint64_t userid);
    void GetLog(const proto::Command &command,
                proto::Command *command_response,
                NullableOutgoingValue *response_value,
                RequestContext& request_context,
                uint64_t userid,
                bool corrupt);
    void Peer2PeerPush(const proto::Command &command, proto::Command *command_response,
                       RequestContext& request_context, uint64_t userid);
    void Unimplemented(const proto::Command &command, proto::Command *command_response);
    bool CheckPinStatusSuccess(proto::Command *command_response,
                               PinStatus status, string command, bool change);
    bool ValidateSecurity(const proto::Command &command, proto::Command *command_response);
    void MediaScan(const Command &command,
                   IncomingValueInterface* request_value,
                   Command *command_response,
                   NullableOutgoingValue *response_value,
                   RequestContext& request_context,
                   ConnectionTimeHandler* time_handler,
                   uint64_t userid);
    void SetPowerLevel(const Command &command,
                       Command *command_response,
                       RequestContext& request_context,
                       uint64_t userid);
    bool IsValidPinSize(proto::Command *command_response, string pin);
    void EvaluateProgressHistory();

    DISALLOW_COPY_AND_ASSIGN(MessageProcessor);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MESSAGE_PROCESSOR_H_
