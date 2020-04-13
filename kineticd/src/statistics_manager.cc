#include <map>
#include <string>
#include "glog/logging.h"

#include "statistics_manager.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_MessageType_IsValid;
using proto::Command_MessageType_Name;
using proto::Command_MessageType;

StatisticsManager::StatisticsManager() : unknown_operation_count_(0), open_connections_(0) {
    for (int message_type = Command_MessageType_MessageType_MIN;
        message_type <= Command_MessageType_MessageType_MAX + 1;
        message_type++) {
        uint64_t* counter_pointer = MessageTypeToCounterPointer((Command_MessageType)message_type, operation_counts_);
        *counter_pointer = 0;
        uint64_t* bytes_pointer = MessageTypeToCounterPointer((Command_MessageType)message_type, bytes_counts_);
        *bytes_pointer = 0;
        uint64_t* fail_pointer = MessageTypeToCounterPointer((Command_MessageType)message_type, operation_failure_counts_);
        *fail_pointer = 0;
        message_type_mapped_max_latency_.insert(make_pair((Command_MessageType)message_type, std::chrono::milliseconds(0)));
    }
}

//operation count methods
void StatisticsManager::IncrementOperationCount(Command_MessageType message_type, uint64_t delta) {
    __sync_add_and_fetch(MessageTypeToCounterPointer(message_type, operation_counts_), delta);
}

uint64_t StatisticsManager::GetOperationCount(Command_MessageType message_type) {
    uint64_t* counter_ptr = MessageTypeToCounterPointer((Command_MessageType)message_type, operation_counts_);
        return __sync_fetch_and_add(counter_ptr, 0);
}

void StatisticsManager::IncrementByteCount(Command_MessageType message_type, uint64_t delta) {
    __sync_add_and_fetch(MessageTypeToCounterPointer(message_type, bytes_counts_), delta);
}

uint64_t StatisticsManager::GetByteCount(Command_MessageType message_type) {
    uint64_t* bytes_ptr = MessageTypeToCounterPointer((Command_MessageType)message_type, bytes_counts_);
        return __sync_fetch_and_add(bytes_ptr, 0);
}

void StatisticsManager::IncrementFailureCount(Command_MessageType message_type, uint64_t delta) {
    __sync_add_and_fetch(MessageTypeToCounterPointer(message_type, operation_failure_counts_), delta);
}

uint64_t StatisticsManager::GetFailureCount(Command_MessageType message_type) {
    uint64_t* fail_ptr = MessageTypeToCounterPointer((Command_MessageType)message_type, operation_failure_counts_);
        return __sync_fetch_and_add(fail_ptr, 0);
}

std::map<std::string, uint64_t> StatisticsManager::GetCounts() {
    std::map<std::string, uint64_t> counts;

    for (int message_type = Command_MessageType_MessageType_MIN;
        message_type <= Command_MessageType_MessageType_MAX;
        message_type++) {
        std::string name = Command_MessageType_Name((Command_MessageType)message_type);
        counts[name] = GetOperationCount((Command_MessageType)message_type);
    }

    uint64_t unknown_count = __sync_fetch_and_add(&unknown_operation_count_, 0);
    counts["UNKNOWN"] = unknown_count;

    return counts;
}

uint64_t* StatisticsManager::MessageTypeToCounterPointer(Command_MessageType message_type, uint64_t* count_storage) {
    if (Command_MessageType_IsValid(message_type)) {
        return count_storage + message_type - Command_MessageType_MessageType_MIN;
    } else {
        return &unknown_operation_count_;
    }
}

//OpenConnections count methods
unsigned int StatisticsManager::IncrementOpenConnections() {
    return __sync_add_and_fetch(&open_connections_, 1);
}

unsigned int StatisticsManager::GetNumberOpenConnections() {
    return __sync_add_and_fetch(&open_connections_, 0);
}

unsigned int StatisticsManager::DecrementOpenConnections() {
    unsigned int count;
    count = __sync_add_and_fetch(&open_connections_, -1);
    return count;
}

// Maxlatency per messagetype methods
void StatisticsManager::SetMaxLatencyForMessageType(int msg_type, std::chrono::milliseconds new_max_response_time) {
    auto it = message_type_mapped_max_latency_.find((Command_MessageType)msg_type);
    if (it != message_type_mapped_max_latency_.end()) it->second = new_max_response_time;
}

std::chrono::milliseconds StatisticsManager::GetLatencyForMessageType(int msg_type) {
    auto it = message_type_mapped_max_latency_.find((Command_MessageType)msg_type);
    return it->second;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
