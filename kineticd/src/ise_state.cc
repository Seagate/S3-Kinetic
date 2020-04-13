#include "ise_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "ready_state.h"
#include "connection_queue.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* ISEState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;
    switch (event) {
        case StateEvent::ISED:
            if (success) {
                nextState = new RestoreDriveState(this);
            } else {
               nextState = prevState_;
               prevState_ = NULL;
            }
            break;
        case StateEvent::STORE_INACCESSIBLE:
            nextState = new StoreInaccessibleState(this);
            break;
        case StateEvent::STORE_CORRUPT:
            nextState = new StoreCorruptState(this);
            break;
        default:
            break;
    }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
