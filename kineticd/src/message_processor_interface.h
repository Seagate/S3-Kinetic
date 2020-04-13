#ifndef KINETIC_MESSAGE_PROCESSOR_INTERFACE_H_
#define KINETIC_MESSAGE_PROCESSOR_INTERFACE_H_

#include <string.h>
#include "kinetic/incoming_value.h"

#include "kinetic.pb.h"
#include "outgoing_value.h"
#include "request_context.h"
#include "connection_time_handler.h"
#include "connection.h"

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {
    class BatchSet;
}

using namespace com::seagate::kinetic::proto; //NOLINT
using ::kinetic::IncomingValueInterface;
using com::seagate::kinetic::cmd::BatchSet;

class MessageProcessorInterface {
 public:
    virtual ~MessageProcessorInterface() {}
    virtual void ProcessMessage(ConnectionRequestResponse& connection_request_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            int64_t connection_id,
            int connFd = -1,
            bool connIdMismatched = false,
            bool corrupt = false)=0;
    virtual void ProcessPinMessage(const proto::Command &command,
            IncomingValueInterface* request_value,
            proto::Command *command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            const proto::Message_PINauth& pin_auth, Connection* connection)=0;
    virtual bool SetRecordStatus(const std::string& key, bool bad = true) = 0;
    virtual const std::string GetKey(const std::string& key, bool next) = 0;
    virtual void SetClusterVersion(proto::Command *command_response) = 0;
    virtual void SetLogInfo(proto::Command *command_response,
            NullableOutgoingValue *response_value,
            std::string device_name,
            proto::Command_GetLog_Type type) = 0;
    virtual bool Flush(bool toSSt = false) = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MESSAGE_PROCESSOR_INTERFACE_H_
