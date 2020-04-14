#include "locked_state.h"
#include "restore_drive_state.h"
#include "ise_state.h"
#include "download_state.h"
#include "unlocked_state.h"
#include "connection_queue.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* LockedState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;

    switch (event) {
        case StateEvent::LOCKED:
            if (!success) {
                nextState = prevState_;
                prevState_ = NULL;
            }
            break;
        case StateEvent::UNLOCK:
            nextState = new UnlockedState(this);
            break;
        case StateEvent::ISE:
            nextState = new ISEState(this);
            break;
        default:
            break;
    }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
