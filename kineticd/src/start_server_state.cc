#include "start_server_state.h"
#include "locked_state.h"
#include "unlocked_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "down_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* StartServerState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;
    switch (event) {
        case StateEvent::LOCKED:
            nextState = new LockedState(this);
            break;
        case StateEvent::UNLOCKED:
            nextState = new UnlockedState(this);
            break;
        case StateEvent::STORE_INACCESSIBLE:
            nextState = new StoreInaccessibleState(this);
            break;
        case StateEvent::STORE_CORRUPT:
            nextState = new StoreCorruptState(this);
            break;
        case StateEvent::DOWN:
            nextState = new DownState(this);
            break;
        default:
            break;
        }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
