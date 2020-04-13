#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>

#include "glog/logging.h"
#define JSON_IS_AMALGAMATION
#include "json/json.h"
#include "version_info.h"


#include "getlog_multicastor.h"

namespace com {
namespace seagate {
namespace kinetic {

GetLogMulticastor::GetLogMulticastor(
        int server_port,
        int server_ssl_port,
        NetworkInterfaces& network_interfaces,
        DeviceInformationInterface& device_information)
            :server_port_(server_port),
            server_ssl_port_(server_ssl_port),
            network_interfaces_(network_interfaces),
            device_information_(device_information),
            sock_fd_(-1),
            sock6_fd_(-1) {}

GetLogMulticastor::~GetLogMulticastor() {
    if (sock_fd_ >= 0 && close(sock_fd_) != 0) {
        PLOG(ERROR) << "Failed to close IPv4 multicast socket";//NO_SPELL
    }
    if (sock6_fd_ >= 0 && close(sock6_fd_) != 0) {
        PLOG(ERROR) << "Failed to close IPv6 multicast socket";//NO_SPELL
    }
}

void GetLogMulticastor::BroadcastMessage() {
    if (sock_fd_ <= 0) {
        LOG_EVERY_N(ERROR, 10) << "Bad socket file descriptor" << sock_fd_;
        return;
    }

    std::string json;
    if (!MakeMulticastMessage(&json)) {
        LOG_EVERY_N(WARNING, 10) << "Error building multicast packet JSON";//NO_SPELL
        return;
    }
#ifdef KDEBUG
    DLOG_EVERY_N(INFO, 10) << "Sending multicast JSON " << json;//NO_SPELL
#endif

    ssize_t bytes_sent = sendto(sock_fd_,
        json.c_str(),
        json.length(),
        0,
        (struct sockaddr *)&multicast_address_,
        sizeof(struct sockaddr));

    if (bytes_sent < 0) {
        PLOG_EVERY_N(WARNING, 10) << "Error sending multicast packet";//NO_SPELL
        return;
    }

    if ((size_t)bytes_sent != json.length()) {
        LOG(WARNING) << "Should have sent multicast packet of length " << json.length() << " but sent " << bytes_sent;
    }
}

void GetLogMulticastor::BroadcastIPv6Message() {
    if (sock6_fd_ < 0) {
        LOG_EVERY_N(ERROR, 10) << "Bad socket file descriptor";
        return;
    }

    std::string json;
    if (!MakeMulticastMessage(&json)) {
        LOG_EVERY_N(WARNING, 10) << "Error building multicast packet JSON";//NO_SPELL
        return;
    }
#ifdef KDEBUG
    DLOG_EVERY_N(INFO, 10) << "Sending multicast6 JSON " << json;//NO_SPELL
#endif
    ssize_t bytes_sent = sendto(sock6_fd_,
        json.c_str(),
        json.length(),
        0,
        (struct sockaddr *)&multicast_address6_,
        sizeof(multicast_address6_));

    if (bytes_sent < 0) {
        PLOG_EVERY_N(WARNING, 10) << "Error sending multicast packet on IPv6";//NO_SPELL
        return;
    }

    if ((size_t)bytes_sent != json.length()) {
        LOG(WARNING) << "Should have sent multicast packet of length " //NO_SPELL
                     << json.length() << " but sent " << bytes_sent << " on IPv6";
    }
}

bool GetLogMulticastor::MakeMulticastMessage(std::string* json) {
    Json::Value root;
    root["port"] = server_port_;
    root["tlsPort"] = server_ssl_port_;
    root["serial_number"] = drive_sn_;
    root["world_wide_name"] = drive_wwn_;
    root["firmware_version"] = CURRENT_SEMANTIC_VERSION;
    root["protocol_version"] = CURRENT_PROTOCOL_VERSION;
    root["compilation_date"] = BUILD_DATE;
    root["manufacturer"] = drive_vendor_;
    root["model"] = drive_model_;

    std::vector<DeviceNetworkInterface> interfaces;
    if (!network_interfaces_.GetExternallyVisibleNetworkInterfaces(&interfaces)) {
        return false;
    }
    for (auto it = interfaces.begin(); it != interfaces.end(); ++it) {
        Json::Value if_json;
        if_json["name"] = it->name;
        if_json["mac_addr"] = it->mac_address;
        if_json["ipv4_addr"] = it->ipv4;
        if_json["ipv6_addr"] = it->ipv6;
        root["network_interfaces"].append(if_json);
    }

    Json::FastWriter writer;
    *json = writer.write(root);

    return true;
}

bool GetLogMulticastor::LoadDriveIdentification() {
    std::string drive_sn;
    std::string drive_wwn;
    std::string drive_vendor;
    std::string drive_model;

    if (device_information_.GetDriveIdentification(&drive_wwn, &drive_sn,
                                                   &drive_vendor, &drive_model)) {
        drive_sn_ = drive_sn;
        drive_wwn_ = drive_wwn;
        drive_vendor_ = drive_vendor;
        drive_model_ = drive_model;
        return true;
    } else {
        drive_sn_ = "";
        drive_wwn_ =  "";
        drive_vendor_ = "";
        drive_model_ = "";
        return false;
    }
}

bool GetLogMulticastor::ConfigureTarget(
        std::string ipv4, in_addr_t target_ip, uint16_t target_port) {
    CloseTarget();

    if (ipv4.empty()) {
        // Means that the interface does not have an IP address so we can't multicast
        return false;
    }

    // IPv4 multicast socket setup
    CHECK_GE(sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0), 0);
    memset(&multicast_address_, 0, sizeof(struct sockaddr_in));
    multicast_address_.sin_family = AF_INET;
    multicast_address_.sin_port = htons(target_port);
    multicast_address_.sin_addr.s_addr = target_ip;

    struct in_addr local_interface;
    inet_pton(AF_INET, ipv4.c_str(), &(local_interface.s_addr));

    if (setsockopt(sock_fd_,
                   IPPROTO_IP, IP_MULTICAST_IF,
                   (char *)&local_interface,
                   sizeof(local_interface)) < 0) {
        PLOG(WARNING) << "Error setting local interface " << ipv4;
        return false;
    }

    return true;
}

bool GetLogMulticastor::ConfigureIPv6Target(
        std::string name, in6_addr target_ip6, uint16_t target_port6) {
    CloseIPv6Target();

    // IPv6 multicast socket setup
    CHECK_GE(sock6_fd_ = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP), 0);

    memset(&multicast_address6_, 0, sizeof(struct sockaddr_in6));
    multicast_address6_.sin6_family = AF_INET6;
    multicast_address6_.sin6_port = htons(target_port6);
    multicast_address6_.sin6_addr = target_ip6;
    multicast_address6_.sin6_scope_id = if_nametoindex(name.c_str());

    return true;
}

void GetLogMulticastor::ChangeTarget(
        std::string ipv4, in_addr_t target_ip, uint16_t target_port) {
    // Change IP and port for IPv4 socket
    multicast_address_.sin_port = htons(target_port);
    multicast_address_.sin_addr.s_addr = target_ip;

    struct in_addr local_interface;
    inet_pton(AF_INET, ipv4.c_str(), &(local_interface.s_addr));

    if (setsockopt(sock_fd_,
                   IPPROTO_IP, IP_MULTICAST_IF,
                   (char *)&local_interface,
                   sizeof(local_interface)) < 0) {
        // if setting the socket options failed, we need to reconfigure the socket
        ConfigureTarget(ipv4, target_ip, target_port);
    }
}

void GetLogMulticastor::ChangeIPv6Target(
        std::string name, in6_addr target_ip6, uint16_t target_port6) {
    // Change IP and port for IPv6 socket
    multicast_address6_.sin6_port = htons(target_port6);
    multicast_address6_.sin6_addr = target_ip6;
    multicast_address6_.sin6_scope_id = if_nametoindex(name.c_str());
}

void GetLogMulticastor::CloseTarget() {
    // Clean up for IPv4 multicast socket
    if (sock_fd_ >= 0 && close(sock_fd_) != 0) {
        PLOG(ERROR) << "Failed to close IPv4 multicast socket";//NO_SPELL
    }
    sock_fd_ = -1;
}

void GetLogMulticastor::CloseIPv6Target() {
    // Clean up for IPv6 multicast socket
    if (sock6_fd_ >= 0 && close(sock6_fd_) != 0) {
        PLOG(ERROR) << "Failed to close IPv6 multicast socket";//NO_SPELL
    }
    sock6_fd_ = -1;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
