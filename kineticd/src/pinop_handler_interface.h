#ifndef KINETIC_PINOP_HANDLER_INTERFACE_H_
#define KINETIC_PINOP_HANDLER_INTERFACE_H_

#include <string.h>
#include "kinetic/incoming_value.h"

#include "kinetic.pb.h"
#include "request_context.h"
#include "connection_handler.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingValueInterface;

class PinOpHandlerInterface {
 public:
    virtual ~PinOpHandlerInterface() {}
    virtual void ProcessRequest(
        const proto::Command &command,
        IncomingValueInterface* request_value,
        proto::Command *command_response,
        RequestContext& request_context,
        const proto::Message_PINauth& pin_auth,
        Connection* connection) = 0;
    virtual void SetServer(Server* server) = 0;
    virtual void SetConnectionHandler(ConnectionHandler* connHandler) = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PINOP_HANDLER_INTERFACE_H_
