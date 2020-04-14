#include "hibernate_state.h"
#include "ready_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "shutdown_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* HibernateState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;

    switch (event) {
        case StateEvent::STORE_INACCESSIBLE:
            nextState = new StoreInaccessibleState(this);
            break;
        case StateEvent::STORE_CORRUPT:
            nextState = new StoreCorruptState(this);
            break;
        case StateEvent::RESTORED:
            nextState = new ReadyState(this);
            break;
        case StateEvent::SHUTDOWN:
            nextState = new ShutdownState(this);
            break;
        default:
            break;
    }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
