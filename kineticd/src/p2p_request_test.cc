#include <chrono>
#include <thread>
#include "gtest/gtest.h"
#include "kinetic/kinetic.h"
#include "p2p_request.h"

#include <iostream>
namespace com {
namespace seagate {
namespace kinetic {

using proto::Message;
using proto::Command_P2POperation;

class P2PRequestTest : public ::testing::Test {
};
/*
TEST_F(P2PRequestTest, WaitUntilCompletedAbortsAfterDeadline) {
    std::chrono::high_resolution_clock::time_point const deadline =
        std::chrono::high_resolution_clock::now() +
                std::chrono::milliseconds(100);

    int64_t user_id = 5;
    Command_P2POperation p2pop;
    Message response;
    RequestContext request_context;
    P2PRequest req(user_id, deadline, p2pop, &response, &request_context);

    req.WaitUntilCompleted();
    ASSERT_FALSE(req.IsCompleted());
    ASSERT_FALSE(req.IsInProgress());
    ASSERT_TRUE(req.WasAborted());
    ASSERT_EQ(proto::Command_Status_StatusCode_NOT_ATTEMPTED,
        response.command().status().code());
}

TEST_F(P2PRequestTest, WaitUntilCompletedWaitsMoreIfInProgress) {
// ARM doesn't have sleep_unitl, so we won't compile this test
// on ARM
#ifndef BUILD_FOR_ARM
    std::chrono::high_resolution_clock::time_point const deadline =
        std::chrono::high_resolution_clock::now() +
                std::chrono::milliseconds(100);

    int64_t user_id = 5;
    Command_P2POperation p2pop;
    Message response;
    RequestContext request_context;
    P2PRequest req(user_id, deadline, p2pop, &response, &request_context);

    req.StartProcessingIfAllowed();

    std::chrono::high_resolution_clock::time_point const later_deadline =
        deadline + std::chrono::milliseconds(150);

    // This thread is going to wait until well after the deadline, then mark complete
    std::thread t1([&] {
        std::this_thread::sleep_until(later_deadline);
        req.MarkCompletedAndNotify();
    });

    // This will wait, even after the deadline elapses
    req.WaitUntilCompleted();

    // We want to make sure that we had to wait until the later deadline
    // to get the completion
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - later_deadline);
    t1.join();

    ASSERT_GE(delta.count(), 0);
    ASSERT_TRUE(req.IsCompleted());
    ASSERT_FALSE(req.IsInProgress());
    ASSERT_FALSE(req.WasAborted());
#endif
}

TEST_F(P2PRequestTest, StartProcessingIfAllowedStartsProgress) {
    std::chrono::high_resolution_clock::time_point const deadline =
        std::chrono::high_resolution_clock::now() +
            std::chrono::milliseconds(10);

    int64_t user_id = 5;
    Command_P2POperation p2pop;
    Message response;
    RequestContext request_context;
    P2PRequest req(user_id, deadline, p2pop, &response, &request_context);
    ASSERT_TRUE(req.StartProcessingIfAllowed());
    ASSERT_TRUE(req.IsInProgress());
}

TEST_F(P2PRequestTest, StartProcessingIfAllowedReturnsFalseIfAborted) {
    std::chrono::high_resolution_clock::time_point const deadline =
        std::chrono::high_resolution_clock::now() +
                std::chrono::milliseconds(10);

    int64_t user_id = 5;
    Command_P2POperation p2pop;
    Message response;
    RequestContext request_context;
    P2PRequest req(user_id, deadline, p2pop, &response, &request_context);
    req.SetAborted(true);

    ASSERT_FALSE(req.StartProcessingIfAllowed());
}

TEST_F(P2PRequestTest, MarkCompletedAndNotifySetsCompletedAndInprogress) {
    std::chrono::high_resolution_clock::time_point const deadline =
        std::chrono::high_resolution_clock::now() +
                std::chrono::milliseconds(10);

    int64_t user_id = 5;
    Command_P2POperation p2pop;
    Message response;
    RequestContext request_context;
    P2PRequest req(user_id, deadline, p2pop, &response, &request_context);
    req.MarkCompletedAndNotify();

    ASSERT_FALSE(req.IsInProgress());
    ASSERT_TRUE(req.IsCompleted());
}
*/
} // namespace kinetic
} // namespace seagate
} // namespace com
