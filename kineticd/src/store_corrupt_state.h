#ifndef KINETIC_STORE_CORRUPT_STATE_H_
#define KINETIC_STORE_CORRUPT_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class StoreCorruptState : public KineticState {
 public:
    static constexpr const char* stateName = "Store Corrupt State";
    explicit StoreCorruptState(KineticState* state) : KineticState(state) {
        name_ = std::string(this->stateName);
        stateEnum_ = StateEnum::STORE_CORRUPT;
        availAuths_.insert(proto::Message_AuthType_PINAUTH);
        availAuths_.insert(proto::Message_AuthType_HMACAUTH);
        availOps_.insert(proto::Command_MessageType_SETUP);
        availOps_.insert(proto::Command_MessageType_SECURITY);
        availOps_.insert(proto::Command_MessageType_GETLOG);
        availOps_.insert(proto::Command_MessageType_SET_POWER_LEVEL);
    }
    explicit StoreCorruptState(const StoreCorruptState& src) : KineticState(src) {}
    virtual ~StoreCorruptState() {}

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_STORE_CORRUPT_STATE_H_
