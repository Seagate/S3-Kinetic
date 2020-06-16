#include "getlog_handler.h"

#include "server.h"
#include "version_info.h"
#include "store_corrupt_state.h"
#include "smrdisk/DriveEnv.h"
#include "smrdisk/Disk.h"

#include "glog/logging.h"
#define JSON_IS_AMALGAMATION
#include "json/json.h"

using namespace com::seagate::kinetic::proto; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::OutgoingStringValue;

const std::regex GetLogHandler::LEVELDB_PATTERN("(leveldb.)(.*)");

const char GetLogHandler::KINETIC_DEVICE_GEN1_NAME[] = "com.Seagate.Kinetic.HDD.Gen1";

const char GetLogHandler::COMMAND_HISTORY[] = "command_history";

const char GetLogHandler::KEY_VALUE_HISTOGRAM[] = "key_value_histogram";

const char GetLogHandler::KEY_SIZE_HISTOGRAM[] = "transfer_key_size_histogram";

const char GetLogHandler::VALUE_SIZE_HISTOGRAM[] = "transfer_value_size_histogram";

const char GetLogHandler::STALE_DATA_REMOVAL_STATS[] = "stale_data_removal_stats";

const char GetLogHandler::F3_FIRMWARE_VERSION[] = "firmware_version";

const char GetLogHandler::UBOOT_VERSION[] = "uboot_version";

const char GetLogHandler::NETWORK_STATISTICS[] = "network_statistics";

const char GetLogHandler::OBJECT_ERROR_RATE[] = "object_error_rate";

const char GetLogHandler::ZONE_USAGE[] = "zone_usage";

const char GetLogHandler::FILL_DRIVE[] = "set_used_zones";

const char GetLogHandler::SUPERBLOCK[] = "superblock";

const char GetLogHandler::INSTALLER[] = "installer";
const char GetLogHandler::KSTART[] =  "kstart";

GetLogHandler::GetLogHandler(KeyValueStoreInterface& primary_data_store,
        DeviceInformationInterface& device_information,
        NetworkInterfaces& network_interfaces,
        const uint port,
        const uint tls_port,
        Limits& limits,
        StatisticsManager& statistics_manager) :
        primary_data_store_(primary_data_store),
        device_information_(device_information),
        network_interfaces_(network_interfaces),
        port_(port),
        tls_port_(tls_port),
        limits_(limits),
        statistics_manager_(statistics_manager) {}

bool GetLogHandler::SetUtilizations(Command *command_response) {
    bool success = true;
    Command_GetLog_Utilization* utilization;
    float hda_utilization;
    if (device_information_.GetHdaUtilization(&hda_utilization)) {
        utilization = command_response->mutable_body()->mutable_getlog()->
                add_utilizations();
        utilization->set_name("HDA");
        utilization->set_value(hda_utilization);
    } else {
        success = false;
    }

    std::vector<DeviceNetworkInterface> interfaces;
    if (!network_interfaces_.GetExternallyVisibleNetworkInterfaces(&interfaces)) {
        return false;
    }
    for (auto it = interfaces.begin(); it != interfaces.end(); ++it) {
        float en_utilization;
        utilization = command_response->mutable_body()->mutable_getlog()->
                add_utilizations();
        utilization->set_name(it->name);
        if (device_information_.GetEnUtilization(it->name, &en_utilization)) {
            utilization->set_value(en_utilization);
        } else {
            success = false;
            utilization->set_value(-1);
        }
    }

    float cpu_idle_percent;
    if (device_information_.GetCpuUtilization(&cpu_idle_percent)) {
        utilization = command_response->mutable_body()->mutable_getlog()->
                add_utilizations();
        utilization->set_name("CPU");
        utilization->set_value(1 - cpu_idle_percent);
    } else {
        success = false;
    }

    // Utilization of Connections
    float connection_percent;
    VLOG(3) << "NUMBER OF CONNECTIONS: "
            << Server::connection_map.TotalConnectionCount(); //open_connection_counter_.GetNumberOpenConnections();
    VLOG(3) << "MAX NUMBER OF CONNECTIONS: " << limits_.max_connections();

    // Calculate percentage
    connection_percent = float(statistics_manager_.GetNumberOpenConnections()) /
            float(limits_.max_connections());
    // Set response message appropriately
    utilization = command_response->mutable_body()->mutable_getlog()->
            add_utilizations();
    utilization->set_name("Connections");
    utilization->set_value(connection_percent);
    return success;
}

bool GetLogHandler::SetTemperatures(Command *command_response) {
    bool success = true;
    std::map<std::string, smart_values> smart_attributes;
    Command_GetLog_Temperature* temperature;
    float current, minimum, maximum, target;

    temperature = command_response->mutable_body()->mutable_getlog()->
            add_temperatures();
    if (device_information_.GetSMARTAttributes(&smart_attributes)) {
        temperature->set_name("HDA");
        temperature->set_current(
                smart_attributes[SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE].value);
        temperature->set_minimum(0);
        temperature->set_maximum(
                smart_attributes[SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE].worst);
        temperature->set_target(
                smart_attributes[SMARTLogProcessor::
                                 SMART_ATTRIBUTE_HDA_TEMPERATURE].threshold);
    } else {
        success = false;
    }

     if (device_information_.GetCpuTemp(&current, &minimum, &maximum, &target)) {
         temperature = command_response->mutable_body()->mutable_getlog()->
                 add_temperatures();
         temperature->set_name("CPU");
         if (minimum != -1) {
            temperature->set_current(current);
         }
         if (minimum != -1) {
            temperature->set_minimum(minimum);
         }
         if (maximum != -1) {
            temperature->set_maximum(maximum);
         }
         temperature->set_target(0);
     } else {
         success = false;
     }
  return success;
}

bool GetLogHandler::SetConfiguration(Command *command_response) {
    bool success = true;
    std::string drive_wwn;
    std::string drive_sn;
    std::string drive_vendor;
    std::string drive_model;
    std::vector<DeviceNetworkInterface> interfaces;
    command_response->mutable_body()->mutable_getlog()->
            mutable_configuration()->set_protocolversion(CURRENT_PROTOCOL_VERSION);
    command_response->mutable_body()->mutable_getlog()->
            mutable_configuration()->set_protocolcompilationdate(BUILD_DATE);
    command_response->mutable_body()->mutable_getlog()->
            mutable_configuration()->set_protocolsourcehash(KINETIC_PROTO_GIT_HASH);

    if (device_information_.GetDriveIdentification(&drive_wwn, &drive_sn,
            &drive_vendor, &drive_model) &&
            network_interfaces_.GetExternallyVisibleNetworkInterfaces(&interfaces)) {
        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_vendor(drive_vendor);
        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_model(drive_model);

        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_version(CURRENT_SEMANTIC_VERSION);
        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_compilationdate(BUILD_DATE);
        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_sourcehash(GIT_HASH);

        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_serialnumber(drive_sn);
        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_worldwidename(drive_wwn);

        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_port(port_);
        command_response->mutable_body()->mutable_getlog()->
                mutable_configuration()->set_tlsport(tls_port_);

        for (std::vector<DeviceNetworkInterface>::iterator it = interfaces.begin();
                it != interfaces.end();
                ++it) {
            Command_GetLog_Configuration_Interface* interface = command_response->
                    mutable_body()->mutable_getlog()->
                    mutable_configuration()->add_interface();
            interface->set_name(it->name);
            interface->set_mac(it->mac_address);
            interface->set_ipv4address(it->ipv4);
            interface->set_ipv6address(it->ipv6);
        }

    } else {
        success = false;
    }
    return success;
}

bool GetLogHandler::SetCapacities(Command *command_response) {
    bool success = true;
    float portionFull = -1;
    uint64_t norminalCapacity = device_information_.GetNominalCapacityInBytes();
    command_response->mutable_body()->mutable_getlog()->
            mutable_capacity()->set_nominalcapacityinbytes(norminalCapacity);
    if (device_information_.GetPortionFull(&portionFull)) {
        command_response->mutable_body()->mutable_getlog()->
                mutable_capacity()->set_portionfull(portionFull);
    } else {
        success = false;
    }
    return success;
}

bool GetLogHandler::SetStatistics(Command *command_response) {
    bool success = true;
    for (int message_type = Command_MessageType_MessageType_MIN;
            message_type <= Command_MessageType_MessageType_MAX;
            message_type++) {
        // There are some unused values between MessageType_MIN and MessageType_MAX
        // so to be sure of only including valid MessageTypes we need to explicitly
        // check each one
        if (Command_MessageType_IsValid((Command_MessageType)message_type)) {
            Command_GetLog_Statistics* statistics = command_response->
                    mutable_body()->mutable_getlog()->
                    add_statistics();
            statistics->set_messagetype((Command_MessageType)message_type);
            statistics->set_bytes(
                    statistics_manager_.GetByteCount(
                            (Command_MessageType)message_type));
            statistics->set_count(
                    statistics_manager_.GetOperationCount(
                            (Command_MessageType)message_type));
            statistics->set_maxlatency(
                uint64_t(statistics_manager_.GetLatencyForMessageType(message_type).count()));
        }
    }
    return success;
}

bool GetLogHandler::SetMessages(Command *command_response) {
    std::string logStr;
    LogRingBuffer::Instance()->toString(logStr);
    command_response->mutable_body()->mutable_getlog()->set_messages(logStr);
    return true;
}

bool GetLogHandler::SetLimits(Command *command_response) {
    bool success = true;
    proto::Command_GetLog_Limits* limits =
            command_response->mutable_body()->mutable_getlog()
            ->mutable_limits();

    limits->set_maxkeysize(limits_.max_key_size());
    limits->set_maxtagsize(limits_.max_tag_size());
    limits->set_maxversionsize(limits_.max_version_size());
    limits->set_maxvaluesize(limits_.max_value_size());
    limits->set_maxmessagesize(limits_.max_message_size());

    limits->set_maxconnections(limits_.max_connections());
    limits->set_maxoutstandingreadrequests(limits_.max_outstanding_read_requests());
    limits->set_maxoutstandingwriterequests(limits_.max_outstanding_write_requests());

    limits->set_maxkeyrangecount(limits_.max_key_range_count());
    limits->set_maxidentitycount(limits_.max_identity_count());

    limits->set_maxpinsize(limits_.max_pin_size());
    limits->set_maxbatchsize(limits_.max_batch_size());
    limits->set_maxdeletesperbatch(limits_.max_deletes_per_batch());
return success;
}

bool GetLogHandler::SetDevice(Command *command_response,
        NullableOutgoingValue *response_value,
        std::string device_name,
        bool corrupt) {
    bool success = true;
    bool unknown = false;
    std::map<std::string, smart_values> smart_attributes;
    std::map<string, NetworkPackets> interface_packet_information;
    ::std::string json;

    if (device_name == KINETIC_DEVICE_GEN1_NAME) {
        if (device_information_.GetSMARTAttributes(&smart_attributes)) {
            Json::Value root;
            int size_of_atrributes_interested = 7;
            for (int i = 0; i < size_of_atrributes_interested; i++) {
                Json::Value attr;
                std::string key = SMARTLogProcessor::ATTRIBUTES_INTERESTED[i];
                if (key != SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE) {
                    SetSMARTAttributeJSON(&attr, key, smart_attributes);
                    root[key].append(attr);
                }
            }

            Json::FastWriter writer;
            json = writer.write(root);
        } else {
            success = false;
        }
    } else if (std::regex_match(device_name, LEVELDB_PATTERN)) {
        if (corrupt) {
            command_response->mutable_status()->
                    set_statusmessage(std::string("Drive is in ") +
                                      std::string(StoreCorruptState::stateName));
            return false;
        }

        Json::Value property;
        std::string value;

        // Check if property is supported
        if (primary_data_store_.GetDBProperty(device_name, &value)) {
            property[device_name] = value;
            Json::FastWriter writer;
            json = writer.write(property);
            VLOG(1) << value;
        } else {
            // Not supported do not want to send anything back
            unknown = true;
        }
    } else if (device_name == COMMAND_HISTORY) {
        ::std::vector<LogRingCommandEntry> entries;
        LogRingBuffer::Instance()->copyCommandBuffer(entries);
        stringstream ss;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            std::string line;
            it->toString(line);
            ss << line;
        }
        command_response->mutable_body()->mutable_getlog()->set_messages(ss.str());
        return true;

    } else if (device_name == KEY_VALUE_HISTOGRAM) {
        ::std::vector<LogRingKeyValueEntry> entries;
        LogRingBuffer::Instance()->copyKeyValueBuffer(entries);
        stringstream ss;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            std::string line;
            it->toString(line);
            ss << line;
        }
        command_response->mutable_body()->mutable_getlog()->set_messages(ss.str());
        return true;

    } else if (device_name == KEY_SIZE_HISTOGRAM) {
        ::std::vector<LogRingKeySizeEntry> entries;
        LogRingBuffer::Instance()->copyKeySizeBuffer(entries);
        stringstream ss;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            std::string line;
            it->toString(line);
            ss << line;
        }
        command_response->mutable_body()->mutable_getlog()->set_messages(ss.str());
        return true;
    } else if (device_name == VALUE_SIZE_HISTOGRAM) {
        ::std::vector<LogRingValueSizeEntry> entries;
        LogRingBuffer::Instance()->copyValueSizeBuffer(entries);
        stringstream ss;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            std::string line;
            it->toString(line);
            ss << line;
        }
        command_response->mutable_body()->mutable_getlog()->set_messages(ss.str());
        return true;
    } else if (device_name == STALE_DATA_REMOVAL_STATS) {
        ::std::vector<LogRingStaleEntry> entries;
        LogRingBuffer::Instance()->copyStaleBuffer(entries);
        stringstream ss;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            std::string line;
            it->toString(line);
            ss << line;
        }
        command_response->mutable_body()->mutable_getlog()->set_messages(ss.str());
        return true;
    } else if (device_name == F3_FIRMWARE_VERSION) {
        std::string version;
        if (device_information_.GetF3Version(&version)) {
            json = version;
        } else {
            success = false;
        }
    } else if (device_name == UBOOT_VERSION) {
        std::string version;
        if (device_information_.GetUbootVersion(&version)) {
            json = version;
        } else {
            success = false;
        }
    } else if (device_name == NETWORK_STATISTICS) {
        if (device_information_.GetNetworkStatistics(&interface_packet_information)) {
            Json::Value root;
            for (auto it = interface_packet_information.begin();
                 it != interface_packet_information.end(); ++it) {
                Json::Value attr;
                attr["receive_packets"] = it->second.receive_packets;
                attr["receive_drop"] = it->second.receive_drop;
                attr["transmit_packets"] = it->second.transmit_packets;
                attr["transmit_drop"] = it->second.transmit_drop;
                root[it->first].append(attr);
            }

            Json::FastWriter writer;
            json = writer.write(root);
        } else {
            success = false;
        }
    } else if (device_name == OBJECT_ERROR_RATE) {
        Json::Value error_rates;

        // Iterate over set to report the failure count for the different message types
        std::set<Command_MessageType>::iterator it;
        for (it = message_type_to_report_.begin(); it != message_type_to_report_.end(); ++it) {
            Json::Value command_error_rate;
            int failed_operations = statistics_manager_.GetFailureCount(*it);
            command_error_rate["count"] = failed_operations;
            error_rates[Command_MessageType_Name(*it)].append(command_error_rate);
        }
        Json::FastWriter writer;
        json = writer.write(error_rates);
    } else if (device_name == ZONE_USAGE) {
        if (corrupt) {
            command_response->mutable_status()->
                    set_statusmessage(std::string("Drive is in ") +
                                      std::string(StoreCorruptState::stateName));
            return false;
        }

        std::string zone_usage;
        if (smr::DriveEnv::getInstance()->GetZoneUsage(zone_usage)) {
            json = zone_usage;
        } else {
            success = false;
        }
#ifdef KDEBUG
    } else if (device_name == FILL_DRIVE) {
        primary_data_store_.FillZoneMap();
#endif  //KDEBUG
    } else if (device_name.substr(0, sizeof(SUPERBLOCK) - 1) == SUPERBLOCK) { // minus 1 for null terminator
        string sRemain = device_name.substr(sizeof(SUPERBLOCK));
        size_t startIdx = sRemain.find_first_not_of(' ', 0);
        while (startIdx != string::npos) {
            size_t endIdx = sRemain.find(' ', startIdx);
            if (endIdx == string::npos) {
                endIdx = sRemain.size();
            }
            string arg = sRemain.substr(startIdx, endIdx - startIdx);
            if (arg  == "b0") {
                smr::Disk::superblockStatus[0] = false;
            } else if (arg == "b1") {
                smr::Disk::superblockStatus[1] = false;
            } else if (arg == "b2") {
                smr::Disk::superblockStatus[2] = false;
            } else if (arg == "g0") {
                smr::Disk::superblockStatus[0] = true;
            } else if (arg == "g1") {
                smr::Disk::superblockStatus[1] = true;
            } else if (arg == "g2") {
                smr::Disk::superblockStatus[2] = true;
            }
            sRemain = sRemain.substr(endIdx);
            startIdx = sRemain.find_first_not_of(' ', 0);
        }
    } else if (device_name == INSTALLER) {
        success = ReadInstallerLog(command_response);
    } else if (device_name == KSTART) {
        success = ReadKineticStartLog(command_response);
    } else {
        unknown = true;
    }

    // This condition will be met if the device name is unknown or if property is not supported
    if (unknown) {
        // Set repsonse value to an empty string
        json = "";
        LOG(WARNING) << "Received unknown device name " << device_name;
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_FOUND);
        command_response->mutable_status()->
                set_detailedmessage("Unknown device name");
    }
    response_value->set_value(new OutgoingStringValue(json));
    return success;
}

bool GetLogHandler::ReadKineticStartLog(Command* response) {
    bool success = true;
    int nCount = -1;
    string line;
    ifstream iStream(this->device_information_.GetKineticdStartLog());
    if (iStream.good() && getline(iStream, line)) {
        nCount = std::atoi(line.c_str());
    }
    iStream.close();
    stringstream ss;
    if (nCount == -1) {
        response->mutable_body()->mutable_getlog()->set_messages("");
        ss << "Kineticd start log not found";
        response->mutable_status()->set_code(Command_Status_StatusCode_NOT_FOUND);
        response->mutable_status()->set_statusmessage(ss.str());
        success = false;
    } else {
        ss << "Number of kineticd starts: " << nCount;
        response->mutable_body()->mutable_getlog()->set_messages(ss.str());
    }
    return success;
}

bool GetLogHandler::ReadInstallerLog(Command* response) {
     bool success = true;
     ifstream file("/mnt/util/installer.log");
     if (file.good()) {
         stringstream ss;
         string line;
         size_t nReads = 0;
         while (nReads < 0.9*limits_.max_message_size() && getline(file, line)) {
             nReads += line.size();
             ss << line;
         }
         file.close();
         response->mutable_body()->mutable_getlog()->set_messages(ss.str());
     } else {
         response->mutable_status()->set_code(Command_Status_StatusCode_NOT_FOUND);
         response->mutable_status()->set_statusmessage("Installer log is not found");
         success = false;
     }
     return success;
}

bool GetLogHandler::SetLogInfo(proto::Command *command_response,
        NullableOutgoingValue *response_value,
        std::string device_name,
        proto::Command_GetLog_Type type,
        bool corrupt) {
    bool success = true;
    switch (type) {
    case Command_GetLog_Type_UTILIZATIONS:
        if (!SetUtilizations(command_response)) {
            success = false;
        }
        break;
    case Command_GetLog_Type_TEMPERATURES:
        if (!SetTemperatures(command_response)) {
                        success = false;
        }
        break;
    case Command_GetLog_Type_CONFIGURATION:
        if (!SetConfiguration(command_response)) {
                        success = false;
        }
        break;
    case Command_GetLog_Type_CAPACITIES:
        if (corrupt) {
            success = false;
            command_response->mutable_status()->
                        set_statusmessage(std::string("Drive is in ") +
                                          std::string(StoreCorruptState::stateName));
        } else if (!SetCapacities(command_response)) {
                        success = false;
        }
        break;
    case Command_GetLog_Type_STATISTICS:
        if (!SetStatistics(command_response)) {
                        success = false;
        }
        break;
    case Command_GetLog_Type_MESSAGES:
        if (!SetMessages(command_response)) {
                        success = false;
        }
        break;
    case Command_GetLog_Type_LIMITS:
        if (!SetLimits(command_response)) {
                        success = false;
        }
        break;
    case Command_GetLog_Type_DEVICE:
        if (!SetDevice(command_response,
                response_value,
                device_name,
                corrupt)) {
                        success = false;
        }
        break;
    default:
        LOG(WARNING) << "Received unknown getlog type " << type;//NO_SPELL
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->
                set_detailedmessage("Unknown getlog type");
        break;
    }
    return success;
}

void GetLogHandler::ParseAndSetMessageTypes(std::string message_types) {
    std::string delimiter = ",";
    size_t pos = 0;
    size_t start = 0;

    // Parse through message_types string using ',' as a delimiter
    while ((pos = message_types.find(delimiter)) != std::string::npos) {
        // Get messagetype value
        int messagetype_value = atoi(message_types.substr(start, pos).c_str());
        // Cast messagetype value and insert to set
        Command_MessageType message_type = (Command_MessageType)messagetype_value;
        message_type_to_report_.insert(message_type);
        message_types = message_types.substr(pos+1, message_types.length());
    }
    // Need to insert last messagetype in the string
    Command_MessageType message_type =
        (Command_MessageType)atoi(message_types.c_str());
    message_type_to_report_.insert(message_type);
}

void GetLogHandler::ProcessRequest(const Command &command,
        Command *command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            uint64_t userid,
            bool corrupt) {
    if (!device_information_.Authorize(userid, request_context)) {
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
        command_response->mutable_status()->
                set_statusmessage("permission denied");
        return;
    }

    bool success = true;
    command_response->mutable_status()->
            set_code(Command_Status_StatusCode_SUCCESS);

    command_response->mutable_header()->
            set_messagetype(Command_MessageType_GETLOG_RESPONSE);

    for (int i = 0; i < command.body().getlog().types_size(); ++i) {
        Command_GetLog_Type type = command.body().getlog().types(i);

        command_response->mutable_body()->mutable_getlog()->add_types(type);

        if (!SetLogInfo(command_response,
                response_value,
                command.body().getlog().device().name(), type,
                corrupt)) {
            success = false;
        }
    }

    if (!success && command_response->status().code() == Command_Status_StatusCode_SUCCESS) {
        LOG(ERROR) << "IE failed to process request";//NO_SPELL
        command_response->mutable_status()->
                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
    }
}

void GetLogHandler::SetSMARTAttributeJSON(Json::Value* attribute, std::string key,
    std::map<std::string, smart_values> smart_attributes) {
    (*attribute)["value"] = smart_attributes[key].value;
    (*attribute)["worst"] = smart_attributes[key].worst;
    (*attribute)["threshold"] = smart_attributes[key].threshold;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
