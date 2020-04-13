#ifndef KINETIC_STATUS_INTERFACE_H_
#define KINETIC_STATUS_INTERFACE_H_

#include "kinetic_state.h"
#include "kinetic.pb.h"

using namespace com::seagate::kinetic::proto; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class StatusInterface {
    public:
    virtual ~StatusInterface() {}
    virtual bool IsSupportable(const Message& msg, const Command &command, string& stateName) = 0;
    virtual bool IsSupportable(Message_AuthType authType, const Command& command,
                               string& stateName) = 0;
    virtual bool IsSupportable(const Message_AuthType& authType, const Command_MessageType& cmdType,
                               string& stateName, bool lock = true) = 0;
    virtual int StateChanged(StateEvent event, bool success = true, void* data = NULL, bool lock = false) = 0;
    virtual StateEnum GetStateEnum() = 0;
    virtual std::string GetStateName() = 0;
    virtual bool IsClusterSupportable() = 0;
    virtual bool IsHmacSupportable() {
        return true;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_STATUS_INTERFACE_H_
