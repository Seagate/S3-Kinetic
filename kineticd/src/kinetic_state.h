#ifndef KINETIC_KINETIC_STATE_H_
#define KINETIC_KINETIC_STATE_H_

#include <unordered_set>
#include "kinetic.pb.h"
#include "glog/logging.h"

using std::string;

namespace com {
namespace seagate {
namespace kinetic {

using com::seagate::kinetic::proto::Command_MessageType_SETUP;

class Server;

enum class StateEvent {
    DOWN,
    START_ANNOUNCER,
    START,
    STARTED,
    ISE,
    ISED,
    LOCK,
    LOCKED,
    UNLOCK,
    UNLOCKED,
    RESTORE,
    RESTORED,
    STORE_CORRUPT,
    STORE_INACCESSIBLE,
    DOWNLOAD,
    DOWNLOADED,
    HIBERNATE,
    SHUTDOWN,
    QUALIFICATION,
    READY_TO_SHUTDOWN
};

enum class StateEnum {
    LOCKED,
    UNLOCKED,
    READY,
    RESTORE_DRIVE,
    START_ANNOUNCER,
    START_SERVER,
    STORE_INACCESSIBLE,
    STORE_CORRUPT,
    QUALIFICATION,
    ISE,
    DOWNLOAD,
    DOWN,
    HIBERNATE,
    SHUTDOWN
};

class KineticState {
    friend class Server;

 protected:
    explicit KineticState(KineticState* state) {
        server_ = state->server_;
        prevState_ = NULL;
        data_ = NULL;
    }

    explicit KineticState(Server* server) {
        server_ = server;
        prevState_ = NULL;
        data_ = NULL;
    }

    explicit KineticState(const KineticState& src) {
        server_ = src.server_;
        name_ = src.name_;
        availOps_ = src.availOps_;
        prevState_ = NULL;
        data_ = NULL;
    }

 public:
    virtual string GetName() {
        return name_;
    }
    virtual StateEnum GetEnum() {
        return stateEnum_;
    }

 protected:
    virtual ~KineticState() {
       delete prevState_;
    }
    virtual KineticState* GetNextState(StateEvent event, bool success = true) = 0;
    virtual bool IsSupportable(proto::Message_AuthType authType, proto::Command_MessageType cmdType) {
        if (availAuths_.find(authType) == availAuths_.end()) {
            return false;
        } else if (authType == proto::Message_AuthType_PINAUTH) {
            return true;
        }
        return (availOps_.find(cmdType) != availOps_.end());
    }
    virtual bool IsSupportable(proto::Message_AuthType authType) {
        if (availAuths_.find(authType) == availAuths_.end()) {
            return false;
        } else {
            return true;
        }
    }
    virtual bool IsClusterSupportable() {
        return false;
    }
    virtual bool ReadyForValidation() {
        return true;
    }
    void DelPrevState() {
        delete prevState_;
        prevState_ = NULL;
    }
    void SetPrevState(KineticState* state) {
        prevState_ = state;
    }
    void SetData(void* data) {
        data_ = data;
    }
    virtual bool IsDownState() {
        return false;
    }
    virtual bool IsStarted() {
        return false;
    }

 protected:
    Server* server_;
    string name_;
    StateEnum stateEnum_;
    std::unordered_set<int> availOps_;
    std::unordered_set<int> availAuths_;
    KineticState* prevState_;
    void* data_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_KINETIC_STATE_H_
