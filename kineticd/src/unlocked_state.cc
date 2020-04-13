#include "unlocked_state.h"
#include "restore_drive_state.h"
#include "store_inaccessible_state.h"
#include "store_corrupt_state.h"
#include "locked_state.h"
#include "ise_state.h"
#include "ready_state.h"
#include "connection_queue.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* UnlockedState::GetNextState(StateEvent event, bool success) {
            KineticState* nextState = NULL;
            switch (event) {
               case StateEvent::UNLOCKED:
                   if (success) {
                       nextState = new RestoreDriveState(this);
                   } else {
                       nextState = prevState_;
                       prevState_ = NULL;
                   }
                   break;
               case StateEvent::RESTORED:
                   nextState = new ReadyState(this);
                   break;
               case StateEvent::STORE_INACCESSIBLE:
                   nextState = new StoreInaccessibleState(this);
                   break;
               case StateEvent::STORE_CORRUPT:
                   nextState = new StoreCorruptState(this);
                   break;
               case StateEvent::LOCK:
                   nextState = new LockedState(this);
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

