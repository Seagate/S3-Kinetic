#ifndef KINETIC_STATISTICS_MANAGER_H_
#define KINETIC_STATISTICS_MANAGER_H_

#include <map>
#include <string>
#include <chrono>
#include "kinetic/common.h"

#include "kinetic.pb.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_MessageType;
using proto::Command_MessageType_MessageType_MAX;
using proto::Command_MessageType_MessageType_MIN;

/**
* Threadsafe class that can maintain a count per Command_MessageType. Invalid MessageTypes get
* counted in the unknown bucket.
*/
class StatisticsManager {
    public:
        StatisticsManager();
        void IncrementOperationCount(Command_MessageType message_type, uint64_t delta = 1);
        void IncrementByteCount(Command_MessageType message_type, uint64_t delta = 1);
        void IncrementFailureCount(Command_MessageType message_type, uint64_t delta = 1);
        std::map<std::string, uint64_t> GetCounts();
        uint64_t GetOperationCount(Command_MessageType message_type);
        uint64_t GetByteCount(Command_MessageType message_type);
        uint64_t GetFailureCount(Command_MessageType message_type);
        unsigned int IncrementOpenConnections();
        unsigned int GetNumberOpenConnections();
        unsigned int DecrementOpenConnections();
        void SetMaxLatencyForMessageType(int msg_type, std::chrono::milliseconds new_max_response_time);
        std::chrono::milliseconds GetLatencyForMessageType(int msg_type);

    private:
        uint64_t operation_counts_[Command_MessageType_MessageType_MAX -
            Command_MessageType_MessageType_MIN];
        uint64_t bytes_counts_[Command_MessageType_MessageType_MAX -
            Command_MessageType_MessageType_MIN];
        uint64_t operation_failure_counts_[Command_MessageType_MessageType_MAX -
            Command_MessageType_MessageType_MIN];
        uint64_t unknown_operation_count_;
        uint64_t* MessageTypeToCounterPointer(Command_MessageType message_type, uint64_t* count_storage);
        unsigned int open_connections_;
        std::map<Command_MessageType, std::chrono::milliseconds> message_type_mapped_max_latency_;
        DISALLOW_COPY_AND_ASSIGN(StatisticsManager);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_STATISTICS_MANAGER_H_
