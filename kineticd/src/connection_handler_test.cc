#include "gtest/gtest.h"
#include "connection_handler.h"
#include "value_factory.h"
#include "hmac_provider.h"
#include "hmac_authenticator.h"
#include "domain.h"

#include "mock_message_processor.h"
#include "mock_user_store.h"
#include "mock_incoming_value.h"
#include "mock_server.h"

using ::testing::DoAll;
using ::testing::SaveArg;
using namespace com::seagate::kinetic::proto; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::MessageStreamFactory;

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

class ConnectionHandlerTest : public ::testing::Test {
    protected:
    ConnectionHandlerTest():
                message_processor_(),
                value_factory_(),
                message_stream_factory_(ssl_context_, value_factory_),
                limits_(4000, 128, 2048, 1024 * 1024, 1024 * 1024,
                    100, 30, 20, 200, 1000, 5, 64*1024*1024, 24000),
                user_store_(),
                authenticator_(user_store_, hmac_provider_),
                server_(),
                connection_handler_(authenticator_,
                    message_processor_,
                    message_stream_factory_,
                    profiler_,
                    limits_,
                    user_store_,
                    999,
                    statistics_manager_) {}
    HmacProvider hmac_provider_;
    MockMessageProcessor message_processor_;
    SSL_CTX *ssl_context_;
    ValueFactory value_factory_;
    MessageStreamFactory message_stream_factory_;
    Profiler profiler_;
    Limits limits_;
    MockUserStore user_store_;
    HmacAuthenticator authenticator_;
    MockServer server_;
    StatisticsManager statistics_manager_;
    ConnectionHandler connection_handler_;
};

//##################### Validate Message tests ##################################

////////////////////////// Failing auth ///////////////////////////////////////////

TEST_F(ConnectionHandlerTest, CommandLacksAuthTypeShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandHasUnKnownAuthTypeShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to an unknowntype
    connection_request_response->request()->set_authtype(proto::Message_AuthType_INVALID_AUTH_TYPE);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithUnsolicitedAuthTypeShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to an unsolicited
    connection_request_response->request()->set_authtype(proto::Message_AuthType_UNSOLICITEDSTATUS);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandPinAuthTypeNotOnTlsPortShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to pin
    connection_request_response->request()->set_authtype(proto::Message_AuthType_PINAUTH);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandHMACAuthWithoutHMACShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to HMAC
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_HMACAUTH);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_HMAC_FAILURE);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandMACAUTHWithHMACWithoutIdentityShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to HMAC
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    connection_request_response->
                request()->mutable_hmacauth()->set_hmac(
        hmac_provider_.ComputeHmac(*(connection_request_response->request()),
                                   "super secret"));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_HMAC_FAILURE);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandHMACAUTHWithIdentityWithoutMACShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to HMAC
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    connection_request_response->
                request()->mutable_hmacauth()->set_identity(42);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_HMAC_FAILURE);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandHMACAUTHWithMisMatchIdentityShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to HMAC
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    connection_request_response->
                request()->mutable_hmacauth()->set_identity(42);
    connection_request_response->
                request()->mutable_hmacauth()->set_hmac(
        hmac_provider_.ComputeHmac(*(connection_request_response->request()),
                                   "super secret"));

    EXPECT_CALL(user_store_, Get(_, _))
        .WillOnce(Return(false));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_HMAC_FAILURE);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandHMACAUTHWithMisMatchHMACShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;
    // Setting up domains for user
    std::list<Domain> domains;
    domains.push_back(Domain(0, "", Domain::kAllRoles, false));
    User demo_user(30, "my key", domains);

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to HMAC
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    connection_request_response->
                request()->mutable_hmacauth()->set_identity(42);
    connection_request_response->
                request()->mutable_hmacauth()->set_hmac(
        hmac_provider_.ComputeHmac(*(connection_request_response->request()),
                                   "super secret"));

    EXPECT_CALL(user_store_, Get(_, _))
        .WillOnce(DoAll(
            SetArgPointee<1>(demo_user),
            Return(true)));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_HMAC_FAILURE);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

////////////////////////// Passing auth ///////////////////////////////////////////

TEST_F(ConnectionHandlerTest, CommandWithPinAuthTypeAndTLSPortShouldPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);
    connection->SetUseSSL(true);
    MockIncomingValue request_value;

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to pin
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_PINAUTH);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandHMACAUTHWithHMACAndIdentiyShouldPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);
    MockIncomingValue request_value;
    // Setting up domains for user
    std::list<Domain> domains;
    domains.push_back(Domain(0, "", Domain::kAllRoles, false));
    User demo_user(42, "super secret", domains);

    // Set server
    connection_handler_.SetServer(&server_);

    // Set request value
    connection_request_response->SetRequestValue(&request_value);
    // Set authtype of message to HMAC
    connection_request_response->
                request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    connection_request_response->
                request()->mutable_hmacauth()->set_identity(42);
    connection_request_response->
                request()->mutable_hmacauth()->set_hmac(
        hmac_provider_.ComputeHmac(*(connection_request_response->request()),
                                   "super secret"));

    EXPECT_CALL(user_store_, Get(_, _))
        .WillOnce(DoAll(
            SetArgPointee<1>(demo_user),
            Return(true)));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateMessage(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_HMAC_FAILURE);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

//##################### Validate Command tests ##################################
////////////////////////// Failing auth /////////////////////////////////////////
TEST_F(ConnectionHandlerTest, CommandWithoutMessageTypeShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();

    // Set server
    connection_handler_.SetServer(&server_);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandNotSupportedInStateShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();

    // Set server
    connection_handler_.SetServer(&server_);

    // Set message type to Get
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithSecurityTypeNotOnTLSShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();

    // Set server
    connection_handler_.SetServer(&server_);

    // Set message type to security
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_SECURITY);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandMissingConnectionIDShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();

    // Set server
    connection_handler_.SetServer(&server_);

    // Set message type to get
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithBadConnectionIDShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);

    // Set message type to get
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    // Missmatch of ids
    connection_request_response->
                command()->mutable_header()->set_connectionid(30);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithBadSequenceIDShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(12);

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithValueSizeGreaterThanLimitShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(&request_value);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    // Size of request value
    EXPECT_CALL(request_value, size())
        .WillOnce(Return(1024*1024+1));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithNewVersionSizeGreaterThanLimitShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(&request_value);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    // Create version that will exceed length
    std::string newversion;
    for (int i = 0; i < 2049; i++) {
        newversion += "x";
    }
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion(newversion);

    // Size of request value
    EXPECT_CALL(request_value, size())
        .WillOnce(Return(1024*1024));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithVersionSizeGreaterThanLimitShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(&request_value);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    // Create version that will exceed length
    std::string version;
    for (int i = 0; i < 2049; i++) {
        version += "x";
    }
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_dbversion(version);

    // Size of request value
    EXPECT_CALL(request_value, size())
        .WillOnce(Return(1024*1024));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithKeySizeGreaterThanLimitShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(&request_value);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Create version that will exceed length
    std::string key;
    for (int i = 0; i < 4001; i++) {
        key += "x";
    }
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key(key);

    // Size of request value
    EXPECT_CALL(request_value, size())
        .WillOnce(Return(1024*1024));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, CommandWithTagSizeGreaterThanLimitShouldFail) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    MockIncomingValue request_value;

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(&request_value);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Generate tag that will exceed limit
    std::string tag;
    for (int i = 0; i < 129; i++) {
        tag += "x";
    }
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag(tag);

    // Size of request value
    EXPECT_CALL(request_value, size())
        .WillOnce(Return(1024*1024));

    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_ERROR);
    ASSERT_EQ(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

////////////////////////// Passing auth ///////////////////////////////////////////

TEST_F(ConnectionHandlerTest, ValidGetCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to get and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GET);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");
    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidPutCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);
    MockIncomingValue request_value;
    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(&request_value);

    // Set message type to put and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_PUT);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");
    // Set Synchronization
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->
        set_synchronization(proto::Command_Synchronization_WRITEBACK);
    // Size of request value
    EXPECT_CALL(request_value, size())
        .WillOnce(Return(1024*1024));
    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    connection_request_response->SetRequestValue(NULL);
    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidDeleteCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);
    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to delete and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_DELETE);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");
    // Set Synchronization
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->
        set_synchronization(proto::Command_Synchronization_WRITEBACK);
    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                        command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidGetNextCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to get next and set id
    connection_request_response->command()->
                mutable_header()->set_messagetype(proto::Command_MessageType_GETNEXT);
    connection_request_response->command()->
                mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->command()->
                mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->command()->
                mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->command()->
                mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->command()->
                mutable_body()->mutable_keyvalue()->set_tag("tag");
    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidGetPreviousCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to get previous and set id
    connection_request_response->
                command()->mutable_header()->
                set_messagetype(proto::Command_MessageType_GETPREVIOUS);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidGetKeyRangeCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to getkeyrange and set id
    connection_request_response->
                command()->mutable_header()->
                set_messagetype(proto::Command_MessageType_GETKEYRANGE);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");
    // Set range
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_startkey("a");
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_startkeyinclusive(true);
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_endkey("d");
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_endkeyinclusive(true);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidGetVersionCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to get version and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GETVERSION);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");
    // Set range
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_startkey("a");
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_startkeyinclusive(true);
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_endkey("d");
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_endkeyinclusive(true);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidSetUpCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to setup and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_SETUP);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidGetLogCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to getlog and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_GETLOG);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidSecurityCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);
    connection->SetUseSSL(true);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to security and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_SECURITY);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidNOOPCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to no op and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_NOOP);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidFlushAllDataCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to flush all and set id
    connection_request_response->
                command()->mutable_header()->
                set_messagetype(proto::Command_MessageType_FLUSHALLDATA);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidPINOPCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to pin op and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_PINOP);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

TEST_F(ConnectionHandlerTest, ValidMEDIASCANCommandPass) {
    Connection *connection = new Connection(0);
    ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
    connection_request_response->command()->mutable_header()->set_priority(Command_Priority::Command_Priority_NORMAL);

    // Set server
    connection_handler_.SetServer(&server_);

    connection->SetId(42);
    connection->SetLastMsgSeq(12);
    connection_request_response->SetRequestValue(NULL);

    // Set message type to pin op and set id
    connection_request_response->
                command()->mutable_header()->set_messagetype(proto::Command_MessageType_MEDIASCAN);
    connection_request_response->
                command()->mutable_header()->set_connectionid(42);

    // Set Seq ID
    connection_request_response->
                command()->mutable_header()->set_sequence(13);
    // Set key
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_key("key");
    // Set version
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_newversion("version");
    // Set tag
    connection_request_response->
                command()->mutable_body()->mutable_keyvalue()->set_tag("tag");
    // Set range
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_startkey("0");
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_startkeyinclusive(true);
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_endkey("1");
    connection_request_response->
                command()->mutable_body()->mutable_range()->set_endkeyinclusive(true);

    connection_request_response->request()->set_authtype(proto::Message_AuthType_HMACAUTH);
    User user;
    user.maxPriority(Command_Priority_HIGHEST);
    EXPECT_CALL(user_store_, Get(_, _)).WillOnce(DoAll(SetArgPointee<1>(user), Return(true)));
    ConnectionStatusCode connection_state;
    connection_state = connection_handler_.ValidateCommand(connection, connection_request_response);
    ASSERT_EQ(connection_state, ConnectionStatusCode::CONNECTION_OK);
    ASSERT_EQ(connection->lastMsgSeq(), connection_request_response->
                                                    command()->mutable_header()->sequence());
    ASSERT_NE(connection_request_response->
                          response_command()->mutable_status()->code(),
              proto::Command_Status_StatusCode_INVALID_REQUEST);

    // Clean up
    delete connection;
    delete connection_request_response;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
