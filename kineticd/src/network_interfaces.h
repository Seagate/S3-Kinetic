#ifndef KINETIC_NETWORK_INTERFACES_H_
#define KINETIC_NETWORK_INTERFACES_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"

namespace com {
namespace seagate {
namespace kinetic {

typedef struct DeviceNetworkInterface {
    std::string name;
    std::string mac_address;
    std::string ipv4;
    std::string ipv6;
} DeviceNetworkInterface;

class NetworkInterfaces {
    public:
    virtual bool GetAllNetworkInterfaces(std::vector<DeviceNetworkInterface>* interfaces);
    virtual bool GetExternallyVisibleNetworkInterfaces(std::vector<DeviceNetworkInterface>* interfaces);
    virtual ~NetworkInterfaces() {}

    private:
    void RemoveInactiveInterfaces(std::vector<DeviceNetworkInterface>* interfaces);
    virtual bool GetNetworkInterfaces(std::vector<DeviceNetworkInterface>* interfaces, bool includeLoopback);
};

class MockNetworkInterfaces : public NetworkInterfaces {
    public:
    MOCK_METHOD1(GetExternallyVisibleNetworkInterfaces, bool(std::vector<DeviceNetworkInterface>* interfaces));
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_NETWORK_INTERFACES_H_
