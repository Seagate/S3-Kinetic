#ifndef KINETIC_GETLOG_HANDLER_H_
#define KINETIC_GETLOG_HANDLER_H_

#include <regex>

#include "kinetic.pb.h"

#include "device_information.h"
#include "network_interfaces.h"
#include "log_ring_buffer.h"
#include "statistics_manager.h"
#include "limits.h"
#include "key_value_store.h"

#ifdef QUAL
#include "qualification_handler.h"
#endif

#include "outgoing_value.h"
#define JSON_IS_AMALGAMATION
#include "json/json.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Message;
using proto::Command;

class GetLogHandler {
    public:
    static const std::regex LEVELDB_PATTERN;
#ifdef QUAL
    static const std::regex QUALIFICATION_PATTERN;
#endif
    static const char KINETIC_DEVICE_GEN1_NAME[];
    static const char COMMAND_HISTORY[];
    static const char KEY_VALUE_HISTOGRAM[];
    static const char KEY_SIZE_HISTOGRAM[];
    static const char VALUE_SIZE_HISTOGRAM[];
    static const char STALE_DATA_REMOVAL_STATS[];
    static const char F3_FIRMWARE_VERSION[];
    static const char UBOOT_VERSION[];
    static const char NETWORK_STATISTICS[];
    static const char OBJECT_ERROR_RATE[];
    static const char FILL_DRIVE[];
    static const char ZONE_USAGE[];
    static const char SUPERBLOCK[];
    static const char INSTALLER[];
    static const char KSTART[];

    explicit GetLogHandler(KeyValueStoreInterface& primary_data_store,
            DeviceInformationInterface& device_information,
            NetworkInterfaces& network_interfaces,
            const uint32_t port,
            const uint32_t tls_port,
            Limits& limits,
            StatisticsManager& statistics_manager);
    void ProcessRequest(const Command& command,
            Command* command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            uint64_t userid,
            bool corrupt = false);
    bool SetTemperatures(Command *command_response);
    bool SetUtilizations(Command *command_response);
    bool SetConfiguration(Command *command_response);
    bool SetCapacities(Command *command_response);
    bool SetStatistics(Command *command_response);
    bool SetMessages(Command *command_response);
    bool SetLimits(Command *command_response);
    bool SetDevice(Command *command_response,
            NullableOutgoingValue *response_value,
            std::string device_name,
            bool corrupt = false);
    bool SetLogInfo(proto::Command *command_response,
            NullableOutgoingValue *response_value,
            std::string device_name,
            proto::Command_GetLog_Type type,
            bool corrupt = false);
    void ParseAndSetMessageTypes(std::string message_types);
#ifdef QUAL
    void SetServer(Server* server) {
        qualification_handler_.SetServer(server);
    }
#endif

    private:
    void SetSMARTAttributeJSON(Json::Value* attribute,
            std::string key,
            std::map<std::string, smart_values> smart_attributes);
    bool ReadInstallerLog(Command* response);
    bool ReadKineticStartLog(Command* response);

    KeyValueStoreInterface& primary_data_store_;

    // Threadsafe; DeviceInformationInterface is threadsafe
    DeviceInformationInterface& device_information_;

    // Threadsafe; No state
    NetworkInterfaces& network_interfaces_;

    const int32_t port_;

    const int32_t tls_port_;

    // Threadsafe; immutable
    Limits& limits_;

#ifdef QUAL
    QualificationHandler qualification_handler_;
#endif

    //common pointer for operation_counter, open_connections and maxlatency by optype methods
    StatisticsManager& statistics_manager_;

    // Array of message types whose failure count will be reported
    std::set<Command_MessageType> message_type_to_report_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_GETLOG_HANDLER_H_
