#ifndef KINETIC_MOCK_PINOP_HANDLER_H_
#define KINETIC_MOCK_PINOP_HANDLER_H_

#include "gmock/gmock.h"

#include "pinop_handler_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockPinOpHandler : public PinOpHandlerInterface {
    public:
    MockPinOpHandler() {}
    MOCK_METHOD6(ProcessRequest, void(const proto::Command &command,
            IncomingValueInterface* request_value,
            proto::Command *command_response,
            RequestContext& request_context,
            const proto::Message_PINauth& pin_auth,
            Connection* connection));
    MOCK_METHOD1(SetServer, void(Server* server));

    MOCK_METHOD1(SetConnectionHandler, void(ConnectionHandler* connHandler));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_PINOP_HANDLER_H_
