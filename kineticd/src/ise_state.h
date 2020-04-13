#ifndef KINETIC_ISE_STATE_H_
#define KINETIC_ISE_STATE_H_

#include "kinetic_state.h"
#include "restore_drive_state.h"
#include "store_inaccessible_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class ISEState : public KineticState {
 public:
    explicit ISEState(KineticState* state) : KineticState(state) {
        name_ = "ISE State";
        stateEnum_ = StateEnum::ISE;
    }
    explicit ISEState(const ISEState& src) : KineticState(src) {}
    virtual ~ISEState() {}
    virtual bool ReadyForValidation() {
        return false;
    }

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_ISE_STATE_H_
