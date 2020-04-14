#ifndef KINETIC_HIBERNATE_STATE_H_
#define KINETIC_HIBERNATE_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class HibernateState : public KineticState {
 public:
    static constexpr const char* stateName = "Hibernate State";
    explicit HibernateState(KineticState* state) : KineticState(state) {
        name_ = std::string(this->stateName);
        stateEnum_ = StateEnum::HIBERNATE;
        availAuths_.insert(proto::Message_AuthType_HMACAUTH);
        availOps_.insert(proto::Command_MessageType_SET_POWER_LEVEL);
    }
    explicit HibernateState(const HibernateState& src) : KineticState(src) {}
    virtual ~HibernateState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_HIBERNATE_STATE_H_
