#ifndef KINETIC_MOCK_MESSAGE_PROCESSOR_H_
#define KINETIC_MOCK_MESSAGE_PROCESSOR_H_

#include "gmock/gmock.h"

#include "message_processor_interface.h"
#include "connection.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockMessageProcessor : public MessageProcessorInterface {
    public:
    MockMessageProcessor() {}
    MOCK_METHOD7(ProcessMessage, void(ConnectionRequestResponse& reqResp,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            int64_t connection_id,
            int connFd,
            bool connIdMismatched,
            bool corrupt));

    MOCK_METHOD2(SetRecordStatus, bool(const std::string& key, bool bad));
    MOCK_METHOD2(GetKey, const std::string(const std::string& key, bool next));
    MOCK_METHOD1(CmdTimeOutResponse, void(proto::Message *response));
    MOCK_METHOD1(InvalidValueSize, void(proto::Message *response));
    MOCK_METHOD7(ProcessPinMessage, void(const proto::Command &command,
            IncomingValueInterface* request_value,
            proto::Command *command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            const proto::Message_PINauth& pin_auth,
            Connection* connection));
    MOCK_METHOD1(SetClusterVersion, void(proto::Command *command_response));
    MOCK_METHOD4(SetLogInfo, void(proto::Command *command_response,
            NullableOutgoingValue *response_value,
            std::string device_name,
            proto::Command_GetLog_Type type));
    MOCK_METHOD1(Flush, bool(bool toSST));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_MESSAGE_PROCESSOR_H_
