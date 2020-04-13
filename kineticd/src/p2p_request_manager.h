#ifndef KINETIC_P2P_REQUEST_MANAGER_H_
#define KINETIC_P2P_REQUEST_MANAGER_H_

#include <thread>

#include "kinetic/kinetic.h"
#include "kinetic/common.h"

#include "kinetic.pb.h"
#include "p2p_request.h"
#include "threadsafe_blocking_queue.h"
#include "p2p_operation_executor.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::shared_ptr;
using std::thread;

using ::kinetic::KineticConnectionFactory;
using proto::Message;
using proto::Command;
using proto::Command_P2POperation;
using proto::Command_P2POperation_Operation;

class P2PRequestManagerInterface {
    public:
    virtual ~P2PRequestManagerInterface() {}
    virtual void ProcessRequest(const Command& command,
            Command* command_response,
            RequestContext& request_context,
            uint64_t userid) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool Running() = 0;
};


class P2PRequestManager : public P2PRequestManagerInterface {
    public:
    explicit P2PRequestManager(
            AuthorizerInterface& authorizer,
            UserStoreInterface& user_store,
            KineticConnectionFactory& kinetic_connection_factory,
            SkinnyWaistInterface& skinny_waist,
            size_t heuristic_memory_max_usage,
            size_t heuristic_memory_continue_threshold,
            size_t max_outstanding_request);

    virtual void ProcessRequest(const Command& command,
            Command* command_response,
            RequestContext& request_context,
            uint64_t userid);
    virtual ~P2PRequestManager() {
    }
    virtual void Start();
    virtual void Stop();
    virtual bool Running();

    // Visible for testing only
    ThreadsafeBlockingQueue<shared_ptr<P2PRequest>>* GetRequestQueue();

    private:
    ThreadsafeBlockingQueue<shared_ptr<P2PRequest>> request_queue_;
    P2POperationExecutor p2p_operation_executor_;
    thread p2p_operation_executor_thread_;

    DISALLOW_COPY_AND_ASSIGN(P2PRequestManager);
};

class MockP2PRequestManager : public P2PRequestManagerInterface {
    public:
    MockP2PRequestManager() {}
    MOCK_METHOD4(ProcessRequest, void(const Command& command,
            Command* command_response,
    RequestContext& request_context,
    uint64_t userid));

    MOCK_METHOD0(Start, void());
    MOCK_METHOD0(Stop, void());
    MOCK_METHOD0(Running, bool());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_P2P_REQUEST_MANAGER_H_
