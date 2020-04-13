#ifndef KINETIC_LOCKED_STATE_H_
#define KINETIC_LOCKED_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class LockedState : public KineticState {
 public:
    explicit LockedState(KineticState* state) : KineticState(state) {
        name_ = "Locked State";
        stateEnum_ = StateEnum::LOCKED;
        availAuths_.insert(proto::Message_AuthType_PINAUTH);
    }
    explicit LockedState(const LockedState& src) : KineticState(src) {}
    virtual ~LockedState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_LOCKED_STATE_H_
