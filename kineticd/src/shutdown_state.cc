#include "shutdown_state.h"
#include "down_state.h"
#include "server.h"
#include "popen_wrapper.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* ShutdownState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;
    switch (event) {
        case StateEvent::READY_TO_SHUTDOWN:
            server_->CloseDB();
            if (ExecuteShutdown()) {
               nextState = new DownState(server_);
            } else {
               nextState = prevState_;
               prevState_ = NULL;
               server_->OpenDB();
            }
            break;
        default:
            break;
    }
    return nextState;
}

bool ShutdownState::ExecuteShutdown() {
    BlackholeLineProcessor processor;
    std::string shutdown_command = "poweroff";
    if (!execute_command(shutdown_command, processor)) {
        return false;
    } else {
        return true;
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
