#include "gtest/gtest.h"

#include "getlog_handler.h"
#include "mock_device_information.h"
#include "hmac_provider.h"
#include "outgoing_value.h"
#include "version_info.h"
#include "std_map_key_value_store.h"
#include "command_line_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_HMAC_FAILURE;
using proto::Command_Status_StatusCode_NO_SPACE;
using proto::Command_Status_StatusCode_NO_SUCH_HMAC_ALGORITHM;
using proto::Command_Status_StatusCode_NOT_AUTHORIZED;
using proto::Command_Status_StatusCode_NOT_FOUND;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;
using proto::Command_Status_StatusCode_INVALID_REQUEST;
using proto::Command_Status_StatusCode_VERSION_MISMATCH;
using proto::Command_Status_StatusCode_VERSION_FAILURE;
using proto::Command_MessageType_GET;
using proto::Command_MessageType_PUT;
using proto::Command_MessageType_GETLOG;
using proto::Command_MessageType_GETLOG_RESPONSE;
using proto::Command_MessageType_GETVERSION;
using proto::Command_GetLog_Type_UTILIZATIONS;
using proto::Command_GetLog_Type_TEMPERATURES;
using proto::Command_GetLog_Type_CAPACITIES;
using proto::Command_GetLog_Type_CONFIGURATION;
using proto::Command_GetLog_Type_STATISTICS;
using proto::Command_GetLog_Type_MESSAGES;
using proto::Command_GetLog_Type_LIMITS;
using proto::Command_GetLog_Type_DEVICE;

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::NiceMock;

class GetLogHandlerTest : public ::testing::Test {
    protected:
    GetLogHandlerTest():
            db_(),
            device_information_(),
            limits_(100, 100, 100, 1024, 1024, 10, 10, 10, 200, 100, 5, 64*1024*1024, 24000),
            get_log_handler_(db_,
                    device_information_,
                    network_interfaces_,
                    123,
                    456,
                    limits_,
                    statistics_manager_)
            { }

    StdMapKeyValueStore db_;
    MockDeviceInformation device_information_;
    MockNetworkInterfaces network_interfaces_;
    Limits limits_;
    GetLogHandler get_log_handler_;
    StatisticsManager statistics_manager_;

    void SetUserAndHmac(Message *message) {
        HmacProvider hmac_provider;
        message->mutable_hmacauth()->set_identity(42);
        message->mutable_hmacauth()->set_hmac(hmac_provider.ComputeHmac(*message, "super secret"));
    }

    virtual void AssertEqual(const std::string &s, const NullableOutgoingValue &value) {
        std::string value_string;
        int err;
        ASSERT_TRUE(value.ToString(&value_string, &err));
        ASSERT_EQ(s, value_string);
    }
};



TEST_F(GetLogHandlerTest, GetLogReturnsUtilizationIfRequested) {
    Command command;
    Message message;
    command.mutable_header()->
    set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_UTILIZATIONS);
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetHdaUtilization(_))
        .WillOnce(DoAll(SetArgPointee<0>(0.10), Return(true)));
    EXPECT_CALL(device_information_, GetEn0Utilization(_))
        .WillOnce(DoAll(SetArgPointee<0>(0.20), Return(true)));
    EXPECT_CALL(device_information_, GetEn1Utilization(_))
        .WillOnce(DoAll(SetArgPointee<0>(0.25), Return(true)));
    EXPECT_CALL(device_information_, GetCpuUtilization(_))
        .WillOnce(DoAll(SetArgPointee<0>(0.30), Return(true)));

    // Open 3 Connections
    for (int i = 0; i < 3; i++) {
        statistics_manager_.IncrementOpenConnections();
    }

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ASSERT_EQ(5, command_response.body().getlog().utilizations_size());

    EXPECT_EQ("HDA", command_response.body().getlog().utilizations(0).name());
    EXPECT_EQ(0.10f, command_response.body().getlog().utilizations(0).value());

    EXPECT_EQ("EN0", command_response.body().getlog().utilizations(1).name());
    EXPECT_EQ(0.20f, command_response.body().getlog().utilizations(1).value());

    EXPECT_EQ("EN1", command_response.body().getlog().utilizations(2).name());
    EXPECT_EQ(0.25f, command_response.body().getlog().utilizations(2).value());

    EXPECT_EQ("CPU", command_response.body().getlog().utilizations(3).name());
    EXPECT_EQ(0.70f, command_response.body().getlog().utilizations(3).value());

    EXPECT_EQ("Connections", command_response.body().getlog().utilizations(4).name());
    EXPECT_EQ(0.30f, command_response.body().getlog().utilizations(4).value());
}

TEST_F(GetLogHandlerTest, GetLogReturnsCapacityIfRequested) {
    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_CAPACITIES);
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetNominalCapacityInBytes())
        .WillOnce(Return(1000));
    EXPECT_CALL(device_information_, GetPortionFull(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(0.5),
            Return(true)));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    EXPECT_EQ(1000U, command_response.body().getlog().capacity()
        .nominalcapacityinbytes());
    EXPECT_EQ((float) 0.5, command_response.body().getlog().capacity().portionfull());
}

TEST_F(GetLogHandlerTest, GetLogReturnsTemperatureIfRequested) {
    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_TEMPERATURES);
    SetUserAndHmac(&message);

    std::map<std::string, smart_values> smart_attributes;
    struct smart_values hda_temp_values;
    hda_temp_values.value = 30;
    hda_temp_values.worst = 100;
    hda_temp_values.threshold = 25;
    smart_attributes["Temperature_Celsius"] = hda_temp_values;

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetSMARTAttributes(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(smart_attributes),
            Return(true)));
    EXPECT_CALL(device_information_, GetCpuTemp(_, _, _, _))
        .WillOnce(DoAll(
            SetArgPointee<0>(20.0),
            SetArgPointee<1>(21.0),
            SetArgPointee<2>(22.0),
            SetArgPointee<3>(23.0),
            Return(true)));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ASSERT_EQ(2, command_response.body().getlog().temperatures_size());

    EXPECT_EQ("HDA", command_response.body().getlog().temperatures(0).name());
    EXPECT_EQ(30.0f, command_response.body().getlog().temperatures(0).current());
    EXPECT_EQ(0.0f, command_response.body().getlog().temperatures(0).minimum());
    EXPECT_EQ(100.0f, command_response.body().getlog().temperatures(0).maximum());
    EXPECT_EQ(25.0f, command_response.body().getlog().temperatures(0).target());

    EXPECT_EQ("CPU", command_response.body().getlog().temperatures(1).name());
    EXPECT_EQ(20.0f, command_response.body().getlog().temperatures(1).current());
    EXPECT_EQ(21.0f, command_response.body().getlog().temperatures(1).minimum());
    EXPECT_EQ(22.0f, command_response.body().getlog().temperatures(1).maximum());
    EXPECT_EQ(0.0f, command_response.body().getlog().temperatures(1).target());
}

TEST_F(GetLogHandlerTest, GetLogReturnsConfigurationsIfRequested) {
    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_CONFIGURATION);
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetDriveIdentification(_, _, _, _))
        .WillOnce(DoAll(
            SetArgPointee<0>("Drive WWN"),
            SetArgPointee<1>("Drive SN"),
            SetArgPointee<2>("Seagate"),
            SetArgPointee<3>("Drive Model"),
            Return(true)));

    DeviceNetworkInterface eth1;
    eth1.name = "eth1";
    eth1.mac_address = "00:00:00:00:00";
    eth1.ipv4 = "127.0.0.1";
    eth1.ipv6 = "::1";

    DeviceNetworkInterface eth2;
    eth2.name = "eth2";
    eth2.mac_address = "00:00:00:00:01";
    eth2.ipv4 = "127.0.0.2";
    eth2.ipv6 = "::2";

    std::vector<DeviceNetworkInterface> interfaces;
    interfaces.push_back(eth1);
    interfaces.push_back(eth2);
    EXPECT_CALL(network_interfaces_, GetExternallyVisibleNetworkInterfaces(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(interfaces),
            Return(true)));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ASSERT_TRUE(command_response.body().getlog().has_configuration());

    EXPECT_EQ("Seagate", command_response.body().getlog().configuration().vendor());
    EXPECT_EQ("Drive Model", command_response.body().getlog().configuration().model());

    EXPECT_EQ("Drive SN", command_response.body().getlog().configuration().serialnumber());
    EXPECT_EQ("Drive WWN", command_response.body().getlog().configuration().worldwidename());

    EXPECT_TRUE(command_response.body().getlog().configuration().has_version());

    EXPECT_EQ(123U, command_response.body().getlog().configuration().port());
    EXPECT_EQ(456U, command_response.body().getlog().configuration().tlsport());

    ASSERT_EQ(2, command_response.body().getlog().configuration().interface_size());

    EXPECT_EQ("eth1", command_response.body().getlog().configuration().interface(0).name());
    EXPECT_EQ("00:00:00:00:00",
    command_response.body().getlog().configuration().interface(0).mac());
    EXPECT_EQ("127.0.0.1",
    command_response.body().getlog().configuration().interface(0).ipv4address());
    EXPECT_EQ("::1",
    command_response.body().getlog().configuration().interface(0).ipv6address());

    EXPECT_EQ("eth2",
    command_response.body().getlog().configuration().interface(1).name());
    EXPECT_EQ("00:00:00:00:01",
    command_response.body().getlog().configuration().interface(1).mac());
    EXPECT_EQ("127.0.0.2",
    command_response.body().getlog().configuration().interface(1).ipv4address());
    EXPECT_EQ("::2",
    command_response.body().getlog().configuration().interface(1).ipv6address());
    EXPECT_EQ(CURRENT_PROTOCOL_VERSION,
    command_response.body().getlog().configuration().protocolversion());
}

TEST_F(GetLogHandlerTest, GetLogReturnsStatisticsIfRequested) {
    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));

    statistics_manager_.IncrementOperationCount(Command_MessageType_GETVERSION);
    statistics_manager_.IncrementByteCount(Command_MessageType_GETVERSION, 1234);

    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_STATISTICS);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ASSERT_EQ(41, command_response.body().getlog().statistics_size());

    EXPECT_EQ(Command_MessageType_GETVERSION,
            command_response.body().getlog().statistics(14).messagetype());
    EXPECT_EQ(1U, command_response.body().getlog().statistics(14).count());
    EXPECT_EQ(1234U, command_response.body().getlog().statistics(14).bytes());
}

TEST_F(GetLogHandlerTest, GetLogReturnsMessagesIfRequested) {
    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));

    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_MESSAGES);
    SetUserAndHmac(&message);

    ::tm time;
    time.tm_year = 113;
    time.tm_mon = 10;
    time.tm_mday = 19;
    time.tm_hour = 19;
    time.tm_min = 45;
    time.tm_sec = 44;
    LogRingBuffer::Instance()->push(
        ::google::GLOG_INFO, "full", "base", 22, &time, "message", 4, 3);
    time.tm_sec = 48;
    LogRingBuffer::Instance()->push(
        ::google::GLOG_INFO, "full2", "base2", 33, &time, "mes2age", 4, 4);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ::std::string expected;
    expected += "2013-11-19T19:45:44Z INFO 3 base:22 mess\n";
    expected += "2013-11-19T19:45:48Z INFO 4 base2:33 mes2\n";

    EXPECT_EQ(expected, command_response.body().getlog().messages());
}

TEST_F(GetLogHandlerTest, GetLogReturnsDeviceSpecificSMARTAttributesIfRequested) {
    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_DEVICE);
    command.mutable_body()->mutable_getlog()->mutable_device()->
    set_name("com.Seagate.Kinetic.HDD.Gen1");
    SetUserAndHmac(&message);

    std::map<std::string, smart_values> smart_attributes;
    struct smart_values attribute_values;
    attribute_values.value = 100;
    attribute_values.worst = 100;
    attribute_values.threshold = 100;

    smart_attributes["Start_Stop_Count"] = attribute_values;
    smart_attributes["Power_On_Hours"] = attribute_values;
    smart_attributes["Power_Cycle_Count"] = attribute_values;
    smart_attributes["Spin_Up_Time"] = attribute_values;
    smart_attributes["Power-Off_Retract_Count"] = attribute_values;
    smart_attributes["Load_Cycle_Count"] = attribute_values;

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetSMARTAttributes(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(smart_attributes),
            Return(true)));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    std::stringstream expectedStream;
    expectedStream << "{\"Load_Cycle_Count\":" <<
        "[{\"threshold\":100,\"value\":100,\"worst\":100}]," <<
        "\"Power-Off_Retract_Count\":[{\"threshold\":100,\"value\":100,\"worst\":100}]," <<
        "\"Power_Cycle_Count\":[{\"threshold\":100,\"value\":100,\"worst\":100}]," <<
        "\"Power_On_Hours\":[{\"threshold\":100,\"value\":100,\"worst\":100}]," <<
        "\"Spin_Up_Time\":[{\"threshold\":100,\"value\":100,\"worst\":100}]," <<
        "\"Start_Stop_Count\":[{\"threshold\":100,\"value\":100,\"worst\":100}]}\n";
    std::string expected = expectedStream.str();
    AssertEqual(expected, response_value);
}

TEST_F(GetLogHandlerTest, GetLogReturnsAppropriateMessageIfNotSupportedDeviceIsRequested) {
    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_DEVICE);
    command.mutable_body()->mutable_getlog()->mutable_device()->
    set_name("com.Seagate.Kinetic.SSD.Gen1");
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ::std::string expected = "";
    AssertEqual(expected, response_value);
    EXPECT_EQ(Command_Status_StatusCode_NOT_FOUND, command_response.status().code());
}

TEST_F(GetLogHandlerTest, GetLogReturnsMessagesEvenIfOverMaxLength) {
    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));

    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_MESSAGES);
    SetUserAndHmac(&message);

    uint8_t buf[500] = {0};
    for (unsigned int i = 0; i < sizeof(buf); i++) {
    buf[i] = (uint8_t)'a';
    }
    std::string long_string((char*)buf, sizeof(buf));

    ::tm time;
    time.tm_year = 113;
    time.tm_mon = 10;
    time.tm_mday = 19;
    time.tm_hour = 19;
    time.tm_min = 45;
    time.tm_sec = 44;
    LogRingBuffer::Instance()->push(
            ::google::GLOG_INFO, long_string.c_str(), long_string.c_str(),
            22, &time, "mesg", 4, 3);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ::std::string expected;
    expected += "2013-11-19T19:45:44Z INFO 3 " + long_string + ":22 mesg\n";

    EXPECT_EQ(Command_Status_StatusCode_SUCCESS, command_response.status().code());
    EXPECT_EQ(expected, command_response.body().getlog().messages());
}

TEST_F(GetLogHandlerTest, GetLogIncludesRequestedTypesInResponse) {
    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));

    EXPECT_CALL(device_information_, GetNominalCapacityInBytes())
        .WillOnce(Return(1000));
    EXPECT_CALL(device_information_, GetPortionFull(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(0.5),
            Return(true)));

    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_MESSAGES);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_STATISTICS);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_CAPACITIES);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    ASSERT_EQ(3, command_response.body().getlog().types_size());
    EXPECT_EQ(Command_GetLog_Type_MESSAGES, command_response.body().getlog().types(0));
    EXPECT_EQ(Command_GetLog_Type_STATISTICS, command_response.body().getlog().types(1));
    EXPECT_EQ(Command_GetLog_Type_CAPACITIES, command_response.body().getlog().types(2));
}

TEST_F(GetLogHandlerTest, GetLogReturnsLimitsIfRequested) {
    EXPECT_CALL(device_information_, Authorize(42, _))
    .WillOnce(Return(true));

    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_LIMITS);
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    EXPECT_EQ((unsigned int) 100, command_response.body().getlog().limits().maxkeysize());
}

TEST_F(GetLogHandlerTest, GetLogReturnsFailureIfUserUnauthorized) {
    Message message;
    Command command;
    command.mutable_header()->
    set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_UTILIZATIONS);
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(false));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());
    EXPECT_EQ(Command_Status_StatusCode_NOT_AUTHORIZED, command_response.status().code());
}

TEST_F(GetLogHandlerTest, GetLogReturnsFirmwareVersionIfRequested) {
    Message message;
    Command command;
    std::string version = "firmware_version";
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);

    command.mutable_body()->mutable_getlog()->add_types(Command_GetLog_Type_DEVICE);
    command.mutable_body()->mutable_getlog()->mutable_device()->set_name("firmware_version");
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetF3Version(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(version),
            Return(true)));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext  request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    std::string expected_version = "firmware_version";
    AssertEqual(expected_version, response_value);
}

TEST_F(GetLogHandlerTest, GetLogReturnsUbootVersionIfRequested) {
    Message message;
    Command command;
    std::string version = "uboot_version";
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);

    command.mutable_body()->mutable_getlog()->add_types(Command_GetLog_Type_DEVICE);
    command.mutable_body()->mutable_getlog()->mutable_device()->set_name("uboot_version");
    SetUserAndHmac(&message);

    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetUbootVersion(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(version),
            Return(true)));
    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext  request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    std::string expected_version = "uboot_version";
    AssertEqual(expected_version, response_value);
}

TEST_F(GetLogHandlerTest, GetLogReturnsErrorRateIfRequested) {
    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));

    // Record failures for a Get and a Put operation
    statistics_manager_.IncrementFailureCount(Command_MessageType_GET);
    statistics_manager_.IncrementFailureCount(Command_MessageType_PUT);

    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_DEVICE);
    command.mutable_body()->mutable_getlog()->mutable_device()->set_name("object_error_rate");
    SetUserAndHmac(&message);

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ParseAndSetMessageTypes(FLAGS_message_types);
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    std::stringstream expected_stream;
    expected_stream << "{\"DELETE\":[{\"count\":0}],"
                    << "\"GET\":[{\"count\":1}],"
                    << "\"GETNEXT\":[{\"count\":0}],"
                    << "\"GETPREVIOUS\":[{\"count\":0}],"
                    << "\"PUT\":[{\"count\":1}]}\n";
    std::string expected = expected_stream.str();
    AssertEqual(expected, response_value);
}

TEST_F(GetLogHandlerTest, GetLogReturnsNetworkStatsIfRequested) {
    Message message;
    Command command;
    command.mutable_header()->set_messagetype(Command_MessageType_GETLOG);
    command.mutable_body()->mutable_getlog()->
    add_types(Command_GetLog_Type_DEVICE);
    command.mutable_body()->mutable_getlog()->mutable_device()->
        set_name("network_statistics");
    SetUserAndHmac(&message);

    std::map<string, NetworkPackets> interface_packet_information;
    struct NetworkPackets eth0_packet_information, eth1_packet_information;
    eth0_packet_information.receive_packets = 100;
    eth0_packet_information.receive_drop = 0;
    eth0_packet_information.transmit_packets = 100;
    eth0_packet_information.transmit_drop = 0;

    eth1_packet_information.receive_packets = 100;
    eth1_packet_information.receive_drop = 0;
    eth1_packet_information.transmit_packets = 100;
    eth1_packet_information.transmit_drop = 0;

    interface_packet_information["eth0"] = eth0_packet_information;
    interface_packet_information["eth1"] = eth1_packet_information;
    EXPECT_CALL(device_information_, Authorize(42, _))
        .WillOnce(Return(true));
    EXPECT_CALL(device_information_, GetNetworkStatistics(_))
        .WillOnce(DoAll(
            SetArgPointee<0>(interface_packet_information),
            Return(true)));

    Command command_response;
    NullableOutgoingValue response_value;
    RequestContext request_context;
    get_log_handler_.ProcessRequest(command, &command_response, &response_value, request_context,
            message.hmacauth().identity());

    std::stringstream expectedStream;
    expectedStream << "{\"eth0\":[{\"receive_drop\":0,\"receive_packets\":100,"
                   << "\"transmit_drop\":0,\"transmit_packets\":100}],"
                   << "\"eth1\":[{\"receive_drop\":0,\"receive_packets\":100,"
                   << "\"transmit_drop\":0,\"transmit_packets\":100}]}\n";
    std::string expected = expectedStream.str();
    AssertEqual(expected, response_value);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
