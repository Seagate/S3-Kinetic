#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <algorithm>
#include <arpa/inet.h>
#include <memory>
#include <iostream>
#include "glog/logging.h"

#include "multicast_announcer.h"

using namespace std; //NOLINT

const char SUBOPT_ANNOUNCE_01 = 01;
const char SUBOPT_IP_ADDRESS_02 = 02;
const char SUBOPT_PORT_NUMBER_03 = 03;
const char SUBOPT_IPV6_ANNOUNCE_04 = 04;
const char SUBOPT_IPV6_ADDRESS_05 = 05;
const char SUBOPT_IPV6_PORT_NUMBER_06 = 06;

namespace com {
namespace seagate {
namespace kinetic {

MulticastAnnouncer::MulticastAnnouncer(
        std::string name,
        std::string vendor_options_file,
        std::string ipv6_vendor_options_file,
        std::shared_ptr<GetLogMulticastor> multicastor,
        NetworkInterfaces& network_interfaces) :
    name_(name),
    vendor_options_file_(vendor_options_file),
    ipv6_vendor_options_file_(ipv6_vendor_options_file),
    multicastor_(multicastor),
    network_interfaces_(network_interfaces) {
        ipv4_ = "";
        ipv4_options_state_ = OptionsFileState::INITIAL;
        ipv6_options_state_ = OptionsFileState::INITIAL;
        multicastor_->LoadDriveIdentification();
    }

bool MulticastAnnouncer::Configure() {
    if (!ReadAnnouncerInfo(true)) {
        return false;
    }

    std::vector<DeviceNetworkInterface> interfaces;
    if (!network_interfaces_.GetAllNetworkInterfaces(&interfaces)) {
        return false;
    }

    // Need to find the interface's IP address and update it incase it got a new lease
    // with a new IP address
    for (std::vector<DeviceNetworkInterface>::iterator it = interfaces.begin();
         true; ++it) {
        if (it == interfaces.end()) {
            bAnnounce_ = false;
            break;
        } else if (it->name == name_) {
            if (!it->ipv4.empty()) {
                ipv4_ = it->ipv4;
            } else {
                bAnnounce_ = false;
            }
            break;
        }
    }

    // Do configuration for IPv4; if configuration fails, change announce to false
    if (bAnnounce_ && !multicastor_->ConfigureTarget(ipv4_, ip_, port_)) {
        bAnnounce_ = false;
    }

    // Do configuration for IPv6; if configuration fails, change announce to false
    if (bAnnounce6_ && !multicastor_->ConfigureIPv6Target(name_, ip6_, port6_)) {
        bAnnounce6_ = false;
    }
    return true;
}

void MulticastAnnouncer::Announce() {
    // IPv4 broadcast
    if (bAnnounce_) {
        multicastor_->ChangeTarget(ipv4_, ip_, port_);
        multicastor_->BroadcastMessage();
    }

    // IPv6 broadcast
    if (bAnnounce6_) {
        multicastor_->ChangeIPv6Target(name_, ip6_, port6_);
        multicastor_->BroadcastIPv6Message();
    }
}

bool MulticastAnnouncer::ReadFirstLineOfFile(const std::string &path, std::string &line) {
    line.clear();

    FILE* fd = fopen(path.c_str(), "r");
    if (!fd) {
        if (errno == 2) {
#ifdef KDEBUG
            DLOG(INFO) << "Failed to read " << path << " because file does not exist";
#endif
        } else {
            PLOG(ERROR) << "Failed to open " << path;
        }
        return false;
    }

    char* pLine = NULL;
    ssize_t len;
    size_t linecapp;
    bool success = true;
    if ((len = getline(&pLine, &linecapp, fd)) == -1) {
        PLOG(WARNING) << "Failed to read " << path;
        success = false;
    } else {
        line.assign(pLine);
    }

    if (pLine) {
        free(pLine);
    }

    if (fclose(fd)) {
        PLOG(WARNING) << "Unable to close " << path;
    }

    return success;
}

bool MulticastAnnouncer::LoadVendorOptions(std::string &vendor_options_file,
                                           std::string &vendor_options,
                                           OptionsFileState &state) {
    vendor_options.clear();

    if (ReadFirstLineOfFile(vendor_options_file, vendor_options)) {
        if (state == OptionsFileState::INITIAL || state == OptionsFileState::NOT_LOADED) {
            VLOG(1) << "Loaded " << name_ << " vendor options from " << vendor_options_file;
            state = OptionsFileState::LOADED;
        }
        return true;
    } else if (state == OptionsFileState::INITIAL || state == OptionsFileState::LOADED) {
        VLOG(1) << "Unable to load " << name_ << " vendor options from " << vendor_options_file;
        state = OptionsFileState::NOT_LOADED;
    }

    return false;
}

bool MulticastAnnouncer::ReadAnnouncerInfo(bool ipv4) {
    std::string vendor_options = "";
    std::string ipv6_vendor_options = "";

    if (ipv4) {
        // Load DHCP vendor options
        if ((!LoadVendorOptions(vendor_options_file_, vendor_options, ipv4_options_state_)) ||
            (vendor_options.length() < 2)) {
            // Configure default multicast
            vendor_options = "0101010204ef01020303021fbb";
        } else {
            VLOG(3) << "Received DHCP custom options <" << vendor_options << ">";//NO_SPELL
        }
    }

    // Load DHCPv6 vendor options
    if ((!LoadVendorOptions(ipv6_vendor_options_file_, ipv6_vendor_options, ipv6_options_state_)) ||
        (ipv6_vendor_options.length() < 2)) {
        // Configure default IPv6 multicast
        ipv6_vendor_options = "0401010510ff02000000000000000000000000000106021fbb";
    } else {
        VLOG(3) << "Received DHCPv6 custom options <" << ipv6_vendor_options << ">";//NO_SPELL
    }

    // concatenate ipv6 vendor options and remove newline characters
    vendor_options += ipv6_vendor_options;
    vendor_options.erase(std::remove(vendor_options.begin(),
                                     vendor_options.end(), '\n'),
                                     vendor_options.end());

    bool ret;
    ret = ParseVendorOptions(vendor_options);
    if (ret) {
        VLOG(3) << "New " << name_
                << " multicast configuration: enabled = " << bAnnounce_//NO_SPELL
                << " ip = " << ip_//NO_SPELL
                << " port = " << port_
                << " ipv6 enabled = " << bAnnounce6_//NO_SPELL
                << " port6 = " << port6_;//NO_SPELL
    } else {
        LOG(WARNING) << "Unable to parse " << name_ << " vendor options";//NO_SPELL
    }

    return ret;
}

void MulticastAnnouncer::HexToBytes(const std::string &hex, std::string &bytes) {
    bytes.clear();
    char buf[3] = {0};
    for (size_t i = 0; i < hex.size() - 1; i += 2) {
        buf[0] = hex[i];
        buf[1] = hex[i + 1];
        bytes += (char)strtoul(buf, NULL, 16);
    }
}

bool MulticastAnnouncer::ParseVendorOptions(const std::string &hex_encoded_vendor_options) {
    bAnnounce_ = false;
    ip_ = 0;
    port_ = 0;

    // Multicast on IPv6 should be on and configured by default
    bAnnounce6_ = true;
    if (!inet_pton(AF_INET6, "ff02::1", &ip6_)) {
        bAnnounce6_ = false;
    }
    port6_ = 8123;

    // Note that the vendor options are hex-encoded, so {0xCA, 0xFE} will show up as "CAFE"
    std::string vendor_options;
    HexToBytes(hex_encoded_vendor_options, vendor_options);

    for (size_t i = 0; i < vendor_options.size() - 1;) {
        int suboption_type = vendor_options[i];
        int suboption_length = vendor_options[i + 1];

        size_t type_idx = i;
        size_t length_idx = i + 1;
        size_t value_end_idx = length_idx + suboption_length;

        if (value_end_idx >= vendor_options.size()) {
            LOG(WARNING) << "DHCP option " << suboption_type << " has invalid length " << suboption_length;//NO_SPELL
            return false;
        }

        // only valid if suboption_length >= 1, of course
        size_t value_start_idx = i + 2;

        switch (suboption_type) {
            case SUBOPT_ANNOUNCE_01:
                if (suboption_length != 1) {
                    LOG(WARNING) << "DHCP announce option has unexpected length " << suboption_length;//NO_SPELL
                    return false;
                }
                bAnnounce_ = vendor_options[value_start_idx];
                break;
            case SUBOPT_IP_ADDRESS_02:
                if (suboption_length != 4) {
                    LOG(WARNING) << "DHCP ip option has unexpected length " << suboption_length;//NO_SPELL
                    return false;
                }
                ip_ = ((((unsigned uint32_t)vendor_options[value_start_idx + 3] << 24) & 0xFF000000) |
                       (((unsigned uint32_t)vendor_options[value_start_idx + 2] << 16) & 0x00FF0000) |
                       (((unsigned uint32_t)vendor_options[value_start_idx + 1] << 8) & 0x0000FF00) |
                       ((unsigned uint32_t)vendor_options[value_start_idx] & 0x000000FF));
                break;
            case SUBOPT_PORT_NUMBER_03:
                if (suboption_length != 2) {
                    LOG(WARNING) << "DHCP port option has unexpected length " << suboption_length;//NO_SPELL
                    return false;
                }
                port_ = (uint16_t)(((uint16_t)vendor_options[value_start_idx] << 8) & 0xFF00) |
                        (((uint16_t)vendor_options[value_start_idx + 1]) & 0x00FF);
                break;
            case SUBOPT_IPV6_ANNOUNCE_04:
                if (suboption_length != 1) {
                    LOG(WARNING) << "DHCP ipv6 announce option has unexpected length " << suboption_length;//NO_SPELL
                    return false;
                }
                bAnnounce6_ = vendor_options[value_start_idx];
                break;
            case SUBOPT_IPV6_ADDRESS_05:
                if (suboption_length != 16) {
                    LOG(WARNING) << "DHCP ipv6 option has unexpected length " << suboption_length;//NO_SPELL
                    return false;
                }
                memcpy(&ip6_, &vendor_options[value_start_idx], sizeof(in6_addr));
                break;
            case SUBOPT_IPV6_PORT_NUMBER_06:
                if (suboption_length != 2) {
                    LOG(WARNING) << "DHCP ipv6 port option has unexpected length " << suboption_length;//NO_SPELL
                    return false;
                }
                port6_ = (uint16_t)(((uint16_t)vendor_options[value_start_idx] << 8) & 0xFF00) |
                        (((uint16_t)vendor_options[value_start_idx + 1]) & 0x00FF);
                break;
            default:
                LOG(WARNING) << "Unexpected DHCP vendor option " << suboption_type <<//NO_SPELL
                        " at index " << type_idx <<
                        " with length " << suboption_length;
                break;
        }

        i += 2 + suboption_length;
    }

    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
