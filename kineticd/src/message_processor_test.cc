#include "gtest/gtest.h"

#include "kinetic/incoming_value.h"
#include <string.h>

#include "mock_authorizer.h"
#include "authorizer_interface.h"
#include "domain.h"
#include "file_system_store.h"
#include "hmac_authenticator.h"
#include "primary_store.h"
#include "message_processor.h"
#include "mock_device_information.h"
#include "mock_skinny_waist.h"
#include "std_map_key_value_store.h"
#include "user_store.h"
#include "network_interfaces.h"
#include "mock_cluster_version_store.h"
#include "connection_time_handler.h"
#include "launch_monitor.h"
#include <fstream>
#include "command_line_flags.h"
#include "instant_secure_eraser.h"
#include "smrdisk/Disk.h"

using namespace com::seagate::kinetic::proto; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {
using ::kinetic::IncomingBuffValue;

using ::kinetic::IncomingStringValue;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::NiceMock;

class MessageProcessorTest : public ::testing::Test {
    protected:
    MessageProcessorTest():
        db_(FLAGS_store_test_partition,
            FLAGS_table_cache_size,
            FLAGS_block_size,
            FLAGS_sst_size),
        limits_(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000),
        user_store_(std::move(unique_ptr<CautiousFileHandlerInterface>(
                new BlackholeCautiousFileHandler())), limits_),
        profiler_(),
        launch_monitor_(),
        file_system_store_("test_file_system_store"),
        device_information_(),
        instant_secure_eraser_(),
        primary_store_(file_system_store_,
                db_, mock_cluster_version_store_, device_information_,
                profiler_,
                32 * 1024,
                instant_secure_eraser_,
                FLAGS_preused_file_path),
        authorizer_(user_store_, profiler_, limits_),
        skinny_waist_("primary.db",
                FLAGS_store_partition,
                FLAGS_store_mountpoint,
                FLAGS_metadata_partition,
                FLAGS_metadata_mountpoint,
                authorizer_, user_store_, primary_store_, profiler_,
                mock_cluster_version_store_,
                launch_monitor_),
        hmac_provider_(),
        authenticator_(user_store_, hmac_provider_),
        get_log_handler_(db_,
                device_information_,
                network_interfaces_,
                123,
                456,
                limits_,
                statistics_manager_),
        security_handler_(),
        setup_handler_(authorizer_, skinny_waist_, mock_cluster_version_store_,
            "/tmp", security_handler_, device_information_),
        pinop_handler_(skinny_waist_, FLAGS_metadata_mountpoint, FLAGS_metadata_partition),
        power_manager_(skinny_waist_, FLAGS_store_test_partition),
        message_processor_(authorizer_,
                skinny_waist_,
                profiler_,
                mock_cluster_version_store_,
                "/tmp",
                500,
                mock_p2p_request_manager_,
                get_log_handler_,
                setup_handler_,
                pinop_handler_,
                power_manager_,
                limits_,
                user_store_),
        mock_skinny_waist_(),
        message_processor_with_mocks_(
                mock_authorizer_,
                mock_skinny_waist_,
                profiler_,
                mock_cluster_version_store_,
                "/tmp",
                500,
                mock_p2p_request_manager_,
                get_log_handler_,
                setup_handler_,
                pinop_handler_,
                power_manager_,
                limits_,
                user_store_),
        empty_value_("") {}

    virtual void SetUp() {
        smr::Disk::initializeSuperBlockAddr(FLAGS_store_test_partition);
        InstantSecureEraserX86::ClearSuperblocks(FLAGS_store_test_partition);
        ASSERT_TRUE(file_system_store_.Init(true));
        ASSERT_TRUE(db_.Init(true));
        db_.SetListOwnerReference(&send_pending_status_sender_);

        // Create a user and give it global permissions
        Domain domain(0, "", Domain::kRead | Domain::kWrite | Domain::kDelete |
            Domain::kRange | Domain::kSetup | Domain::kSecurity, false);
        std::list<Domain> domains;
        domains.push_back(domain);
        User test_user(42, "super secret", domains);
        std::list<User> users;
        users.push_back(test_user);
        ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));

        // Simulate a file system with plenty of free space
        EXPECT_CALL(device_information_, GetCapacity(_, _)).WillRepeatedly(DoAll(
            SetArgPointee<0>(4000000000000L),
            SetArgPointee<1>(1000000000000L),
            Return(true)));
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_file_system_store"));
        ASSERT_NE(-1, system("rm -rf fsize"));
        EXPECT_CALL(send_pending_status_sender_, SendAllPending(_, _));
        db_.Close();
        std::stringstream command;
        uint64_t seek_to;
        seek_to = smr::Disk::SUPERBLOCK_0_ADDR/1048576;
        command << "dd if=/dev/zero of=" << FLAGS_store_test_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT
        std::string system_command = command.str();
        if (!com::seagate::kinetic::execute_command(system_command)) {
            LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
        }
        command.str("");
        seek_to = smr::Disk::SUPERBLOCK_1_ADDR/1048576;

        command << "dd if=/dev/zero of=" << FLAGS_store_test_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

        system_command = command.str();
        if (!com::seagate::kinetic::execute_command(system_command)) {
            LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
        }

        command.str("");
        seek_to = smr::Disk::SUPERBLOCK_2_ADDR/1048576;

        command << "dd if=/dev/zero of=" << FLAGS_store_test_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

        system_command = command.str();
        if (!com::seagate::kinetic::execute_command(system_command)) {
            LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
        }
    }

    // 3-17-15 added option to set hmac for different user ID's
    void SetUserAndHmac(Message *message, int alt_id = 0, std::string alt_key = "super secret") {
        HmacProvider hmac_provider;
        if (alt_id == 0) {
            message->mutable_hmacauth()->set_identity(42);
            message->mutable_hmacauth()->
                set_hmac(hmac_provider.ComputeHmac(*message, "super secret"));
        } else {
            message->mutable_hmacauth()->set_identity(alt_id);
            message->mutable_hmacauth()->
                set_hmac(hmac_provider.ComputeHmac(*message, alt_key));
        }
    }

    virtual void AssertEqual(const std::string &s, const NullableOutgoingValue &value) {
        std::string value_string;
        int err;
        ASSERT_TRUE(value.ToString(&value_string, &err));
        ASSERT_EQ(s, value_string);
    }

    void ProcessMessage(ConnectionRequestResponse& reqResp, Message* reqMsg, proto::Command& command,
            IncomingValueInterface* request_value,
            proto::Command *command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            ConnectionTimeHandler* time_handler,
            uint64_t userid,
            int64_t connection_id,
            int connFd = -1,
            bool connIdMismatched = false,
            bool corrupt = false) {
        reqResp.SetCommand(&command);
        reqResp.SetRequestValue(request_value);
        reqResp.SetResponseCommand(command_response);
        reqResp.SetTimeHandler(time_handler);
        reqResp.SetRequest(reqMsg);

        message_processor_.ProcessMessage(reqResp, response_value,
           request_context, connection_id, connFd, connIdMismatched, corrupt);
    }

    void ProcessMockMessage(ConnectionRequestResponse& reqResp, Message* reqMsg, proto::Command& command,
            IncomingValueInterface* request_value,
            proto::Command *command_response,
            NullableOutgoingValue *response_value,
            RequestContext& request_context,
            ConnectionTimeHandler* time_handler,
            uint64_t userid,
            int64_t connection_id,
            int connFd = -1,
            bool connIdMismatched = false,
            bool corrupt = false) {
        reqResp.SetCommand(&command);
        reqResp.SetRequestValue(request_value);
        reqResp.SetResponseCommand(command_response);
        reqResp.SetTimeHandler(time_handler);
        reqResp.SetRequest(reqMsg);

        message_processor_with_mocks_.ProcessMessage(reqResp, response_value,
           request_context, connection_id, connFd, connIdMismatched, corrupt);
    }


    KeyValueStore db_;
    Limits limits_;
    UserStore user_store_;
    Profiler profiler_;
    LaunchMonitorPassthrough launch_monitor_;
    FileSystemStore file_system_store_;
    MockDeviceInformation device_information_;
    MockNetworkInterfaces network_interfaces_;
    MockInstantSecureEraser instant_secure_eraser_;
    PrimaryStore primary_store_;
    Authorizer authorizer_;
    MockAuthorizer mock_authorizer_;
    SkinnyWaist skinny_waist_;
    HmacProvider hmac_provider_;
    HmacAuthenticator authenticator_;
    NiceMock<MockClusterVersionStore> mock_cluster_version_store_;
    MockP2PRequestManager mock_p2p_request_manager_;
    GetLogHandler get_log_handler_;
    MockSecurityHandler security_handler_;
    SetupHandler setup_handler_;
    PinOpHandler pinop_handler_;
    PowerManager power_manager_;
    MessageProcessor message_processor_;
    MockSkinnyWaist mock_skinny_waist_;
    MessageProcessor message_processor_with_mocks_;
    IncomingStringValue empty_value_;
    StatisticsManager statistics_manager_;
    MockSendPendingStatusInterface send_pending_status_sender_;
};

TEST_F(MessageProcessorTest, GetReturnsValueFromServer) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version";
    primary_store_value.tag = "tag";
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value, "value", 5);
    IncomingBuffValue value(the_value, 5);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    IncomingStringValue string_value("");
    int64_t connection_id = 0;

    ConnectionTimeHandler time_handler;
    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &string_value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ("key", command_response.body().keyvalue().key());
    EXPECT_EQ("version", command_response.body().keyvalue().dbversion());
    AssertEqual("value", response_value);
    EXPECT_EQ("tag", command_response.body().keyvalue().tag());
}

TEST_F(MessageProcessorTest, GetReturnsNotFoundForMissingKey) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_NOT_FOUND, command_response.status().code());
    AssertEqual("", response_value);
}

TEST_F(MessageProcessorTest, GetWithValueReturnsInvalidRequest) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    IncomingStringValue string_value("value");
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &string_value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
}

TEST_F(MessageProcessorTest, GetVersionReturnsVersionFromServer) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version";
    primary_store_value.tag = "tag";
    primary_store_value.algorithm = 0;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);

    IncomingBuffValue the_value(value, 5);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETVERSION);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ("version", command_response.body().keyvalue().dbversion());
    AssertEqual("", response_value);
}


TEST_F(MessageProcessorTest, GetVersionReturnsNotFoundForMissingKey) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETVERSION);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_NOT_FOUND, command_response.status().code());
    AssertEqual("", response_value);
}


TEST_F(MessageProcessorTest, GetMetadataOnlySpecifiesMetadataOnlyInResponse) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version";
    primary_store_value.tag = "tag";
    primary_store_value.algorithm = 2;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);

    IncomingBuffValue the_value(value, 5);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_metadataonly(true);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    IncomingStringValue string_value("");
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &string_value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ("key", command_response.body().keyvalue().key());
    EXPECT_EQ("version", command_response.body().keyvalue().dbversion());
    EXPECT_EQ("tag", command_response.body().keyvalue().tag());
    EXPECT_EQ(2, command_response.body().keyvalue().algorithm());
    EXPECT_EQ(0u, response_value.size());
    EXPECT_TRUE(command_response.body().keyvalue().metadataonly());
}


TEST_F(MessageProcessorTest, GetMetadataOnlyHandlesUnknownAlgorithm) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version";
    primary_store_value.tag = "tag";
    primary_store_value.algorithm = 2;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);
    IncomingBuffValue the_value(value, 5);

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_metadataonly(true);
        command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    IncomingStringValue string_value("");
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &string_value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ("key", command_response.body().keyvalue().key());
    EXPECT_EQ("version", command_response.body().keyvalue().dbversion());
    EXPECT_EQ("tag", command_response.body().keyvalue().tag());
    EXPECT_EQ(2, command_response.body().keyvalue().algorithm());
    EXPECT_EQ(0u, response_value.size());
    EXPECT_TRUE(command_response.body().keyvalue().metadataonly());
}

TEST_F(MessageProcessorTest, PutStoresValueInDB) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    command.mutable_body()->mutable_keyvalue()->set_algorithm(
            proto::Command_Algorithm_CRC64);
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;

    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value, "value", 5);
    IncomingBuffValue value(the_value, 5);

    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 0;
    NullableOutgoingValue result_value;
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
        primary_store_.Get("key", &primary_store_value, &result_value));
    AssertEqual("value", result_value);
    EXPECT_EQ("tag", primary_store_value.tag);
    EXPECT_EQ(proto::Command_Algorithm_CRC64, primary_store_value.algorithm);
}

TEST_F(MessageProcessorTest, PutEmptyValueStoresValueInDB) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    IncomingStringValue string_value("");
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &string_value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 0;
    NullableOutgoingValue value;
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
    primary_store_.Get("key", &primary_store_value, &value));
    AssertEqual("", response_value);
    EXPECT_EQ("tag", primary_store_value.tag);
}

TEST_F(MessageProcessorTest, PutReturnsVersionMismatchIfWrongVersionGiven) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);
    IncomingBuffValue the_value(value, 5);

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("wrongversion");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_VERSION_MISMATCH, command_response.status().code());
}

TEST_F(MessageProcessorTest, PutReturnsVersionMismatchIfNoVersionGivenForExisting) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);
    IncomingBuffValue the_value(value, 5);

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_VERSION_MISMATCH, command_response.status().code());
}

TEST_F(MessageProcessorTest, PutReturnsVersionMismatchIfVersionGivenForNotExisting) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("wrongversion");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_VERSION_MISMATCH, command_response.status().code());
}

TEST_F(MessageProcessorTest, PutResponseIncludesKeyValue) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("wrongversion");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_TRUE(command_response.body().has_keyvalue());
}

TEST_F(MessageProcessorTest, PutForceSucceedsWithInvalidVersion) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;
    char *the_value_a;
    char *the_value_b;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value_a, "value", 5);
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value_b, "value", 5);
    IncomingBuffValue value_a(the_value_a, 5);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &value_a, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("wrongversion");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    command.mutable_body()->mutable_keyvalue()->set_force(true);
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    command.mutable_body()->mutable_keyvalue()->set_flush(false);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    IncomingBuffValue value_b(the_value_b, 5);
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &value_b, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    LOG(ERROR) << "*reqResp, After message_processor_.ProcessMessage";

    NullableOutgoingValue value;
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
        primary_store_.Get("key", &primary_store_value, &value));
    AssertEqual("value", value);
}

TEST_F(MessageProcessorTest, PutFailsIfDriveFull) {
    EXPECT_CALL(device_information_, GetCapacity(_, _)).WillOnce(DoAll(
        SetArgPointee<0>(4000000000000L),
        SetArgPointee<1>(3995000000000L),
        Return(true)));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("key");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value, "value", 6);
    IncomingBuffValue value(the_value, 6);
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ(smr::Disk::DiskStatus::NO_SPACE, smr::Disk::_status);
}

TEST_F(MessageProcessorTest, PutFailsWithoutTag) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);
    char *value1 = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value1, "value", 5);
    std::string value2(std::string(value1), 5);
    IncomingStringValue value(value2);
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
}

TEST_F(MessageProcessorTest, PutFailsForClusterVersionMismatch) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);
    char *value1 = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value1, "value", 5);
    std::string value2(std::string(value1), 5);
    IncomingStringValue value(value2);
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    EXPECT_CALL(mock_cluster_version_store_, GetClusterVersion()).WillOnce(Return(55));

    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &value, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_VERSION_FAILURE, command_response.status().code());
    EXPECT_EQ(55, command_response.header().clusterversion());
}

TEST_F(MessageProcessorTest, DeleteRemovesValueFromDB) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version";
    primary_store_value.algorithm = 1;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);
    IncomingBuffValue the_value(value, 5);

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key123", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_DELETE);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("version");
    command.mutable_body()->mutable_keyvalue()->set_key("key123");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    char *actual_value = new char(sizeof(actual_value));
    EXPECT_EQ(StoreOperationStatus_NOT_FOUND, db_.Get("key123", actual_value));
    delete[] actual_value;
}

TEST_F(MessageProcessorTest, DeleteReturnsVersionMismatchIfDeletingWithWrongVersion) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);
    IncomingBuffValue the_value(value, 5);

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_DELETE);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("version");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_VERSION_MISMATCH, command_response.status().code());
}

TEST_F(MessageProcessorTest, DeleteForceSucceedsIfVersionMismatch) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;

    char *value;
    value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(value, "value", 5);
    IncomingBuffValue the_value(value, 5);

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("key", primary_store_value, &the_value, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_DELETE);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("version");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_force(true);
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
}

TEST_F(MessageProcessorTest, DeleteReturnsNotFoundIfDeletingNonexistingRecordWithVersion) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_DELETE);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("version");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_NOT_FOUND, command_response.status().code());
}

TEST_F(MessageProcessorTest, DeleteIncludesKV) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_DELETE);
    command.mutable_body()->mutable_keyvalue()->set_dbversion("version");
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_TRUE(command_response.body().has_keyvalue());
}

TEST_F(MessageProcessorTest, GetNextReturnsAllValueFields) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version_a";
    primary_store_value.tag = "tag_a";
    primary_store_value.algorithm = proto::Command_Algorithm_CRC64;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value_a, "value a", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("a", primary_store_value, &value_a, false, (token));
    primary_store_value.version = "version_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = proto::Command_Algorithm_CRC32;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value_b, "value b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    primary_store_.Put("b", primary_store_value, &value_b, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETNEXT);
    command.mutable_body()->mutable_keyvalue()->set_key("a");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ("b", command_response.body().keyvalue().key());
    AssertEqual("value b", response_value);
    EXPECT_EQ("version_b", command_response.body().keyvalue().dbversion());
    EXPECT_EQ("tag_b", command_response.body().keyvalue().tag());
    EXPECT_EQ(proto::Command_Algorithm_CRC32, command_response.body().keyvalue().algorithm());
}

TEST_F(MessageProcessorTest, GetPreviousReturnsAllValueFields) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version_a";
    primary_store_value.tag = "tag_a";
    primary_store_value.algorithm = proto::Command_Algorithm_CRC64;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value_a, "value a", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("a", primary_store_value, &value_a, false, (token));
    primary_store_value.version = "version_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = proto::Command_Algorithm_CRC32;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value_b, "value a", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    primary_store_.Put("b", primary_store_value, &value_b, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETPREVIOUS);
    command.mutable_body()->mutable_keyvalue()->set_key("b");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ("a", command_response.body().keyvalue().key());
    AssertEqual("value a", response_value);
    EXPECT_EQ("version_a", command_response.body().keyvalue().dbversion());
    EXPECT_EQ("tag_a", command_response.body().keyvalue().tag());
    EXPECT_EQ(proto::Command_Algorithm_CRC64, command_response.body().keyvalue().algorithm());
}

TEST_F(MessageProcessorTest, GetKeyRangeHandlesInclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    char *the_value_a;
    char *the_value_b;
    char *the_value_c;
    char *the_value_d;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_c = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_d = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    memcpy(the_value_a, "foo", 3);
    memcpy(the_value_b, "foo", 3);
    memcpy(the_value_c, "foo", 3);
    memcpy(the_value_d, "foo", 3);

    IncomingBuffValue value_a(the_value_a, 3);
    IncomingBuffValue value_b(the_value_b, 3);
    IncomingBuffValue value_c(the_value_c, 3);
    IncomingBuffValue value_d(the_value_d, 3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("a", primary_store_value, &value_a, false, (token));
    primary_store_.Put("b", primary_store_value, &value_b, false, (token));
    primary_store_.Put("c", primary_store_value, &value_c, false, (token));
    primary_store_.Put("d", primary_store_value, &value_d, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("a");
    range->set_startkeyinclusive(true);
    range->set_endkey("d");
    range->set_endkeyinclusive(true);
    range->set_maxreturned(4);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    ASSERT_EQ(4, command_response.body().range().keys_size());
    ASSERT_EQ("a", command_response.body().range().keys(0));
    ASSERT_EQ("b", command_response.body().range().keys(1));
    ASSERT_EQ("c", command_response.body().range().keys(2));
    ASSERT_EQ("d", command_response.body().range().keys(3));
}

TEST_F(MessageProcessorTest, GetKeyRangeHandlesExclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    char *the_value_a;
    char *the_value_b;
    char *the_value_c;
    char *the_value_d;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_c = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_d = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    memcpy(the_value_a, "foo", 3);
    memcpy(the_value_b, "foo", 3);
    memcpy(the_value_c, "foo", 3);
    memcpy(the_value_d, "foo", 3);
    IncomingBuffValue value_a(the_value_a, 3);
    IncomingBuffValue value_b(the_value_b, 3);
    IncomingBuffValue value_c(the_value_c, 3);
    IncomingBuffValue value_d(the_value_d, 3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("a", primary_store_value, &value_a, false, (token));
    primary_store_.Put("b", primary_store_value, &value_b, false, (token));
    primary_store_.Put("c", primary_store_value, &value_c, false, (token));
    primary_store_.Put("d", primary_store_value, &value_d, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("a");
    range->set_startkeyinclusive(false);
    range->set_endkey("d");
    range->set_endkeyinclusive(false);
    range->set_maxreturned(4);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    ASSERT_EQ(2, command_response.body().range().keys_size());
    ASSERT_EQ("b", command_response.body().range().keys(0));
    ASSERT_EQ("c", command_response.body().range().keys(1));
}

TEST_F(MessageProcessorTest, GetKeyRangeHandles0Limit) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    command.mutable_body()->mutable_range()->set_maxreturned(0);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    ASSERT_EQ(0, command_response.body().range().keys_size());
}

TEST_F(MessageProcessorTest, GetKeyRangeHandlesExcludedLimit) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    command.mutable_body()->mutable_range();
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    ASSERT_EQ(0, command_response.body().range().keys_size());
}

TEST_F(MessageProcessorTest, GetKeyRangeHandlesRequestOverLimit) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    command.mutable_body()->mutable_range()->set_maxreturned(1000);

    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
    EXPECT_EQ("Key limit exceeded.", command_response.status().statusmessage());
    ASSERT_EQ(0, command_response.body().range().keys_size());
}

TEST_F(MessageProcessorTest, RangeHandlesKeysInOneKSRForConstraintedUser) {
    ///If userid has been assigned an ACL definition for RANGE
    ///startkey && endkey must be confined to the same scope
    ///----------------
    ///User A Domains:
    /// -Domain1: RANGE, offset: 1, value: bc
    ///----------------

    /// Create Domain Definitions
    std::list<Domain> a_domains;
    //Domain(int offset, string value, permission_type, require_ssl)
    a_domains.push_back(Domain(1, "bc", Domain::kRange, false));

    /// Create User A
    //User(int64_t id, const std::string &key, const std::list<Domain> &domains);
    User range_user_a(1, "constrained user", a_domains);
    std::list<User> users;
    users.push_back(range_user_a);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));

    /// Put arbitrary values in the database
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    char *the_value_a;
    char *the_value_b;
    char *the_value_c;
    char *the_value_d;
    char *the_value_e;
    char *the_value_f;
    char *the_value_g;
    char *the_value_h;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_c = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_d = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_e = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_f = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_g = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_h = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    memcpy(the_value_a, "foo", 3);
    memcpy(the_value_b, "foo", 3);
    memcpy(the_value_c, "foo", 3);
    memcpy(the_value_d, "foo", 3);
    memcpy(the_value_e, "foo", 3);
    memcpy(the_value_f, "foo", 3);
    memcpy(the_value_g, "foo", 3);
    memcpy(the_value_h, "foo", 3);

    IncomingBuffValue value_a(the_value_a, 3);
    IncomingBuffValue value_b(the_value_b, 3);
    IncomingBuffValue value_c(the_value_c, 3);
    IncomingBuffValue value_d(the_value_d, 3);
    IncomingBuffValue value_e(the_value_e, 3);
    IncomingBuffValue value_f(the_value_f, 3);
    IncomingBuffValue value_g(the_value_g, 3);
    IncomingBuffValue value_h(the_value_h, 3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("ca", primary_store_value, &value_a, false, (token));
    primary_store_.Put("cbc", primary_store_value, &value_b, false, (token));
    primary_store_.Put("cbca", primary_store_value, &value_c, false, (token));
    primary_store_.Put("cbcd", primary_store_value, &value_d, false, (token));
    primary_store_.Put("cd", primary_store_value, &value_e, false, (token));
    primary_store_.Put("ce", primary_store_value, &value_f, false, (token));
    primary_store_.Put("cf", primary_store_value, &value_g, false, (token));
    primary_store_.Put("dbc", primary_store_value, &value_h, false, (token));
    //////////////////////////////////////////////////////////////////
    /// TEST pt1: Create Command GetKeyRange('cbc','cbcd')
    /// should succeed as the request is confined to 1 Key scope region
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("cbc");
    range->set_startkeyinclusive(true);
    range->set_endkey("cbcd");
    range->set_endkeyinclusive(true);
    range->set_maxreturned(4);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message, 1, "constrained user");

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                      &response_value, request_context, &time_handler,
                                      message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    /////////////////////////////////////////////////////////////////
    /// TEST pt2: Create Command GetKeyRange('ca','cd')
    /// should succeed as the request is confined to 1 Key scope region
    Message message2;
    Command command2;
    std::string serialized_command2;
    command2.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    Command_Range *range2 = command2.mutable_body()->mutable_range();
    range2->set_startkey("ca");
    range2->set_startkeyinclusive(true);
    range2->set_endkey("cd");
    range2->set_endkeyinclusive(true);
    range2->set_maxreturned(4);
    command2.SerializeToString(&serialized_command2);
    message2.set_commandbytes(serialized_command2);
    SetUserAndHmac(&message2, 1, "constrained user");

    Command command_response2;
    NullableOutgoingValue response_value2;
    RequestContext request_context2;
    ConnectionTimeHandler time_handler2;

    ConnectionRequestResponse *reqResp2 = new ConnectionRequestResponse();
    ProcessMessage(*reqResp2, &message2, command2, &empty_value_, &command_response2,
                                      &response_value2, request_context2, &time_handler2,
                                      message2.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response2.status().code());
    //////////////////////////////////////////////////////////////////
    /// TEST pt3: Create Command MediaScan('cbc','cbcd')
    /// should succeed as the request is confined to 1 Key scope region
    Message message3;
    Command command3;
    std::string serialized_command3;
    command3.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range3 = command3.mutable_body()->mutable_range();
    range3->set_startkey("cbc");
    range3->set_startkeyinclusive(true);
    range3->set_endkey("cbcd");
    range3->set_endkeyinclusive(true);
    range3->set_maxreturned(4);
    command3.SerializeToString(&serialized_command3);
    message3.set_commandbytes(serialized_command3);
    SetUserAndHmac(&message3, 1, "constrained user");

    Command command_response3;
    NullableOutgoingValue response_value3;
    RequestContext request_context3;
    ConnectionTimeHandler time_handler3;

    ConnectionRequestResponse *reqResp3 = new ConnectionRequestResponse();
    ProcessMessage(*reqResp3, &message3, command3, &empty_value_, &command_response3,
                                      &response_value3, request_context3, &time_handler3,
                                      message3.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response3.status().code());
    /////////////////////////////////////////////////////////////////
    /// TEST pt4: Create Command MediaScan('ca','cd')
    /// should succeed as the request is confined to 1 Key scope region
    Message message4;
    Command command4;
    std::string serialized_command4;
    command4.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range4 = command4.mutable_body()->mutable_range();
    range4->set_startkey("ca");
    range4->set_startkeyinclusive(true);
    range4->set_endkey("cd");
    range4->set_endkeyinclusive(true);
    range4->set_maxreturned(4);
    command4.SerializeToString(&serialized_command4);
    message4.set_commandbytes(serialized_command4);
    SetUserAndHmac(&message4, 1, "constrained user");

    Command command_response4;
    NullableOutgoingValue response_value4;
    RequestContext request_context4;
    ConnectionTimeHandler time_handler4;

    ConnectionRequestResponse *reqResp4 = new ConnectionRequestResponse();
    ProcessMessage(*reqResp4, &message4, command4, &empty_value_, &command_response4,
                                      &response_value4, request_context4, &time_handler4,
                                      message4.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response4.status().code());
}

TEST_F(MessageProcessorTest, RangeRejectsKeysInMoreThanOneKSRForConstraintedUser) {
    ///If userid has been assigned an ACL definition for RANGE
    ///startkey && endkey must be confined to the a continuous key scope region
    ///The scope must be applicable to the user's ACL constraints
    ///See @authorizer.cc method "AuthorizeKeyRange" for more details
    ///----------------
    ///User A Domains:
    /// -Domain1: RANGE, offset: 1, value: bc
    ///----------------

    /// Create Domain Definitions
    std::list<Domain> a_domains;
    //Domain(int offset, string value, permission_type, require_ssl)
    a_domains.push_back(Domain(1, "bc", Domain::kRange, false));

    /// Create User A
    //User(int64_t id, const std::string &key, const std::list<Domain> &domains);
    User range_user_a(1, "constrained user", a_domains);
    std::list<User> users;
    users.push_back(range_user_a);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));

    /// Put arbitrary values in the database
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    char *the_value_a;
    char *the_value_b;
    char *the_value_c;
    char *the_value_d;
    char *the_value_e;
    char *the_value_f;
    char *the_value_g;
    char *the_value_h;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_c = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_d = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_e = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_f = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_g = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    the_value_h = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    memcpy(the_value_a, "foo", 3);
    memcpy(the_value_b, "foo", 3);
    memcpy(the_value_c, "foo", 3);
    memcpy(the_value_d, "foo", 3);
    memcpy(the_value_e, "foo", 3);
    memcpy(the_value_f, "foo", 3);
    memcpy(the_value_g, "foo", 3);
    memcpy(the_value_h, "foo", 3);

    IncomingBuffValue value_a(the_value_a, 3);
    IncomingBuffValue value_b(the_value_b, 3);
    IncomingBuffValue value_c(the_value_c, 3);
    IncomingBuffValue value_d(the_value_d, 3);
    IncomingBuffValue value_e(the_value_e, 3);
    IncomingBuffValue value_f(the_value_f, 3);
    IncomingBuffValue value_g(the_value_g, 3);
    IncomingBuffValue value_h(the_value_h, 3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("ca", primary_store_value, &value_a, false, (token));
    primary_store_.Put("cbc", primary_store_value, &value_b, false, (token));
    primary_store_.Put("cbca", primary_store_value, &value_c, false, (token));
    primary_store_.Put("cbcd", primary_store_value, &value_d, false, (token));
    primary_store_.Put("cd", primary_store_value, &value_e, false, (token));
    primary_store_.Put("ce", primary_store_value, &value_f, false, (token));
    primary_store_.Put("cf", primary_store_value, &value_g, false, (token));
    primary_store_.Put("dbc", primary_store_value, &value_h, false, (token));
    //////////////////////////////////////////////////////////////////
    /// TEST pt1: Create Command GetKeyRange('cd','cf')
    /// Should be rejected as startkey and endkey does not enclose any key scope region
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("cd");
    range->set_startkeyinclusive(true);
    range->set_endkey("cf");
    range->set_endkeyinclusive(true);
    range->set_maxreturned(4);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message, 1, "constrained user");

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                      &response_value, request_context, &time_handler,
                                      message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
    //////////////////////////////////////////////////////////////////
    /// TEST pt2: Create Command GetKeyRange('cbc','dbc')
    /// Should be rejected as End key is in another key scope region
    Message message2;
    Command command2;
    std::string serialized_command2;
    command2.mutable_header()->set_messagetype(Command_MessageType_GETKEYRANGE);
    Command_Range *range2 = command2.mutable_body()->mutable_range();
    range2->set_startkey("cbc");
    range2->set_startkeyinclusive(true);
    range2->set_endkey("dbc");
    range2->set_endkeyinclusive(true);
    range2->set_maxreturned(4);
    command2.SerializeToString(&serialized_command2);
    message2.set_commandbytes(serialized_command2);
    SetUserAndHmac(&message2, 1, "constrained user");

    Command command_response2;
    NullableOutgoingValue response_value2;
    RequestContext request_context2;
    ConnectionTimeHandler time_handler2;

    ConnectionRequestResponse *reqResp2 = new ConnectionRequestResponse();
    ProcessMessage(*reqResp2, &message2, command2, &empty_value_, &command_response2,
                                      &response_value2, request_context2, &time_handler2,
                                      message2.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response2.status().code());

    //////////////////////////////////////////////////////////////////
    /// TEST pt3: Create Command MediaScan('cd','cf')
    /// Should be rejected as startkey and endkey does not enclose any key scope region
    Message message3;
    Command command3;
    std::string serialized_command3;
    command3.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range3 = command3.mutable_body()->mutable_range();
    range3->set_startkey("cd");
    range3->set_startkeyinclusive(true);
    range3->set_endkey("cf");
    range3->set_endkeyinclusive(true);
    range3->set_maxreturned(4);
    command3.SerializeToString(&serialized_command3);
    message3.set_commandbytes(serialized_command3);
    SetUserAndHmac(&message3, 1, "constrained user");

    Command command_response3;
    NullableOutgoingValue response_value3;
    RequestContext request_context3;
    ConnectionTimeHandler time_handler3;

    ConnectionRequestResponse *reqResp3 = new ConnectionRequestResponse();
    ProcessMessage(*reqResp3, &message3, command3, &empty_value_, &command_response3,
                                      &response_value3, request_context3, &time_handler3,
                                      message3.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response3.status().code());
    /////////////////////////////////////////////////////////////////
    /// TEST pt4: Create Command MediaScan('cbc','dbc')
    /// Should be rejected as End key is in another key scope region
    Message message4;
    Command command4;
    std::string serialized_command4;
    command4.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range4 = command4.mutable_body()->mutable_range();
    range4->set_startkey("cbc");
    range4->set_startkeyinclusive(true);
    range4->set_endkey("dbc");
    range4->set_endkeyinclusive(true);
    range4->set_maxreturned(4);
    command4.SerializeToString(&serialized_command4);
    message4.set_commandbytes(serialized_command4);
    SetUserAndHmac(&message4, 1, "constrained user");

    Command command_response4;
    NullableOutgoingValue response_value4;
    RequestContext request_context4;
    ConnectionTimeHandler time_handler4;

    ConnectionRequestResponse *reqResp4 = new ConnectionRequestResponse();
    ProcessMessage(*reqResp4, &message4, command4, &empty_value_, &command_response4,
                                      &response_value4, request_context4, &time_handler4,
                                      message4.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response4.status().code());
}

TEST_F(MessageProcessorTest, SetupRejectsCallWithWrongClusterVersion) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SETUP);
    command.mutable_body()->mutable_setup()->set_setupoptype(Command_Setup_SetupOpType_CLUSTER_VERSION_SETUPOP);
    command.mutable_body()->mutable_setup()->set_newclusterversion(99);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    EXPECT_CALL(mock_cluster_version_store_, GetClusterVersion()).WillOnce(Return(55));
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_VERSION_FAILURE, command_response.status().code());
    EXPECT_EQ(55, command_response.header().clusterversion());
}

TEST_F(MessageProcessorTest, SecurityPopulatesUserStore) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_ACL_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    Command_Security_ACL *acl = security->add_acl();
    acl->set_identity(5);
    acl->set_key("secret key");
    acl->set_hmacalgorithm(Command_Security_ACL_HMACAlgorithm_HmacSHA1);
    Command_Security_ACL_Scope *domain = acl->add_scope();
    domain->set_offset(12);
    domain->set_value("xxbb");
    domain->add_permission(Command_Security_ACL_Permission_READ);
    domain->set_tlsrequired(true);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    User user;
    ASSERT_TRUE(user_store_.Get(5, &user));
    EXPECT_EQ("secret key", user.key());
    ASSERT_EQ(1u, user.domains().size());
    EXPECT_EQ(12u, user.domains().front().offset());
    EXPECT_EQ("xxbb", user.domains().front().value());
    EXPECT_TRUE(user.domains().front().tls_required());
    // This static_cast is necessary here to work around an obscure issue
    // involving the fact that we would otherwise be passing a static const
    // integral type by reference.
    EXPECT_EQ(static_cast<role_t>(Domain::kRead), user.domains().front().roles());
}

TEST_F(MessageProcessorTest, SecurityReturnsErrorForUnrecognizedHmacAlgorithm) {
    // If we neglect to specify an HMAC algorithm, we should get an error. This
    // mimics the situation where a hypothetical future client with an updated
    // protocol buffer definition tries to use an HMAC algorithm that the
    // current server code does not recognize.
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_ACL_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    Command_Security_ACL *acl = security->add_acl();
    acl->set_identity(5);
    acl->set_key("secret key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_NO_SUCH_HMAC_ALGORITHM, command_response.status().code());
}

TEST_F(MessageProcessorTest, SecurityReturnsErrorForInvalidRole) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_ACL_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    Command_Security_ACL *acl = security->add_acl();
    acl->set_identity(5);
    acl->set_key("secret key");
    acl->set_hmacalgorithm(proto::Command_Security_ACL_HMACAlgorithm_HmacSHA1);
    Command_Security_ACL_Scope *domain = acl->add_scope();
    domain->add_permission(proto::Command_Security_ACL_Permission_INVALID_PERMISSION);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
    EXPECT_EQ("Permission is invalid in acl", command_response.status().statusmessage());
}


TEST_F(MessageProcessorTest, SecurityReturnsErrorForNoRoles) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_ACL_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    Command_Security_ACL *acl = security->add_acl();
    acl->set_identity(5);
    acl->set_key("secret key");
    acl->set_hmacalgorithm(proto::Command_Security_ACL_HMACAlgorithm_HmacSHA1);
    acl->add_scope();
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
    EXPECT_EQ("No permission set in acl", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, SecurityReturnsInvalidForPinOverLimit) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_LOCK_PIN_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    security->set_oldlockpin("123");
    security->set_newlockpin("1234567");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
    EXPECT_EQ("Pin provided is too long", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, SecurityReturnsSuccessForPinEqualToLimit) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_LOCK_PIN_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    security->set_oldlockpin("123");
    security->set_newlockpin("12345");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
}

TEST_F(MessageProcessorTest, SecurityReturnsSuccessForPinUnderLimit) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_SECURITY);
    command.mutable_body()->mutable_security()->set_securityoptype(Command_Security_SecurityOpType_LOCK_PIN_SECURITYOP);
    Command_Security *security = command.mutable_body()->mutable_security();
    security->set_oldlockpin("123");
    security->set_newlockpin("123");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    ASSERT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
}

TEST_F(MessageProcessorTest, ProcessMessageDoesntSetAckSequenceIfNoSequencePresent) {
    Message message;
    Command command;
    std::string serialized_command;
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response, &response_value,
        request_context, &time_handler, message.hmacauth().identity(), connection_id);
    EXPECT_FALSE(command_response.header().has_acksequence());
}

// Authorization tests

TEST_F(MessageProcessorTest, GetReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, Get(42, "key", _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    //message_processor_with_mocks_.
    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(MessageProcessorTest, GetVersionReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETVERSION);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, GetVersion(42, "key", _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(MessageProcessorTest, GetNextReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETNEXT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, GetNext(42, "key", _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(MessageProcessorTest, GetPreviousReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETPREVIOUS);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, GetPrevious(42, "key", _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    //message_processor_with_mocks_.
    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(MessageProcessorTest, PutReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_body()->mutable_keyvalue()->set_tag("tag");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, Put(42, "key", "", _, _, _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    Command command_response;
    IncomingStringValue value("");
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &value,
            &command_response, &response_value, request_context, &time_handler,
            message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(MessageProcessorTest, DeleteReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_DELETE);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, Delete(42, "key", "", _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(MessageProcessorTest, GetReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, Get(42, "key", _, _, _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
    &response_value, request_context, &time_handler, message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, GetVersionReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->
            set_messagetype(proto::Command_MessageType_GETVERSION);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, GetVersion(42, "key", _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, GetNextReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->
            set_messagetype(proto::Command_MessageType_GETNEXT);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, GetNext(42, "key", _, _, _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, GetPrevReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->
            set_messagetype(proto::Command_MessageType_GETPREVIOUS);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, GetPrevious(42, "key", _, _, _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, PutReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(proto::Command_MessageType_PUT);
    command.mutable_body()->mutable_keyvalue()->set_tag("foo");
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, Put(42, _, _, _, _, _, _, _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, DeleteReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(proto::Command_MessageType_DELETE);
    #ifdef QOS_ENABLED
    command.mutable_body()->mutable_keyvalue()->add_prioritizedqos(Command_QoS_EFFICIENT_FILL);
    #endif
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_skinny_waist_, Delete(42, _, _, _, _, _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, GetKeyRangeReturnsDetailedErrorForCorruption) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->
            set_messagetype(proto::Command_MessageType_GETKEYRANGE);
    command.mutable_body()->mutable_range()->set_maxreturned(1);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    EXPECT_CALL(mock_authorizer_, AuthorizeKeyRange(42, _, _, _, _, _, _))
                .WillOnce(Return(AuthorizationStatus_SUCCESS));

    EXPECT_CALL(mock_skinny_waist_, GetKeyRange(42, _, _, _, _, _, _, _, _))
                .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMockMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                                 &response_value, request_context, &time_handler,
                                                 message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INTERNAL_ERROR, command_response.status().code());
    EXPECT_EQ("Internal DB corruption", command_response.status().statusmessage());
}

TEST_F(MessageProcessorTest, MediaScanTimeoutStatusSetCorrectly) {
    /// Put arbitrary values in the database
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(3);
    memcpy(the_value, "foo", 3);
    IncomingBuffValue value_a(the_value, 3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("000Foo", primary_store_value, &value_a, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("000Foo");
    range->set_startkeyinclusive(false);
    range->set_endkey("00Foo9");
    range->set_endkeyinclusive(false);
    range->set_maxreturned(4);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    time_handler.SetExpired();
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                      &response_value, request_context, &time_handler,
                                      message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_EXPIRED, command_response.status().code());
}

TEST_F(MessageProcessorTest, MediaScanInterruptStatusSetCorrectly) {
    /// Put arbitrary values in the database
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_a, "foo", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    primary_store_.Put("000Foo", primary_store_value, &value_a, false, (token));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("000Foo");
    range->set_startkeyinclusive(true);
    range->set_endkey("00Foo9");
    range->set_endkeyinclusive(false);
    range->set_maxreturned(4);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    time_handler.SetInterrupt();
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                      &response_value, request_context, &time_handler,
                                      message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_FALSE(command_response.mutable_body()->mutable_range()->startkeyinclusive());
}

TEST_F(MessageProcessorTest, MediaScanRejectsReverse) {
    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_header()->set_messagetype(Command_MessageType_MEDIASCAN);
    Command_Range *range = command.mutable_body()->mutable_range();
    range->set_startkey("000Foo");
    range->set_endkey("00Foo9");
    range->set_reverse(true);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    ConnectionTimeHandler time_handler;
    int64_t connection_id = 0;

    ConnectionRequestResponse *reqResp = new ConnectionRequestResponse();
    ProcessMessage(*reqResp, &message, command, &empty_value_, &command_response,
                                      &response_value, request_context, &time_handler,
                                      message.hmacauth().identity(), connection_id);

    EXPECT_EQ(Command_Status_StatusCode_INVALID_REQUEST, command_response.status().code());
}

} // namespace kinetic
} // namespace seagate
} // namespace com
