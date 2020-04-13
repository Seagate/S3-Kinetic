#ifndef KINETIC_QUALIFICATION_STATE_H_
#define KINETIC_QUALIFICATION_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class QualificationState : public KineticState {
 public:
    explicit QualificationState(KineticState* state) : KineticState(state) {
        name_ = "Qualification State";
        stateEnum_ = StateEnum::QUALIFICATION;
        availAuths_.insert(proto::Message_AuthType_HMACAUTH);
        availOps_.insert(proto::Command_MessageType_GETLOG);
    }
    explicit QualificationState(const QualificationState& src) : KineticState(src) {}
    virtual ~QualificationState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_QUALIFICATION_STATE_H_
