#ifndef KINETIC_LAUNCH_MONITOR_H_
#define KINETIC_LAUNCH_MONITOR_H_

#include "cautious_file_handler.h"
#include "gmock/gmock.h"

/*
 * A class for monitoring and mitigating launch failures
 */

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

enum class LaunchStep {
    NO_STOP = 0,
    LOCK_STATUS_QUERY = 1,
    UNLOCK,
    MOUNT_CHECK,
    INIT_USER_DATA_STORE,
    INIT_USERS
};

class LaunchMonitorInterface {
    public:
    virtual ~LaunchMonitorInterface() {}
    virtual bool OperationAllowed(LaunchStep requested_operation) = 0;
    virtual void OperationCompleted()=0;
    virtual void LoadMonitor()=0;
    virtual void SuccessfulLoad()=0;
    virtual bool DidStop()=0;
    virtual void WriteState(LaunchStep current_operation, bool bSuccess) = 0;
};

class LaunchMonitorPassthrough: public LaunchMonitorInterface {
    public:
    explicit LaunchMonitorPassthrough();
    ~LaunchMonitorPassthrough();
    bool OperationAllowed(LaunchStep requested_operation);
    void OperationCompleted();
    void LoadMonitor();
    void SuccessfulLoad();
    bool DidStop();
    void WriteState(LaunchStep current_operation, bool bSuccess) {
    }
};

class LaunchMonitor: public LaunchMonitorInterface {
    public:
    explicit LaunchMonitor(unique_ptr<CautiousFileHandlerInterface> file_handler);
    ~LaunchMonitor();

    void LoadMonitor();
    void SuccessfulLoad();
    bool OperationAllowed(LaunchStep requested_operation);
    void OperationCompleted();
    bool DidStop();
    void WriteState(LaunchStep current_operation, bool bSuccess);

    private:
    void InitClearCount();
    LaunchStep stop_point;
    bool stop_load;
    int fail_count;

    unique_ptr<CautiousFileHandlerInterface> file_handler_;
};

} //namespace kinetic
} //namespace seagate
} //namespace com

#endif  // KINETIC_LAUNCH_MONITOR_H_
