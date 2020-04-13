#ifndef KINETIC_MOCK_SERVER_H_
#define KINETIC_MOCK_SERVER_H_

#include "gmock/gmock.h"

#include "status_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockServer : public StatusInterface {
    public:
    MockServer() {}
    MOCK_METHOD3(IsSupportable, bool(const Message& msg, const Command& command, string& stateName));
    MOCK_METHOD3(IsSupportable, bool(Message_AuthType authType, const Command& command, string& stateName));
    MOCK_METHOD3(StateChanged, int(StateEvent event, bool success, void* data));
    MOCK_METHOD4(IsSupportable, bool(const Message_AuthType& authType, const Command_MessageType& cmdType,
                       string& stateName, bool lock));
    MOCK_METHOD4(StateChanged, int(StateEvent event, bool success, void* data, bool lock));
    MOCK_METHOD0(GetStateEnum, StateEnum());
    MOCK_METHOD0(GetStateName, std::string());
    MOCK_METHOD0(IsClusterSupportable, bool());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_SERVER_H_
