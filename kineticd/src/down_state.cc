#include "down_state.h"
#include "locked_state.h"
#include "unlocked_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "restore_drive_state.h"
#include "ready_state.h"
#include "start_announcer_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* DownState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;
    switch (event) {
        case StateEvent::START_ANNOUNCER:
            nextState = new StartAnnouncerState(this);
            break;
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
        case StateEvent::RESTORED:
            nextState = new ReadyState(this);
            break;
        default:
            break;
        }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
