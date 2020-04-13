#include "gtest/gtest.h"
#include "kinetic/kinetic.h"
#include "p2p_push_callback.h"
#include "kinetic.pb.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::vector;
using std::unique_ptr;

using proto::Message;
using proto::Command_P2POperation;

using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_NOT_FOUND;
using proto::Command_Status_StatusCode_NOT_ATTEMPTED;
using proto::Command_Status_StatusCode_NESTED_OPERATION_ERRORS;
using proto::Command_Status_StatusCode_EXPIRED;

class P2PPushCallbackTest : public ::testing::Test {
};

/*

TEST_F(P2PPushCallbackTest, SuccessAddsResponse) {
    com::seagate::kinetic::client::proto::Message incoming_response;
    auto incoming_p2p =
            incoming_response.mutable_command()->mutable_body()->mutable_p2poperation();

    incoming_p2p->set_allchildoperationssucceeded(false);
    incoming_p2p->mutable_peer()->set_hostname("bar.tld");
    incoming_p2p->mutable_peer()->set_port(9999);
    auto incoming_p2p_op = incoming_p2p->add_operation();
    incoming_p2p_op->set_key("bar_key");
    incoming_p2p_op->set_version("bar_version");
    incoming_p2p_op->set_force(true);
    incoming_p2p_op->mutable_status()->set_code(
        com::seagate::kinetic::client::proto::Command_Status_StatusCode_SUCCESS);

    auto nested_incoming_p2p = incoming_p2p_op->mutable_p2pop();
    nested_incoming_p2p->mutable_peer()->set_hostname("zip.tld");
    nested_incoming_p2p->mutable_peer()->set_port(7777);

    auto nested_incoming_p2p_op = nested_incoming_p2p->add_operation();
    nested_incoming_p2p_op->set_key("zip_key");
    nested_incoming_p2p_op->set_version("zip_version");
    nested_incoming_p2p_op->set_force(true);
    nested_incoming_p2p_op->mutable_status()->set_code(
        com::seagate::kinetic::client::proto::Command_Status_StatusCode_NOT_FOUND);
    nested_incoming_p2p_op->mutable_status()->set_statusmessage("Who dat is?");

    int outstanding_pushes = 0;
    proto::Command_Status status;
    int successful_operation_count = 0;
    size_t heuristic_cache_size = 100;
    size_t heuristic_operation_size = 10;
    Command_P2POperation actual_p2p;

    P2PPushCallback callback(&outstanding_pushes,
        &status,
        &successful_operation_count,
        &heuristic_cache_size,
        heuristic_operation_size,
        &actual_p2p);

    unique_ptr<vector<KineticStatus>> statuses;
    callback.Success(std::move(statuses), incoming_response);

    ASSERT_EQ(0, successful_operation_count);

    ASSERT_EQ(status.code(), Command_Status_StatusCode_NESTED_OPERATION_ERRORS);
    ASSERT_EQ("bar.tld", actual_p2p.peer().hostname());
    ASSERT_EQ(9999, actual_p2p.peer().port());
    ASSERT_EQ(1, actual_p2p.operation_size());

    auto actual_p2p_op = actual_p2p.operation(0);
    ASSERT_EQ("bar_key", actual_p2p_op.key());
    ASSERT_EQ("bar_version", actual_p2p_op.version());
    ASSERT_TRUE(actual_p2p_op.force());
    ASSERT_EQ(Command_Status_StatusCode_SUCCESS, actual_p2p_op.status().code());

    auto actual_nested_p2p = actual_p2p_op.p2pop();
    ASSERT_EQ("zip.tld", actual_nested_p2p.peer().hostname());
    ASSERT_EQ(7777, actual_nested_p2p.peer().port());
    ASSERT_EQ(1, actual_nested_p2p.operation_size());

    auto actual_nested_p2p_op = actual_nested_p2p.operation(0);
    ASSERT_EQ("zip_key", actual_nested_p2p_op.key());
    ASSERT_EQ("zip_version", actual_nested_p2p_op.version());
    ASSERT_TRUE(actual_nested_p2p_op.force());
    ASSERT_EQ(Command_Status_StatusCode_NOT_FOUND, actual_nested_p2p_op.status().code());
    ASSERT_EQ("Who dat is?", actual_nested_p2p_op.status().statusmessage());
}

TEST_F(P2PPushCallbackTest, SuccessIncrementsSuccessCounterWhenAllChildOpsSucceeded) {
    com::seagate::kinetic::client::proto::Message incoming_response;
    auto incoming_p2p =
            incoming_response.mutable_command()->mutable_body()->mutable_p2poperation();

    incoming_p2p->set_allchildoperationssucceeded(true);

    int outstanding_pushes = 0;
    proto::Command_Status status;
    int successful_operation_count = 0;
    size_t heuristic_cache_size = 100;
    size_t heuristic_operation_size = 10;
    Command_P2POperation actual_p2p;

    P2PPushCallback callback(&outstanding_pushes,
        &status,
        &successful_operation_count,
        &heuristic_cache_size,
        heuristic_operation_size,
        &actual_p2p);

    unique_ptr<vector<KineticStatus>> statuses;
    callback.Success(std::move(statuses), incoming_response);

    ASSERT_EQ(1, successful_operation_count);
}

TEST_F(P2PPushCallbackTest, FailureAddsResponse) {
  com::seagate::kinetic::client::proto::Message incoming_response;
    auto incoming_p2p =
            incoming_response.mutable_command()->mutable_body()->mutable_p2poperation();

    incoming_p2p->set_allchildoperationssucceeded(false);
    incoming_p2p->mutable_peer()->set_hostname("bar.tld");
    incoming_p2p->mutable_peer()->set_port(9999);
    auto incoming_p2p_op = incoming_p2p->add_operation();
    incoming_p2p_op->set_key("bar_key");
    incoming_p2p_op->set_version("bar_version");
    incoming_p2p_op->set_force(true);
    incoming_p2p_op->mutable_status()->set_code(
        com::seagate::kinetic::client::proto::Command_Status_StatusCode_NOT_ATTEMPTED);

    auto nested_incoming_p2p = incoming_p2p_op->mutable_p2pop();
    nested_incoming_p2p->mutable_peer()->set_hostname("zip.tld");
    nested_incoming_p2p->mutable_peer()->set_port(7777);

    auto nested_incoming_p2p_op = nested_incoming_p2p->add_operation();
    nested_incoming_p2p_op->set_key("zip_key");
    nested_incoming_p2p_op->set_version("zip_version");
    nested_incoming_p2p_op->set_force(true);
    nested_incoming_p2p_op->mutable_status()->set_code(
        com::seagate::kinetic::client::proto::Command_Status_StatusCode_SUCCESS);

    int outstanding_pushes = 0;
    proto::Command_Status status;
    int successful_operation_count;
    size_t heuristic_cache_size = 100;
    size_t heuristic_operation_size = 10;
    Command_P2POperation actual_p2p;

    P2PPushCallback callback(&outstanding_pushes,
        &status,
        &successful_operation_count,
        &heuristic_cache_size,
        heuristic_operation_size,
        &actual_p2p);

    ::kinetic::KineticStatus error(
        ::kinetic::StatusCode::REMOTE_EXPIRED,
        "Not all good, man. Not. All. Good.");

    callback.Failure(error, &incoming_response);

    ASSERT_EQ(status.code(), Command_Status_StatusCode_EXPIRED);
    ASSERT_EQ("bar.tld", actual_p2p.peer().hostname());
    ASSERT_EQ(9999, actual_p2p.peer().port());
    ASSERT_EQ(1, actual_p2p.operation_size());

    auto actual_p2p_op = actual_p2p.operation(0);
    ASSERT_EQ("bar_key", actual_p2p_op.key());
    ASSERT_EQ("bar_version", actual_p2p_op.version());
    ASSERT_TRUE(actual_p2p_op.force());
    ASSERT_EQ(Command_Status_StatusCode_NOT_ATTEMPTED, actual_p2p_op.status().code());

    auto actual_nested_p2p = actual_p2p_op.p2pop();
    ASSERT_EQ("zip.tld", actual_nested_p2p.peer().hostname());
    ASSERT_EQ(7777, actual_nested_p2p.peer().port());
    ASSERT_EQ(1, actual_nested_p2p.operation_size());

    auto actual_nested_p2p_op = actual_nested_p2p.operation(0);
    ASSERT_EQ("zip_key", actual_nested_p2p_op.key());
    ASSERT_EQ("zip_version", actual_nested_p2p_op.version());
    ASSERT_TRUE(actual_nested_p2p_op.force());
    ASSERT_EQ(Command_Status_StatusCode_SUCCESS, actual_nested_p2p_op.status().code());
}
*/

} // namespace kinetic
} // namespace seagate
} // namespace com
