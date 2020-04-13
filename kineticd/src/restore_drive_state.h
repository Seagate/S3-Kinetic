#ifndef KINETIC_RESTORE_DRIVE_STATE_H_
#define KINETIC_RESTORE_DRIVE_STATE_H_

#include "kinetic_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class RestoreDriveState : public KineticState {
 public:
    explicit RestoreDriveState(KineticState* state) : KineticState(state) {
        name_ = "Restore Drive State";
        stateEnum_ = StateEnum::RESTORE_DRIVE;
    }
    explicit RestoreDriveState(const RestoreDriveState& src) : KineticState(src) {}
    virtual ~RestoreDriveState() {}

 public:
    virtual bool IsSupportable(proto::Message_AuthType authType, proto::Command_MessageType cmdType) {
        return false;
    }
    KineticState* GetNextState(StateEvent event, bool success = true);
    virtual bool ReadyForValidation() {
        return false;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_RESTORE_DRIVE_STATE_H_
