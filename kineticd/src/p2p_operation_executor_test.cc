#include "gtest/gtest.h"
#include "glog/logging.h"

#include "kinetic/incoming_value.h"
#include "kinetic/kinetic.h"

#include "file_system_store.h"
#include "hmac_authenticator.h"
#include "primary_store.h"
#include "message_processor.h"
#include "mock_skinny_waist.h"
#include "std_map_key_value_store.h"
#include "user_store.h"
#include "mock_device_information.h"
#include "mock_cluster_version_store.h"
#include "threadsafe_blocking_queue.h"
#include "p2p_request.h"
#include "limits.h"
#include "launch_monitor.h"
#include "command_line_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::P2PPushCallbackInterface;
using ::kinetic::IncomingStringValue;
using ::kinetic::PutCallbackInterface;
using ::kinetic::NonblockingKineticConnection;

using namespace com::seagate::kinetic::proto; //NOLINT

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::NiceMock;
using ::testing::DoAll;
using ::testing::WithArgs;
using testing::Sequence;
using testing::InSequence;

using ::std::string;
using ::std::shared_ptr;
using ::std::make_shared;
using ::std::unique_ptr;

using com::seagate::kinetic::Limits;

class MockKineticConnectionFactory : public ::kinetic::KineticConnectionFactory {
    public:
    MockKineticConnectionFactory() :
        ::kinetic::KineticConnectionFactory(::kinetic::HmacProvider()) {}

    MOCK_METHOD2(NewNonblockingConnection, ::kinetic::Status(
            const ::kinetic::ConnectionOptions& options,
            unique_ptr<NonblockingKineticConnection>& connection));
};

class MockNonblockingKineticConnection : public ::kinetic::NonblockingKineticConnection {
    public:
    MockNonblockingKineticConnection() : ::kinetic::NonblockingKineticConnection(NULL) {}

    MOCK_METHOD3(Run, bool(fd_set* read_fds, fd_set* write_fds, int*nfds));

    MOCK_METHOD5(Put, ::kinetic::HandlerKey(const string key,
    const string current_version, ::kinetic::WriteMode mode,
    const shared_ptr<const ::kinetic::KineticRecord> record,
    const shared_ptr<PutCallbackInterface> callback));

    MOCK_METHOD2(P2PPush, ::kinetic::HandlerKey(const P2PPushRequest& push_request,
            const shared_ptr<P2PPushCallbackInterface> callback));

    private:
    DISALLOW_COPY_AND_ASSIGN(MockNonblockingKineticConnection);
};

ACTION(SimulatePutCallbackSuccess) { arg4->Success(); }
ACTION_P(SimulatePutCallbackError, error) { arg4->Failure(error); }
ACTION(ResetCount) {*arg5 = 0;}
ACTION_P2(DecreaseCache, cache, amount) {*cache -= amount; }
ACTION_P(CaptureP2PPushRequest, req) {*req = arg0;}

class P2POperationExecutorTest : public ::testing::Test {
    protected:
    P2POperationExecutorTest():
    db_(),
    limits_(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5),
    profiler_(),
    launch_monitor_(),
    file_system_store_("test_file_system_store"),
    device_information_(),
    instant_secure_eraser_(),
    primary_store_(file_system_store_,
            db_,
            mock_cluster_version_store_,
            device_information_,
            profiler_,
            32 * 1024,
            instant_secure_eraser_,
            "fsize"),
    authorizer_(user_store_, profiler_),
    skinny_waist_("primary.db",
            FLAGS_store_partition,
            FLAGS_store_mountpoint,
            authorizer_,
            user_store_,
            primary_store_,
            profiler_,
            mock_cluster_version_store_,
            launch_monitor_),
    user_store_(std::move(unique_ptr<CautiousFileHandlerInterface>(
            new BlackholeCautiousFileHandler())), limits_),
    mock_skinny_waist_(),
    operation_bytes_counter_(),
    operation_invocation_counter_(),
    mock_connection_factory_(),
    request_queue_(100),
    p2p_operation_executor_(authorizer_,
            user_store_,
            mock_connection_factory_,
            skinny_waist_,
            100,
            75,
            &request_queue_),
    deadline_(std::chrono::high_resolution_clock::now() + std::chrono::seconds(1))
    {}

    virtual void SetUp() {
        CHECK(user_store_.CreateDemoUser());
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_file_system_store"));
        ASSERT_NE(-1, system("rm -rf fsize"));
    }

    void copyConnToParam(unique_ptr< ::kinetic::NonblockingKineticConnection>& ptr) {
        ptr.reset(mock_nonblocking_kinetic_connection_);
    }

    void expectConnectionAttempt() {
        mock_nonblocking_kinetic_connection_ = new MockNonblockingKineticConnection();

        EXPECT_CALL(mock_connection_factory_, NewNonblockingConnection(_, _))
                .WillOnce(DoAll(
                SaveArg<0>(&last_connection_options_),
                WithArgs<1>(Invoke(this, &P2POperationExecutorTest::copyConnToParam)),
                Return(::kinetic::Status::makeOk())));

        EXPECT_CALL(
        *mock_nonblocking_kinetic_connection_,
        Run(_, _, _)).WillRepeatedly(Return(false));
    }

    StdMapKeyValueStore db_;
    Limits limits_;
    Profiler profiler_;
    LaunchMonitorPassthrough launch_monitor_;
    FileSystemStore file_system_store_;
    MockDeviceInformation device_information_;
    MockInstantSecureEraser instant_secure_eraser_;
    PrimaryStore primary_store_;
    Authorizer authorizer_;
    NiceMock<MockClusterVersionStore> mock_cluster_version_store_;
    SkinnyWaist skinny_waist_;
    UserStore user_store_;
    MockSkinnyWaist mock_skinny_waist_;
    StatisticsManager operation_bytes_counter_;
    StatisticsManager operation_invocation_counter_;
    MockNonblockingKineticConnection* mock_nonblocking_kinetic_connection_;
    MockKineticConnectionFactory mock_connection_factory_;
    ThreadsafeBlockingQueue<std::shared_ptr<P2PRequest>> request_queue_;
    P2POperationExecutor p2p_operation_executor_;
    ::kinetic::ConnectionOptions last_connection_options_;
    std::chrono::high_resolution_clock::time_point const deadline_;
};

TEST_F(P2POperationExecutorTest, Peer2PeerPushReturnsFailureIfUserUnauthorized) {
    User user(123, "foobarbaz", std::list<Domain>());
    ASSERT_TRUE(user_store_.Put(user));

    Command_P2POperation p2pop;
    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(123, deadline_, p2pop, &response_command, request_context);

    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, response_command.status().code());
}

TEST_F(P2POperationExecutorTest, Peer2PeerPushConnectsWithCorrectInformation) {
    std::list<Domain> domains = std::list<Domain>();
    domains.push_back(Domain(0, "", Domain::kAllRoles, false));
    User user(99, "foobarbaz", domains);
    ASSERT_TRUE(user_store_.Put(user));

    Command_P2POperation p2pop;
    p2pop.mutable_peer()->set_hostname("1.2.3.4");
    p2pop.mutable_peer()->set_port(1234);
    p2pop.mutable_peer()->set_tls(true);

    Command response_command;
    expectConnectionAttempt();
    RequestContext request_context;
    p2p_operation_executor_.Execute(99, deadline_, p2pop, &response_command, request_context);

    EXPECT_EQ("1.2.3.4", last_connection_options_.host);
    EXPECT_EQ(1234, last_connection_options_.port);
    EXPECT_TRUE(last_connection_options_.use_ssl);
    EXPECT_EQ(99, last_connection_options_.user_id);
    EXPECT_EQ("foobarbaz", last_connection_options_.hmac_key);
}

TEST_F(P2POperationExecutorTest, Peer2PeerPushConnectsReturnsErrorIfConnectionFails) {
    EXPECT_CALL(mock_connection_factory_, NewNonblockingConnection(_, _))
            .WillOnce(Return(::kinetic::Status::makeInternalError("Connection failure")));

    Command_P2POperation p2pop;
    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    EXPECT_EQ(Command_Status_StatusCode_REMOTE_CONNECTION_ERROR,
    response_command.status().code());
}

TEST_F(P2POperationExecutorTest, Peer2PeerPushReturnsP2PFailureIfLocalGetNotFound) {
    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();

    Command response_command;

    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_EQ(1, response_command.body().p2poperation().operation_size());
    EXPECT_EQ(Command_Status_StatusCode_NOT_FOUND,
    response_command.body().p2poperation().operation(0).status().code());
}

TEST_F(P2POperationExecutorTest, Peer2PeerPushReturnsP2PFailureIfLocalTagAlgorithmInvalid) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 999;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();

    Command response_command;

    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_EQ(1, response_command.body().p2poperation().operation_size());
    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR,
    response_command.body().p2poperation().operation(0).status().code());
    EXPECT_EQ("Invalid tag algorithm",
    response_command.body().p2poperation().operation(0).status().statusmessage());
}

TEST_F(P2POperationExecutorTest, Peer2PeerPushSendsCorrectP2PRequest) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    EXPECT_CALL(
            *mock_nonblocking_kinetic_connection_,
            Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _) )
            .WillOnce(
                DoAll(
                SimulatePutCallbackSuccess(),
                Return(123456)));

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_, Run(_, _, _)).WillRepeatedly(Return(true));

    Command response_command;

    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, response_command.status().code());
}

TEST_F(P2POperationExecutorTest, NestedP2PsAreSentAlong) {
        EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;
    p2pop.mutable_peer()->set_hostname("foo.tld");
    p2pop.mutable_peer()->set_port(1234);
    p2pop.mutable_peer()->set_tls(true);

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    auto nested_1 = op->mutable_p2pop();
    nested_1->mutable_peer()->set_hostname("bar.tld");
    nested_1->mutable_peer()->set_port(1235);

    auto nested_1_op = nested_1->add_operation();
    nested_1_op->set_key("key");
    nested_1_op->set_newkey("bar_key");
    nested_1_op->set_version("bar_version");
    nested_1_op->set_force(false);

    auto nested_2 = nested_1_op->mutable_p2pop();
    nested_2->mutable_peer()->set_hostname("baz.tld");
    nested_2->mutable_peer()->set_port(1236);

    auto nested_2_op = nested_2->add_operation();
    nested_2_op->set_key("key");
    nested_2_op->set_newkey("baz_key");
    nested_2_op->set_version("baz_version");
    nested_2_op->set_force(true);

    expectConnectionAttempt();

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
       Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
            .WillOnce(
            DoAll(
            SimulatePutCallbackError(
                ::kinetic::KineticStatus(::kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH,
                "oh no")),
            Return(123456)));

    ::kinetic::P2PPushRequest req;
    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
       P2PPush(_, _))
            .WillOnce(
            DoAll(
            CaptureP2PPushRequest(&req),
            Return(123457)));

    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    // We expect the p2p push request to reflect the structure of the nested message
    ASSERT_EQ("bar.tld", req.host);
    ASSERT_EQ(1235, req.port);
    ASSERT_EQ(1UL, req.operations.size());

    auto req_op0 = req.operations[0];
    ASSERT_EQ("key", req_op0.key);
    ASSERT_EQ("bar_key", req_op0.newKey);
    ASSERT_EQ("bar_version", req_op0.version);
    ASSERT_FALSE(req_op0.force);

    auto nested_req = req_op0.request;
    ASSERT_EQ("baz.tld", nested_req->host);
    ASSERT_EQ(1236, nested_req->port);
    ASSERT_EQ(1UL, nested_req->operations.size());

    auto nested_req_op0 = nested_req->operations[0];
    ASSERT_EQ("key", nested_req_op0.key);
    ASSERT_EQ("baz_key", nested_req_op0.newKey);
    ASSERT_EQ("baz_version", nested_req_op0.version);
    ASSERT_TRUE(nested_req_op0.force);

    ASSERT_EQ(nullptr, nested_req_op0.request);
}

TEST_F(P2POperationExecutorTest, Peer2PeerPushReturnsOperationErrorIfPutFails) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    shared_ptr< ::kinetic::PutCallbackInterface> callback;
    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
    Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
            .WillOnce(
            DoAll(
            SimulatePutCallbackError(
                ::kinetic::KineticStatus(::kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH,
                "oh no")),
            Return(123456)));

    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_EQ(1, response_command.body().p2poperation().operation_size());
    EXPECT_EQ(Command_Status_StatusCode_VERSION_FAILURE,
        response_command.body().p2poperation().operation(0).status().code());
    EXPECT_EQ("PUT failed with message: oh no",
        response_command.body().p2poperation().operation(0).status().statusmessage());
}

TEST_F(P2POperationExecutorTest, IncompletePeer2PeerOperationsHaveRemoteConnectionErrorStatus) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    shared_ptr< ::kinetic::PutCallbackInterface> callback;

    // Don't simulate a callback call
    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
        Put("key", _, _, _, _))
        .WillOnce(Return(123456));

    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_EQ(1, response_command.body().p2poperation().operation_size());
    EXPECT_EQ(Command_Status_StatusCode_REMOTE_CONNECTION_ERROR,
        response_command.body().p2poperation().operation(0).status().code());
}

TEST_F(P2POperationExecutorTest, RunErrorsResultInErrorCode) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    shared_ptr< ::kinetic::PutCallbackInterface> callback;

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
        Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
            .WillOnce(
            DoAll(
            SimulatePutCallbackSuccess(),
            Return(123456)));

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_, Run(_, _, _)).WillOnce(Return(false));


    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_EQ(Command_Status_StatusCode_REMOTE_CONNECTION_ERROR,
            response_command.status().code());
}

TEST_F(P2POperationExecutorTest, SuccessfulOperationsSetFlag) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    shared_ptr< ::kinetic::PutCallbackInterface> callback;

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_, Run(_, _, _)).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
        Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
            .WillOnce(
            DoAll(
            SimulatePutCallbackSuccess(),
            Return(123456)));

    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_TRUE(response_command.body().p2poperation().allchildoperationssucceeded());
}

TEST_F(P2POperationExecutorTest, FailedOperationsSetFlag) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    shared_ptr< ::kinetic::PutCallbackInterface> callback;

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
        Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
            .WillOnce(
            DoAll(
            SimulatePutCallbackError(
                ::kinetic::KineticStatus(::kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH,
                "oh no")),
            Return(123456)));

    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_FALSE(response_command.body().p2poperation().allchildoperationssucceeded());
}

TEST_F(P2POperationExecutorTest, IncompletOperationsSetFlag) {
    EXPECT_CALL(device_information_, GetCapacity(_, _))
            .WillOnce(DoAll(
            SetArgPointee<0>(1000000000000),
            SetArgPointee<1>(0),
            Return(true)));

    PrimaryStoreValue psv;
    psv.algorithm = 1;
    IncomingStringValue isv("asdf");
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key", psv, &isv, false));

    Command_P2POperation p2pop;

    Command_P2POperation_Operation *op = p2pop.add_operation();
    op->set_key("key");
    op->set_version("version");
    op->set_force(true);

    expectConnectionAttempt();
    shared_ptr< ::kinetic::PutCallbackInterface> callback;

    EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
        Put("key", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
            .WillOnce(Return(123456));

    Command response_command;
    RequestContext request_context;
    p2p_operation_executor_.Execute(1, deadline_, p2pop, &response_command, request_context);

    ASSERT_FALSE(response_command.body().p2poperation().allchildoperationssucceeded());
}

//////////////////////////////////////////////////////////////////
/////////////// T E S T   O B S O L E T E ////////////////////////
//////////////////////////////////////////////////////////////////
/// 3-19-2015 : James D                                        ///
/// P2P behavior has slightly changed after ASOKVAD-312        ///
/// A proper substitute for this test has yet to be determined ///
//////////////////////////////////////////////////////////////////
///TEST_F(P2POperationExecutorTest, PerformsIOWhenMemoryUsageExceedsMaxAllowed) {

    /////////////// T E S T   O B S O L E T E //////////////////////////////
    /// P2P behavior has slightly changed after ASOKVAD-312
    /// A proper substitute for this test has yet to be determined
    /////////////////////////////////////////////////////////////


    // EXPECT_CALL(device_information_, GetCapacity(_, _))
    //         .WillRepeatedly(DoAll(
    //         SetArgPointee<0>(1000000000000),
    //         SetArgPointee<1>(0),
    //         Return(true)));

    // // This is a special mock which lets us mock DoIO but use the real Execute.
    // // We need to construct it explicitly like this so Execute runs normally.
    // MockP2POperationExecutorInternal mock_p2p_operation_executor(authorizer_,
    //         user_store_,
    //         mock_connection_factory_,
    //         skinny_waist_,
    //         500,
    //         300,
    //         &request_queue_);

    // expectConnectionAttempt();


    // std::stringstream long_val_stream;
    // for (int i = 0; i < 200; ++i) {
    //     long_val_stream << "a";
    // }
    // string long_val = long_val_stream.str();

    // PrimaryStoreValue psv1;
    // psv1.algorithm = 1;
    // IncomingStringValue isv1(long_val);
    // ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key1", psv1, &isv1, false));

    // PrimaryStoreValue psv2;
    // psv2.algorithm = 1;
    // IncomingStringValue isv2(long_val);
    // ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key2", psv2, &isv2, false));

    // PrimaryStoreValue psv3;
    // psv3.algorithm = 1;
    // IncomingStringValue isv3("3");
    // ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key3", psv3, &isv3, false));

    // Command_P2POperation p2pop;

    // Command_P2POperation_Operation *op1 = p2pop.add_operation();
    // op1->set_key("key1");
    // op1->set_version("version");
    // op1->set_force(true);

    // Command_P2POperation_Operation *op2 = p2pop.add_operation();
    // op2->set_key("key2");
    // op2->set_version("version");
    // op2->set_force(true);

    // Command_P2POperation_Operation *op3 = p2pop.add_operation();
    // op3->set_key("key3");
    // op3->set_version("version");
    // op3->set_force(true);

    // // We use a Sequence here to ensure that events happen in this order:
    // // 1) The first two keys are put, saturating the write cache
    // //   ... and DoRun is called in each loop
    // // 2) DoIO is called, simulating the clearing of the cache
    // // 3) The third Key is put
    // // 4) DoIO is called at the end of the loop
    // Sequence s;
    // EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
    //     Put("key1", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
    //     .InSequence(s).WillOnce(Return(123456));

    // EXPECT_CALL(mock_p2p_operation_executor, DoRun(_, _))
    //     .InSequence(s).WillOnce(Return(true));

    // EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
    //     Put("key2", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
    //     .InSequence(s).WillOnce(Return(123456));

    // EXPECT_CALL(mock_p2p_operation_executor, DoRun(_, _))
    //  .InSequence(s).WillOnce(Return(true));

    // // Middle of loop (abort = true)
    // EXPECT_CALL(mock_p2p_operation_executor, DoIO(_, _, _, _, true, _))
    //     .InSequence(s).WillOnce(DoAll(ResetCount(), Return(true)));

    // EXPECT_CALL(*mock_nonblocking_kinetic_connection_,
    //     Put("key3", "version", ::kinetic::WriteMode::IGNORE_VERSION, _, _))
    //     .InSequence(s).WillOnce(Return(123456));

    // EXPECT_CALL(mock_p2p_operation_executor, DoRun(_, _))
    //     .InSequence(s).WillOnce(Return(true));


    // // End of loop (abort = false)
    // EXPECT_CALL(mock_p2p_operation_executor, DoIO(_, _, _, _, false, _))
    //     .InSequence(s).WillOnce(DoAll(ResetCount(), Return(true)));

    // Command response_command;
    // RequestContext request_context;
    // mock_p2p_operation_executor.Execute(1, deadline_, p2pop, &response_command, request_context);
//////////////////////////////////////////////////////////////////
/////////////// T E S T   O B S O L E T E ////////////////////////
//////////////////////////////////////////////////////////////////
// }

TEST_F(P2POperationExecutorTest, DoIOAbortsWhenCacheSizeDecreases) {
    int outstanding_puts = 1;
    int outstanding_pushes = 0;
    Command response_command;
    // Slightly over limit
    size_t cache_size = 101;
    MockNonblockingKineticConnection mock_nonblocking_kinetic_connection;

    EXPECT_CALL(
        mock_nonblocking_kinetic_connection,
        Run(_, _, _)).WillOnce(DoAll(DecreaseCache(&cache_size, 26), Return(true)));

    EXPECT_TRUE(p2p_operation_executor_.DoIO(&outstanding_puts, &outstanding_pushes,
        &response_command, deadline_, mock_nonblocking_kinetic_connection, true, &cache_size));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
