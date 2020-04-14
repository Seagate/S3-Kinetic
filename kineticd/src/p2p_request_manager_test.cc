#include "gtest/gtest.h"
#include "glog/logging.h"

#include "kinetic/kinetic.h"

#include "p2p_request_manager.h"
#include "mock_user_store.h"
#include "mock_authorizer.h"
#include "mock_skinny_waist.h"
#include "threadsafe_blocking_queue.h"
#include "p2p_request.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::DoAll;
using ::testing::WithArgs;

using ::std::string;
using ::std::shared_ptr;
using ::std::make_shared;
using ::std::unique_ptr;

using proto::Command_Status_StatusCode_SERVICE_BUSY;

static const int QUEUE_SIZE = 2;

class MockKineticConnectionFactory : public ::kinetic::KineticConnectionFactory {
    public:
    MockKineticConnectionFactory() :
    ::kinetic::KineticConnectionFactory(::kinetic::HmacProvider()) {}

    MOCK_METHOD2(NewNonblockingConnection, ::kinetic::Status(
            const ::kinetic::ConnectionOptions& options,
            unique_ptr< ::kinetic::NonblockingKineticConnection>& connection));
};

class P2PRequestManagerTest : public ::testing::Test {
    protected:
    P2PRequestManagerTest():
            mock_authorizer_(),
            mock_user_store_(),
            mock_skinny_waist_(),
            p2p_request_manager_(mock_authorizer_,
                    mock_user_store_,
                    mock_connection_factory_,
                    mock_skinny_waist_,
                    100,
                    75,
                    QUEUE_SIZE) {
        request_queue_ = p2p_request_manager_.GetRequestQueue();
    }

    MockAuthorizer mock_authorizer_;
    MockUserStore mock_user_store_;
    MockSkinnyWaist mock_skinny_waist_;
    MockKineticConnectionFactory mock_connection_factory_;
    ThreadsafeBlockingQueue<std::shared_ptr<P2PRequest>>* request_queue_;
    P2PRequestManager p2p_request_manager_;

    std::thread DoCompletionThread(std::shared_ptr<P2PRequest>& request) {
        std::thread completion_thread([&] {
            // Wait for one item
            request_queue_->BlockingRemove(request);
            request->MarkCompletedAndNotify();
        });
        return completion_thread;
    }
};
/*
TEST_F(P2PRequestManagerTest, ProcessRequestEnqueuesRequestWithDefaultTimeout) {
    Message message;
    message.mutable_command()->mutable_header()->set_identity(100);

    Message response;
    RequestContext request_context;
    // ProcessRequest waits until the item is completed, so we can't just assert
    // that the queue size increased, we have to have another thread read from the queue
    std::shared_ptr<P2PRequest> request;
    std::thread t = DoCompletionThread(request);
    p2p_request_manager_.ProcessRequest(message, &response, request_context);

    // Let the thread finish
    t.join();
    // If the thread completed, then an item was added to the queue.

    auto actual_delta = std::chrono::duration_cast<std::chrono::seconds>(
        request->GetDeadline() - std::chrono::high_resolution_clock::now());

    // Hard to assert on time exactly, so let's just make sure it's close and that
    // it did use the expected default
    ASSERT_LT((30 - actual_delta.count()), 2);
}

TEST_F(P2PRequestManagerTest, ProcessRequestEnqueuesRequestWithSepcifiedTimeout) {
    Message message;
    message.mutable_command()->mutable_header()->set_identity(100);
    message.mutable_command()->mutable_header()->set_timeout(10);

    Message response;
    RequestContext request_context;
    // ProcessRequest waits until the item is completed, so we can't just assert
    // that the queue size increased, we have to have another thread read from the queue
    std::shared_ptr<P2PRequest> request;
    std::thread t = DoCompletionThread(request);
    p2p_request_manager_.ProcessRequest(message, &response, request_context);

    // Let the thread finish
    t.join();
    // If the thread completed, then an item was added to the queue.

    auto actual_delta = std::chrono::duration_cast<std::chrono::seconds>(
            request->GetDeadline() - std::chrono::high_resolution_clock::now());

    // Hard to assert on time exactly, so let's just make sure it's close and that
    // it did not use the default
    ASSERT_LT((10 - actual_delta.count()), 2);
}

TEST_F(P2PRequestManagerTest, WaitsForP2PRequestCompletion) {
    Message message;
    message.mutable_command()->mutable_header()->set_identity(100);

    Message response;
    RequestContext request_context;
    // ProcessRequest waits until the item is completed, so we can't just assert
    // that the queue size increased, we have to have another thread read from the queue
    std::shared_ptr<P2PRequest> request;
    std::thread t = DoCompletionThread(request);
    p2p_request_manager_.ProcessRequest(message, &response, request_context);

    // Let the thread finish
    t.join();
    // If the thread completed, then an item was added to the queue.
}

TEST_F(P2PRequestManagerTest, ReturnsServiceBusyWhenQueueFull) {
    for (int i = 0; i < QUEUE_SIZE; ++i) {
        request_queue_->Add(nullptr);
    }
    Message message;
    message.mutable_command()->mutable_header()->set_identity(100);

    Message response;
    RequestContext request_context;
    p2p_request_manager_.ProcessRequest(message, &response, request_context);

    ASSERT_EQ(Command_Status_StatusCode_SERVICE_BUSY, response.command().status().code());
}
*/
} // namespace kinetic
} // namespace seagate
} // namespace com
