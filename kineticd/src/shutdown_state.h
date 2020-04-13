#ifndef KINETIC_SHUTDOWN_STATE_H_
#define KINETIC_SHUTDOWN_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class ShutdownState : public KineticState {
 public:
    explicit ShutdownState(KineticState* state) : KineticState(state) {
        name_ = "Shutdown State";
        stateEnum_ = StateEnum::SHUTDOWN;
    }
    explicit ShutdownState(const ShutdownState& src) : KineticState(src) {}
    virtual ~ShutdownState() {
        delete (string*)data_;
    }

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);

 private:
    bool ExecuteShutdown();
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SHUTDOWN_STATE_H_
