#ifndef KINETIC_UNLOCKED_STATE_H_
#define KINETIC_UNLOCKED_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class UnlockedState : public KineticState {
 public:
    explicit UnlockedState(KineticState* state) : KineticState(state) {
        name_ = "Unlocked State";
        stateEnum_ = StateEnum::UNLOCKED;
    }
    explicit UnlockedState(const UnlockedState& src) : KineticState(src) {}
    virtual ~UnlockedState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool successi = true);
    virtual bool ReadyForValidation() {
        return false;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_UNLOCKED_STATE_H_
