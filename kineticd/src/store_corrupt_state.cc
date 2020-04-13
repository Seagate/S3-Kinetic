#include "product_flags.h"
#include "store_corrupt_state.h"
#include "ise_state.h"
#include "download_state.h"
#include "locked_state.h"
#include "hibernate_state.h"
#include "shutdown_state.h"
#ifdef QUAL
#include "qualification_state.h"
#endif

namespace com {
namespace seagate {
namespace kinetic {

KineticState* StoreCorruptState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;

    switch (event) {
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
#ifdef QUAL
        case StateEvent::QUALIFICATION:
            nextState = new QualificationState(this);
            break;
#endif
        default:
            break;
    }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
