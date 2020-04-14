#ifndef KINETIC_START_SERVER_STATE_H_
#define KINETIC_START_SERVER_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class StartServerState : public KineticState {
 public:
    explicit StartServerState(KineticState* state) : KineticState(state) {
        name_ = "Start Server State";
        stateEnum_ = StateEnum::START_SERVER;
    }
    explicit StartServerState(const StartServerState& src) : KineticState(src) {}
    virtual ~StartServerState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
    virtual bool ReadyForValidation() {
        return false;
    }
    bool IsStarted() {
        return true;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_START_SERVER_STATE_H_
