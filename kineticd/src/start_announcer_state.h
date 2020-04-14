#ifndef KINETIC_START_ANNOUNCER_STATE_H_
#define KINETIC_START_ANNOUNCER_STATE_H_

#include "kinetic_state.h"
#include "start_server_state.h"
#include "locked_state.h"
#include "unlocked_state.h"


namespace com {
namespace seagate {
namespace kinetic {

class StartAnnouncerState : public KineticState {
 public:
    explicit StartAnnouncerState(KineticState* state) : KineticState(state) {
        name_ = "Start Announcer State";
        stateEnum_ = StateEnum::START_ANNOUNCER;
    }
    explicit StartAnnouncerState(const StartAnnouncerState& src) : KineticState(src) {}
    virtual ~StartAnnouncerState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success) {
        KineticState* nextState = NULL;
        switch (event) {
        case StateEvent::STARTED:
            nextState = new StartServerState(this);
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
        default:
            break;
        }
        return nextState;
    }
    virtual bool ReadyForValidation() {
        return false;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_START_ANNOUNCER_STATE_H_
