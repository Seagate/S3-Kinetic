#include "gtest/gtest.h"
#define JSON_IS_AMALGAMATION
#include "json/json.h"

#include "getlog_multicastor.h"
#include "mock_device_information.h"
#include "network_interfaces.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_MessageType_GET;
using proto::Command_MessageType_GET_RESPONSE;

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Invoke;


bool _setMockNetworkInterfaceInfo(std::vector<DeviceNetworkInterface> *interfaces) {
    DeviceNetworkInterface if1;
    if1.name = "eth11";
    if1.mac_address = "deadbeef";
    if1.ipv4 = "127.1.1.1";
    if1.ipv6 = "::1";

    interfaces->push_back(if1);

    DeviceNetworkInterface if2;
    if2.name = "eth22";
    if2.mac_address = "cafebabe";
    if2.ipv4 = "127.2.2.2";
    if2.ipv6 = "::2";

    interfaces->push_back(if2);

    return true;
}

class GetLogMulticastorTest : public ::testing::Test {
    protected:
    GetLogMulticastorTest():
        port_(123),
        tls_port_(123),
        multicast_announcer_(port_,
                tls_port_,
                mock_network_interfaces_,
                mock_device_information_) {}


    Json::Value MakeMulticastMessage() {
        EXPECT_CALL(mock_network_interfaces_, GetExternallyVisibleNetworkInterfaces(_)).WillOnce(
            Invoke(_setMockNetworkInterfaceInfo));

        std::string message;
        EXPECT_TRUE(multicast_announcer_.MakeMulticastMessage(&message));
        Json::Value root;
        Json::Reader reader;
        bool result = reader.parse(message, root);
        EXPECT_TRUE(result);
        return root;
    }

    uint port_;
    uint tls_port_;
    MockNetworkInterfaces mock_network_interfaces_;
    MockDeviceInformation mock_device_information_;
    GetLogMulticastor multicast_announcer_;
};

TEST_F(GetLogMulticastorTest, MessageIncludesPorts) {
    Json::Value root = MakeMulticastMessage();

    EXPECT_EQ(port_, root["port"].asUInt());
    EXPECT_EQ(port_, root["tlsPort"].asUInt());
}

TEST_F(GetLogMulticastorTest, MessageIncludesNetworkInterfaces) {
    Json::Value root = MakeMulticastMessage();

    ASSERT_EQ(2U, root["network_interfaces"].size());

    EXPECT_EQ("eth11", root["network_interfaces"][0]["name"].asString());
    EXPECT_EQ("deadbeef", root["network_interfaces"][0]["mac_addr"].asString());
    EXPECT_EQ("127.1.1.1", root["network_interfaces"][0]["ipv4_addr"].asString());
    EXPECT_EQ("::1", root["network_interfaces"][0]["ipv6_addr"].asString());

    EXPECT_EQ("eth22", root["network_interfaces"][1]["name"].asString());
    EXPECT_EQ("cafebabe", root["network_interfaces"][1]["mac_addr"].asString());
    EXPECT_EQ("127.2.2.2", root["network_interfaces"][1]["ipv4_addr"].asString());
    EXPECT_EQ("::2", root["network_interfaces"][1]["ipv6_addr"].asString());
}

} // namespace kinetic
} // namespace seagate
} // namespace com
