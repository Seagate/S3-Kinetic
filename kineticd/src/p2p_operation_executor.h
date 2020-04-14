#ifndef KINETIC_P2P_OPERATION_EXECUTOR_H_
#define KINETIC_P2P_OPERATION_EXECUTOR_H_

#include <thread>
#include <chrono>
#include "kinetic/kinetic.h"
#include "kinetic/common.h"

#include "authenticator_interface.h"
#include "domain.h"
#include "kinetic.pb.h"
#include "message_processor_interface.h"
#include "profiler.h"
#include "skinny_waist.h"
#include "device_information_interface.h"
#include "statistics_manager.h"
#include "log_ring_buffer.h"
#include "cluster_version_store.h"
#include "p2p_request.h"
#include "threadsafe_blocking_queue.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::shared_ptr;
using proto::Message;
using proto::Command_P2POperation;
using proto::Command_P2POperation_Operation;

using ::kinetic::P2PPushRequest;

/**
* Implementations must be threadsafe.
*/
class P2POperationExecutorInterface {
    public:
    virtual void Execute(int64_t user_id,
            std::chrono::high_resolution_clock::time_point deadline,
            const Command_P2POperation& p2pop,
            Command* response_command,
            RequestContext& request_context) = 0;

    virtual ~P2POperationExecutorInterface() {}
};

class P2POperationExecutor : public P2POperationExecutorInterface {
    public:
    explicit P2POperationExecutor(
            AuthorizerInterface& authorizer,
            UserStoreInterface& user_store,
            ::kinetic::KineticConnectionFactory& kinetic_connection_factory,
            SkinnyWaistInterface& skinny_waist,
            size_t heuristic_memory_max_usage,
            size_t heuristic_memory_continue_threshold,
            ThreadsafeBlockingQueue<std::shared_ptr<P2PRequest>>* request_queue);

    virtual void ProcessRequestQueue();

    virtual void Execute(int64_t user_id,
            std::chrono::high_resolution_clock::time_point deadline,
            const Command_P2POperation& p2pop,
            Command* response_command,
            RequestContext& request_context);

    // Visiblefor mocking
    virtual bool DoIO(int* outstanding_puts, int* outstanding_pushes,
            Command* response_command,
            std::chrono::high_resolution_clock::time_point deadline,
            ::kinetic::NonblockingKineticConnection& nonblocking_connection,
            bool abort_when_cache_is_below_threshold,
            size_t* heuristic_cache_size);

    private:
    // Virtual for testing
    virtual bool DoRun(::kinetic::NonblockingKineticConnection& nonblocking_connection,
                Command* response_command);

    // Virtual for testing
    virtual bool DoRun(::kinetic::NonblockingKineticConnection& nonblocking_connection,
                Command* response_command, fd_set* read_fds, fd_set* write_fds, int* num_fds);

    void SetOverallStatus(Command* response_command,
            proto::Command_Status_StatusCode code,
            char const* msg);

    size_t HeuristicPutOperationSize(
            const string& target_key,
            const std::shared_ptr<const ::kinetic::KineticRecord> record,
            const string& version);

    size_t HeuristicP2POperationSize(Command_P2POperation op);

    P2PPushRequest BuildP2PPushRequest(Command_P2POperation p2p_op);

    // Threadsafe; AuthorizerInterface implementations must be threadsafe
    AuthorizerInterface& authorizer_;

    // Threadsafe; UserStoreInterface implementations must be threadsafe
    UserStoreInterface& user_store_;

    // Threadsafe
    ::kinetic::KineticConnectionFactory& kinetic_connection_factory_;

    // Threadsafe; SkinnyWaistInterface implementations must be threadsafe
    SkinnyWaistInterface& skinny_waist_;

    size_t heuristic_memory_max_usage_;
    size_t heuristic_memory_continue_threshold_;

    ThreadsafeBlockingQueue<std::shared_ptr<P2PRequest>>* request_queue_;
    std::thread worker_thread_;

    DISALLOW_COPY_AND_ASSIGN(P2POperationExecutor);
};

class MockP2POperationExecutor : public P2POperationExecutorInterface {
    public:
    MockP2POperationExecutor() {}
    MOCK_METHOD5(Execute, void(int64_t,
    std::chrono::high_resolution_clock::time_point deadline,
    const Command_P2POperation& p2pop,
    Command* response_command,
    RequestContext& request_context));

    MOCK_METHOD0(StartWorkerThread, void());
};

class MockP2POperationExecutorInternal : public P2POperationExecutor {
    public:
    MockP2POperationExecutorInternal(AuthorizerInterface &authorizer,
            UserStoreInterface &user_store,
            ::kinetic::KineticConnectionFactory &kinetic_connection_factory,
            SkinnyWaistInterface &skinny_waist,
            size_t heuristic_memory_max_usage,
            size_t heuristic_memory_continue_threshold,
            ThreadsafeBlockingQueue<std::shared_ptr<P2PRequest>>* request_queue) :
        P2POperationExecutor(authorizer, user_store, kinetic_connection_factory,
                skinny_waist, heuristic_memory_max_usage, heuristic_memory_continue_threshold,
                request_queue)
    {}

    MOCK_METHOD6(DoIO, bool(int* outstanding_puts,
            Command* response_command,
            std::chrono::high_resolution_clock::time_point deadline,
            ::kinetic::NonblockingKineticConnection& nonblocking_connection,
            bool abort_when_cache_is_below_threshold,
            size_t* heuristic_cache_size));

    MOCK_METHOD2(DoRun, bool(::kinetic::NonblockingKineticConnection& nonblocking_connection,
            Command* response_command));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_P2P_OPERATION_EXECUTOR_H_
