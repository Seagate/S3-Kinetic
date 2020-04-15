#include "product_flags.h"
#include "ready_state.h"
#include "ise_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "download_state.h"
#include "locked_state.h"
#include "hibernate_state.h"
#include "shutdown_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* ReadyState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;

    switch (event) {
        case StateEvent::STORE_INACCESSIBLE:
            nextState = new StoreInaccessibleState(this);
            break;
        case StateEvent::STORE_CORRUPT:
            nextState = new StoreCorruptState(this);
            break;
        case StateEvent::ISE:
            nextState = new ISEState(this);
            break;
        case StateEvent::DOWNLOAD:
            nextState = new DownloadState(this);
            break;
        case StateEvent::LOCK:
            nextState = new LockedState(this);
            break;
        case StateEvent::HIBERNATE:
            nextState = new HibernateState(this);
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
