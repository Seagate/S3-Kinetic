#ifndef KINETIC_P2P_REQUEST_H_
#define KINETIC_P2P_REQUEST_H_

#include <mutex>
#include <condition_variable>
#include <chrono>
#include "kinetic/kinetic.h"
#include "kinetic.pb.h"
#include "request_context.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command;
using proto::Command_P2POperation;

using std::unique_lock;
using std::mutex;
using std::condition_variable;
using std::chrono::high_resolution_clock;

class P2PRequest {
    public:
    P2PRequest(int64_t user_id,
            high_resolution_clock::time_point const deadline,
            const Command_P2POperation& p2pop,
            Command* response_command,
            RequestContext* request_context);

    void WaitUntilCompleted();

    // Prevent check-then-act situation by setting the in_progress_ flag
    // inside the same lock that checks if the request should be aborted.
    // Returns true if the request should be processed.
    // A return value of false means the caller should do nothing with
    // the request
    bool StartProcessingIfAllowed();
    void MarkCompletedAndNotify();

    int64_t GetUserId();
    high_resolution_clock::time_point const GetDeadline();
    const Command_P2POperation GetP2POp();
    Command* GetResponse();
    RequestContext* GetRequestContext();

    // Public for testing
    bool IsCompleted();
    bool IsInProgress();
    bool WasAborted();
    void SetAborted(bool aborted);

    private:
    int64_t user_id_;
    high_resolution_clock::time_point const deadline_;
    const Command_P2POperation p2pop_;
    Command* response_command_;
    RequestContext* request_context_;

    bool completed_;
    bool aborted_;
    bool in_progress_;
    mutex mutex_;
    condition_variable condition_variable_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_P2P_REQUEST_H_
