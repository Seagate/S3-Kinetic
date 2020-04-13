#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>

#include "gtest/gtest.h"

#include "multicast_announcer.h"
#include "mock_device_information.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class MulticastAnnouncerTest : public ::testing::Test {
    protected:
    virtual void SetUp() {
        eth0_path_ = "/tmp/multicast_announcer_testXXXXXX";
        eth0_char_ = strdup("/tmp/multicast_announcer_testXXXXXX");
        eth0_ = mkstemp(eth0_char_);
        ASSERT_GE(eth0_, 0);

        std::shared_ptr<GetLogMulticastor> multicastor(new GetLogMulticastor(123,
                                                       456,
                                                       mock_network_interfaces_,
                                                       mock_device_information_));

        multicast_announcer_ = new MulticastAnnouncer("eth0",
                                                      eth0_path_,
                                                      eth0_path_,
                                                      multicastor,
                                                      mock_network_interfaces_);
    }

    virtual void TearDown() {
        PCHECK(!close(eth0_));

        PCHECK(!unlink(eth0_char_));

        free(eth0_char_);

        delete multicast_announcer_;
    }

    MockNetworkInterfaces mock_network_interfaces_;
    std::string eth0_path_;
    char* eth0_char_;
    int eth0_;
    MockDeviceInformation mock_device_information_;
    MulticastAnnouncer* multicast_announcer_;
};
TEST_F(MulticastAnnouncerTest, LoadVendorOptionsReturnsFalseIfNoFiles) {
    string options;
    OptionsFileState state = OptionsFileState::NOT_LOADED;
    ASSERT_FALSE(multicast_announcer_->LoadVendorOptions(eth0_path_, options, state));
}

TEST_F(MulticastAnnouncerTest, HexToBytesWorks) {
    string output;
    multicast_announcer_->HexToBytes("30313233", output);
    EXPECT_EQ("0123", output);

    char expected_buf[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xf};
    string expected(expected_buf, sizeof(expected_buf));
    multicast_announcer_->HexToBytes("0102030405060708090A0f\n", output);
    EXPECT_EQ(expected, output);

    char expected_buf2[] = {1, 1, 1, 2, 4, 0x7f, 0, 0, 1, 3, 2, 0x27, 0x0f};
    string expected2(expected_buf2, sizeof(expected_buf2));
    multicast_announcer_->HexToBytes("01010102047f0000010302270f\n", output);
    EXPECT_EQ(expected2, output);
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsHandlesDisabledMulticast) {
    ASSERT_TRUE(
            multicast_announcer_->ParseVendorOptions("010100\n"));

    EXPECT_FALSE(multicast_announcer_->bAnnounce_);
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsHandlesMulticastAddress) {
    ASSERT_TRUE(
            multicast_announcer_->ParseVendorOptions("01010102047f0000010302270f\n"));

    EXPECT_TRUE(multicast_announcer_->bAnnounce_);
    EXPECT_EQ(inet_addr("127.0.0.1"), multicast_announcer_->ip_);
    EXPECT_EQ(9999, multicast_announcer_->port_);
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsHandlesIPv6MulticastAddress) {
    ASSERT_TRUE(
            multicast_announcer_->ParseVendorOptions(
                "0510000000000000000000000000000000010602270f\n"));

    char addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(multicast_announcer_->ip6_), addr, INET6_ADDRSTRLEN);
    EXPECT_TRUE(multicast_announcer_->bAnnounce6_);
    EXPECT_EQ("::1", (std::string) addr);
    EXPECT_EQ(9999, multicast_announcer_->port6_);
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsUsesDefaultIpv6MulticastConfiguration) {
    ASSERT_TRUE(
            multicast_announcer_->ParseVendorOptions("01010102047f0000010302270f\n"));

    char addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(multicast_announcer_->ip6_), addr, INET6_ADDRSTRLEN);
    EXPECT_TRUE(multicast_announcer_->bAnnounce6_);
    EXPECT_EQ("ff02::1", (std::string) addr);
    EXPECT_EQ(8123, multicast_announcer_->port6_);
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsHandlesUnicastAddress) {
    ASSERT_TRUE(
            multicast_announcer_->ParseVendorOptions("01010102040a00010103023039\n"));
    EXPECT_TRUE(multicast_announcer_->bAnnounce_);
    EXPECT_EQ(inet_addr("10.0.1.1"), multicast_announcer_->ip_);
    EXPECT_EQ(12345, multicast_announcer_->port_);
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsDetectsImpossiblyLongLength) {
    ASSERT_FALSE(
            multicast_announcer_->ParseVendorOptions("010200\n"));
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsAllowsUnusedPaddingAfter) {
    ASSERT_TRUE(
            multicast_announcer_->ParseVendorOptions("01010000\n"));
}

TEST_F(MulticastAnnouncerTest, ParseVendorOptionsIgnoresUnexpectedOption) {
    ASSERT_TRUE(multicast_announcer_->ParseVendorOptions("100101010100040100\n"));

    EXPECT_FALSE(multicast_announcer_->bAnnounce_);
    EXPECT_FALSE(multicast_announcer_->bAnnounce6_);
}
} // namespace kinetic
} // namespace seagate
} // namespace com
