#ifndef KINETIC_READY_STATE_H_
#define KINETIC_READY_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class ReadyState : public KineticState {
 public:
    explicit ReadyState(KineticState* state) : KineticState(state) {
        name_ = "Ready State";
        stateEnum_ = StateEnum::READY;
    }
    explicit ReadyState(const ReadyState& src) : KineticState(src) {}
    virtual ~ReadyState() {}

 public:
    virtual bool IsSupportable(proto::Message_AuthType authType, proto::Command_MessageType cmdType) {
        return true;
    }
    virtual bool IsSupportable(proto::Message_AuthType authType) {
        return true;
    }
    virtual bool IsClusterSupportable() {
        return true;
    }
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_READY_STATE_H_
