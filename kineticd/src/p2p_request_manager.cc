#include "p2p_request_manager.h"
#include <memory>
#include <sstream>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::unique_ptr;
using std::shared_ptr;
using std::make_shared;
using std::chrono::high_resolution_clock;
using std::chrono::seconds;
using std::thread;

using ::kinetic::KineticConnectionFactory;

using proto::Command_Status_StatusCode;
using proto::Command_Status_StatusCode_IsValid;
using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;
using proto::Command_Status_StatusCode_REMOTE_CONNECTION_ERROR;
using proto::Command_Status_StatusCode_NOT_ATTEMPTED;
using proto::Command_Status_StatusCode_NESTED_OPERATION_ERRORS;
using proto::Command_Status_StatusCode_EXPIRED;
using proto::Command_Status_StatusCode_SERVICE_BUSY;
using proto::Command_P2POperation_Peer;

P2PRequestManager::P2PRequestManager(AuthorizerInterface &authorizer,
        UserStoreInterface &user_store,
        KineticConnectionFactory &kinetic_connection_factory,
        SkinnyWaistInterface &skinny_waist,
        size_t heuristic_memory_max_usage,
        size_t heuristic_memory_continue_threshold,
        size_t max_outstanding_request) :
            request_queue_(max_outstanding_request),
            p2p_operation_executor_(authorizer, user_store, kinetic_connection_factory,
                    skinny_waist, heuristic_memory_max_usage,
                    heuristic_memory_continue_threshold, &request_queue_) {}


void P2PRequestManager::ProcessRequest(const Command& command,
        Command* response_command,
        RequestContext& request_context,
        uint64_t userid) {
    Command_P2POperation const &p2pop = command.body().p2poperation();

    // Default the timeout to a large period, but respect user-specified timeout
    uint64_t seconds_timeout = command.header().timeout();
    if (seconds_timeout == 0) {
        seconds_timeout = 30;
    }

    // Calculate the execution deadline. If the P2P operation takes longer than this then kill
    // it.
    high_resolution_clock::time_point const deadline =
            high_resolution_clock::now() + seconds(seconds_timeout);

    auto p2p_request = make_shared<P2PRequest>(userid,
            deadline,
            p2pop,
            response_command,
            &request_context);

    if (request_queue_.Add(p2p_request)) {
        // We tie up this thread waiting for the operation to
        // be removed from the queue and completed
        p2p_request->WaitUntilCompleted();
    } else {
        // If the queue was full, we fail immediately
        VLOG(2) << "P2P queue full; rejecting request.";//NO_SPELL
        proto::Command_Status* status = response_command->mutable_status();
        status->set_code(Command_Status_StatusCode_SERVICE_BUSY);
        status->set_statusmessage("Too many outstanding P2P Requests. Try again later");
    }
}

void P2PRequestManager::Start() {
    p2p_operation_executor_thread_ = thread(
            &P2POperationExecutor::ProcessRequestQueue,
            &p2p_operation_executor_);
}

void P2PRequestManager::Stop() {
    if (Running()) {
        // Request queue sends interrupt to threads waiting on blocking calls
        LOG(INFO) << "Joining P2P request manager...";
        request_queue_.InterruptAll();
        p2p_operation_executor_thread_.join();
        LOG(INFO) << "P2P request manager joined.";
    }
}

bool P2PRequestManager::Running() {
    return p2p_operation_executor_thread_.joinable();
}

ThreadsafeBlockingQueue<shared_ptr<P2PRequest>>* P2PRequestManager::GetRequestQueue() {
    return &request_queue_;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
