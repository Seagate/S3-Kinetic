#ifndef KINETIC_DOWN_STATE_H_
#define KINETIC_DOWN_STATE_H_

#include "glog/logging.h"
#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class DownState : public KineticState {
 public:
    explicit DownState(KineticState* state) : KineticState(state) {
        name_ = "Down State";
        stateEnum_ = StateEnum::DOWN;
    }
    explicit DownState(Server* server) : KineticState(server) {
        name_ = "Down State";
        stateEnum_ = StateEnum::DOWN;
    }
    explicit DownState(const DownState& src) : KineticState(src) {}

    virtual ~DownState() {}

    virtual bool IsDownState() {
        return true;
    }

 public:
    KineticState* GetNextState(StateEvent, bool success = true);
    virtual bool ReadyForValidation() {
        return false;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DOWN_STATE_H_
