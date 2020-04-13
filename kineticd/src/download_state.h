#ifndef KINETIC_DOWNLOAD_STATE_H_
#define KINETIC_DOWNLOAD_STATE_H_

#include <queue>

#include "kinetic_state.h"
#include "ready_state.h"

namespace com {
namespace seagate {
namespace kinetic {

class DownloadState : public KineticState {
 public:
    explicit DownloadState(KineticState* state) : KineticState(state) {
        name_ = "Download State";
        stateEnum_ = StateEnum::DOWNLOAD;
    }
    explicit DownloadState(const DownloadState& src) : KineticState(src) {}
    virtual ~DownloadState() {
        delete static_cast<std::queue<std::string>*>(data_);
    }

 public:
    KineticState* GetNextState(StateEvent event, bool success = true);

 private:
    bool ExecuteFirmware();
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DOWNLOAD_STATE_H_
