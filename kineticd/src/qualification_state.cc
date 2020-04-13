#include "qualification_state.h"
#include "store_corrupt_state.h"
#include "ready_state.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* QualificationState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;
    switch (event) {
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
