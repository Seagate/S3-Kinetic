#include <vector>

#include "gtest/gtest.h"

#include "domain.h"
#include "mock_authorizer.h"
#include "network_interfaces.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::testing::Return;

TEST(NetworkInterfacesTest, GetNetworkInterfacesReturnsPlausibleInformation) {
    // Obviously, we can't assert actual MAC addresses/IPs/etc, but we can at
    // least do some basic sanity checks. This way we catch memory leaks,
    // errors in grabbing MAC addresses, etc
    std::vector<DeviceNetworkInterface> interfaces;

    NetworkInterfaces network_interfaces;
    ASSERT_TRUE(network_interfaces.GetAllNetworkInterfaces(&interfaces));

    // Everything we run tests on has at least 1 interface
    ASSERT_GE(interfaces.size(), 1U);

    DeviceNetworkInterface interface = interfaces[0];

    // Everywhere we run tests should at least have lo or something
    ASSERT_GE(interface.name.length(), 2U);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
