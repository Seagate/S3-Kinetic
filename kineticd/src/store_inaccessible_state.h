#ifndef KINETIC_STORE_INACCESSIBLE_STATE_H_
#define KINETIC_STORE_INACCESSIBLE_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class StoreInaccessibleState : public KineticState {
 public:
    explicit StoreInaccessibleState(KineticState* state) : KineticState(state) {
        name_ = "Store Inaccessible State";
        stateEnum_ = StateEnum::STORE_INACCESSIBLE;
        availAuths_.insert(proto::Message_AuthType_PINAUTH);
    }
    explicit StoreInaccessibleState(const StoreInaccessibleState& src) : KineticState(src) {}
    virtual ~StoreInaccessibleState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_STORE_INACCESSIBLE_STATE_H_
