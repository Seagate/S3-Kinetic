#include "restore_drive_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "ready_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* RestoreDriveState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;

    switch (event) {
        case StateEvent::STORE_INACCESSIBLE:
            nextState = new StoreInaccessibleState(this);
            break;
        case StateEvent::STORE_CORRUPT:
            nextState = new StoreCorruptState(this);
            break;
        case StateEvent::RESTORED:
            if (success) {
                nextState = new ReadyState(this);
            } else {
                nextState = new StoreInaccessibleState(this);
            }
            break;

        default:
            break;
    }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
