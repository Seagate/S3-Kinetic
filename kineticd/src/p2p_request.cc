#include "p2p_request.h"
#include "kinetic/kinetic.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::unique_lock;
using std::mutex;
using std::chrono::high_resolution_clock;

using proto::Command;
using proto::Command_P2POperation;
/////////////////////////////////////////////////////////
/// P2PRequest class, encapsulates all related items for a
/// p2p Command. The response command, actual P2P op from original
/// command, deadlines for timing p2pop. Also controls mutexes
/// and necessary thread management control structures for itself.
/// (does not control actual threads)
///
/// Instances of this class are used by the @p2p_request_manager and it's thread(s),
/// placed into the @ThreadsafeBlockingQueue and extracted via the
/// @p2p_operation_executor.
///
/// @constructor param[in] user_id
/// @constructor param[in] deadline -@request_manager derived deadline,executionTime > deadline:kill
/// @constructor param[in] p2pop -orig cmd body's p2pop (command.body().p2poperation())
/// @constructor param[in] response_command
/// @constructor param[in] request_context


P2PRequest::P2PRequest(int64_t user_id,
        high_resolution_clock::time_point const deadline,
        const Command_P2POperation& p2pop,
        Command* response_command,
        RequestContext* request_context) :
            user_id_(user_id),
            deadline_(deadline),
            p2pop_(p2pop),
            response_command_(response_command),
            request_context_(request_context),
            completed_(false),
            aborted_(false),
            in_progress_(false) {}

void P2PRequest::WaitUntilCompleted() {
    unique_lock<mutex> lock(mutex_);
    condition_variable_.wait_until(lock, deadline_, [&] {return completed_;});

    // If it's not completed, then it has timed out and must be aborted
    if (!completed_) {
        if (in_progress_) {
            // If the request is in progress, we need to let the Execute continue
            // until it observes the timeout. Otherwise our worker thread may
            // try to access objects (e.g. response) which no longer exist.
            // This may cause our request to slightly exceed the timeout, but
            // it is our best way to ensure everything is cleaned up
            condition_variable_.wait(lock, [&] {return completed_;});
        } else {
            // The processing hasn't started, so we mark this aborted to prevent any execution.
            aborted_ = true;
            auto status = response_command_->mutable_status();
            status->set_code(proto::Command_Status_StatusCode_NOT_ATTEMPTED);
            status->set_statusmessage(
                    "The request timed out before starting. No progress was made");
        }
    }
}

bool P2PRequest::StartProcessingIfAllowed() {
    unique_lock<mutex> lock(mutex_);
    // Don't allow work if this has already been aborted
    if (aborted_) {
        return false;
    }
    // Note that it is in progress
    in_progress_ = true;
    return true;
}

void P2PRequest::MarkCompletedAndNotify() {
    unique_lock<mutex> lock(mutex_);
    completed_ = true;
    in_progress_ = false;
    condition_variable_.notify_one();
}

int64_t P2PRequest::GetUserId() {
    return user_id_;
}

high_resolution_clock::time_point const P2PRequest::GetDeadline() {
    return deadline_;
}

const Command_P2POperation P2PRequest::GetP2POp() {
    return p2pop_;
}

Command* P2PRequest::GetResponse() {
    return response_command_;
}

RequestContext* P2PRequest::GetRequestContext() {
    return request_context_;
}

bool P2PRequest::IsCompleted() {
    return completed_;
}

bool P2PRequest::IsInProgress() {
    return in_progress_;
}

bool P2PRequest::WasAborted() {
    return aborted_;
}

void P2PRequest::SetAborted(bool aborted) {
    aborted_ = aborted;
}


} // namespace kinetic
} // namespace seagate
} // namespace com
