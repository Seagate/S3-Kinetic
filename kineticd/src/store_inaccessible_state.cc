#include "store_inaccessible_state.h"
#include "ise_state.h"
#include "download_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* StoreInaccessibleState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;

    switch (event) {
        case StateEvent::ISE:
            nextState = new ISEState(this);
            break;
        case StateEvent::DOWNLOAD:
            nextState = new DownloadState(this);
            break;
        default:
            break;
    }
    return nextState;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
