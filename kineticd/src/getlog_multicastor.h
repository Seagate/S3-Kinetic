#ifndef KINETIC_GETLOG_MULTICASTOR_H_
#define KINETIC_GETLOG_MULTICASTOR_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "glog/logging.h"
#include "kinetic/common.h"

#include "device_information_interface.h"
#include "network_interfaces.h"
#include "statistics_manager.h"
#include "scheduled_background_task.h"

namespace com {
namespace seagate {
namespace kinetic {

class GetLogMulticastor {
    public:
    GetLogMulticastor(
        int server_port,
        int server_ssl_port,
            NetworkInterfaces& network_interfaces,
            DeviceInformationInterface& device_information);
    ~GetLogMulticastor();
    bool ConfigureTarget(
        std::string ipv4,
        in_addr_t target_ip,
        uint16_t target_port);
    bool ConfigureIPv6Target(
        std::string name,
        in6_addr target_ip6,
        uint16_t target_port6);
    void ChangeTarget(
        std::string ipv4,
        in_addr_t target_ip,
        uint16_t target_port);
    void ChangeIPv6Target(
        std::string name,
        in6_addr target_ip6,
        uint16_t target_port6);
    void CloseTarget();
    void CloseIPv6Target();
    void BroadcastMessage();
    void BroadcastIPv6Message();
    bool MakeMulticastMessage(std::string* json);
    bool LoadDriveIdentification();

    private:
    int server_port_;
    int server_ssl_port_;
    NetworkInterfaces& network_interfaces_;
    DeviceInformationInterface& device_information_;
    int sock_fd_;
    int sock6_fd_;
    struct sockaddr_in multicast_address_;
    struct sockaddr_in6 multicast_address6_;
    std::string drive_sn_;
    std::string drive_wwn_;
    std::string drive_vendor_;
    std::string drive_model_;
    DISALLOW_COPY_AND_ASSIGN(GetLogMulticastor);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_GETLOG_MULTICASTOR_H_
