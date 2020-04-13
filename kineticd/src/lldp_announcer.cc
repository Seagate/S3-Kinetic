#include <arpa/inet.h>
#include <pthread.h>
#include "glog/logging.h"
#include "version_info.h"
#include "device_information_interface.h"
#include "network_interfaces.h"
#include "popen_wrapper.h"
#include "lldp_announcer.h"
#include "lldp_tlv.h"
#include "server.h"

using ::lldp_kin::LldpSocket;
using ::lldp_kin::TlvType;
using ::lldp_kin::LldpTlv;
using ::lldp_kin::SubtypedLldpTlv;
using ::lldp_kin::OrgLldpTlv;

namespace com {
namespace seagate {
namespace kinetic {

const std::string LldpAnnouncer::kBOND_NAME = "bond0";
const std::string LldpAnnouncer::kETH0_NAME = "eth0";
const std::string LldpAnnouncer::kETH1_NAME = "eth1";

LldpAnnouncer::LldpAnnouncer(std::vector<std::string>& iface_names,
                             NetworkInterfaces& network_interfaces,
                             DeviceInformationInterface& device_information)
    : network_interfaces_(network_interfaces),
      device_information_(device_information),
      sock_(iface_names) {
        std::vector<std::string> def_addresses = {"", "", ""};
        for (auto& it : iface_names) {
            iface_map_.insert(std::make_pair(it, def_addresses));

            // Get mac address from uboot environment variable
            int ret_code;
            std::string command;
            std::string command_result;
            RawStringProcessor processor(&command_result, &ret_code);

            if (it == kETH0_NAME) {
                command = "fw_printenv ethaddr";
            } else if (it == kETH1_NAME) {
                command = "fw_printenv eth1addr";
            } else {
                // Unknown interface, continue
                continue;
            }

            if (execute_command(command, processor)) {
                size_t start = command_result.find("=");
                if (start != std::string::npos) {
                    iface_map_[it][2] = command_result.substr(start + 1);
                }
            }
        }
    }

bool LldpAnnouncer::Configure() {
    if (!LoadDriveIdentification()) {
        return false;
    }

    std::vector<DeviceNetworkInterface> interfaces;
    if (!network_interfaces_.GetExternallyVisibleNetworkInterfaces(&interfaces)) {
        return false;
    }

    for (auto& it : interfaces) {
        if (it.name == kBOND_NAME) {
            for (auto& i_info : iface_map_) {
                i_info.second[0] = it.ipv4;
                i_info.second[1] = it.ipv6;
            }
        } else if (iface_map_.find(it.name) != iface_map_.end()) {
            iface_map_[it.name][0] = it.ipv4;
            iface_map_[it.name][1] = it.ipv6;
        }
    }
    return true;
}

void LldpAnnouncer::Announce() {
    for (auto& it : iface_map_) {
        std::vector<uint8_t> mac_addr = sock_.SetTarget(it.first);
        BuildLldpdu(it.first, it.second, mac_addr);
        if (sock_.SendBuffer() < 0) {
            LOG(WARNING) << "Error sending LLDP announce on " << it.first;
        }
    }
}

void LldpAnnouncer::run() {
    // The number of fast transmits is the constant minus one to account for the first transmit
    uint8_t fast_xmits = kFAST_XMITS - 1;
    uint8_t sleep_time;

    while (!Server::_shuttingDown) {
        // Configure and announce
        if (Configure()) {
            Announce();
        }

        // If we are still in the fast transmit window, use shorter sleep time, else use normal period
        if (fast_xmits > 0) {
            sleep_time = kFAST_XMIT_PERIOD;
            --fast_xmits;
        } else {
            sleep_time = kNORMAL_XMIT_PERIOD;
        }

        // Sleep for period
        if (!Server::_shuttingDown) {
           sleep(sleep_time);
        }
    }
}

bool LldpAnnouncer::LoadDriveIdentification() {
    std::string drive_serial_number;
    std::string drive_wwn;
    std::string drive_vendor;
    std::string drive_model;

    drive_firmware_ = std::string(CURRENT_SEMANTIC_VERSION);

    if (device_information_.GetDriveIdentification(&drive_wwn, &drive_serial_number,
                                                   &drive_vendor, &drive_model)) {
        // Convert WWN into numeric form
        StringToNumeric(drive_wwn, drive_wwn_);

        drive_serial_number_ = drive_serial_number;
        drive_vendor_ = drive_vendor;
        drive_model_ = drive_model;
        return true;
    } else {
        drive_serial_number_ = "";
        drive_wwn_ = {};
        drive_vendor_ = "";
        drive_model_ = "";
        return false;
    }
}

void LldpAnnouncer::BuildLldpdu(const std::string& interface, std::vector<std::string>& addresses,
                                std::vector<uint8_t>& mac_addr) {
    uint8_t* buff = sock_.GetPayloadBuffer();
    uint16_t offset = 0;
    std::vector<std::string*> org_strings;

    // Build the mandatory TLVs
    BuildChassisIdTlv(offset, buff, mac_addr);
    BuildPortIdTlv(offset, buff, interface);
    BuildTimeToLiveTlv(offset, buff);

    // Build the organizationally specific TLV type 2 (model, vendor, firmware)
    org_strings = {&drive_model_, &drive_vendor_, &drive_firmware_};
    BuildOrgTlv(offset, buff, 2, org_strings);

    // Build the organizationally specific TLV type 3 (serial number, world wide name)
    BuildOrgTlv(offset, buff, 3);

    // Build the organizationally specific TLV type 4 (IPv4)
    BuildOrgTlv(offset, buff, 4, addresses[0], AF_INET);

    // Build the organizationally specific TLV type 5 (IPv6)
    BuildOrgTlv(offset, buff, 5, addresses[1], AF_INET6);

    // Build the organizationally specific TLV type 6 (port number, port mac address)
    BuildOrgTlv(offset, buff, 6, interface, addresses[2]);

    // Set end TLV
    LldpTlv end_tlv(TlvType::END, 0, buff + offset);

    // Set the payload size for the socket
    sock_.SetPayloadSize(offset + 2);
}

void LldpAnnouncer::BuildChassisIdTlv(uint16_t& offset, uint8_t* buff, std::vector<uint8_t>& mac_addr) {
    // Build the chassis ID TLV with mac address subtype
    SubtypedLldpTlv chassis_id(TlvType::CHASSIS_ID, kCHASSIS_ID_LENGTH, buff + offset);

    chassis_id.SetSubtype(4);
    uint8_t* cid_val = chassis_id.GetValue();

    *cid_val = mac_addr[0];
    *(cid_val + 1) = mac_addr[1];
    *(cid_val + 2) = mac_addr[2];
    *(cid_val + 3) = mac_addr[3];
    *(cid_val + 4) = mac_addr[4];
    *(cid_val + 5) = mac_addr[5];
    offset += chassis_id.GetSize();
}

void LldpAnnouncer::BuildPortIdTlv(uint16_t& offset, uint8_t* buff, const std::string& interface) {
    // Build the port ID TLV with interface name subtype
    SubtypedLldpTlv port_id(TlvType::PORT_ID, kPORT_ID_LENGTH, buff + offset);

    port_id.SetSubtype(5);
    strncpy((char*)port_id.GetValue(), interface.c_str(), interface.length());
    offset += port_id.GetSize();
}

void LldpAnnouncer::BuildTimeToLiveTlv(uint16_t& offset, uint8_t* buff) {
    // Build the time to live TLV
    LldpTlv ttv(TlvType::TIME_TO_LIVE, kTTL_LENGTH, buff + offset);
    *(ttv.GetValue() + 1) = kTIME_TO_LIVE;
    offset += ttv.GetSize();
}

void LldpAnnouncer::BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype, std::vector<std::string*>& strings) {
    // Build an organizationally specific tlv with the specified strings as values
    uint16_t value_offset = 0;
    uint16_t tlv_size = 0;

    for (auto& it : strings) {
        // Length plus one since it will be encoded as length, string
        tlv_size += it->length() + 1;
    }

    OrgLldpTlv org_tlv(subtype, tlv_size, buff + offset);

    for (auto& it : strings) {
        org_tlv.SetString(value_offset, *it);
    }

    offset += org_tlv.GetSize();
}

void LldpAnnouncer::BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype) {
    // Build an organizationally specific tlv with serial number in string form and WWN in numeric form
    uint16_t value_offset = 0;
    uint16_t tlv_size = 0;

    // Length plus one since it will be encoded as length, string
    tlv_size += drive_serial_number_.length() + 1;
    tlv_size += drive_wwn_.size() + 1;

    OrgLldpTlv org_tlv(subtype, tlv_size, buff + offset);

    org_tlv.SetString(value_offset, drive_serial_number_);

    uint8_t* value_ptr = org_tlv.GetValue();

    // Set size
    *(value_ptr + value_offset) = drive_wwn_.size();
    ++value_offset;

    // Set wwn
    for (auto& it : drive_wwn_) {
        *(value_ptr + value_offset) = it;
        ++value_offset;
    }

    offset += org_tlv.GetSize();
}

void LldpAnnouncer::BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype, std::string& addr,
                                    int addr_format) {
    // Build an organizationally specific tlv with the string converted to a ip address
    uint16_t tlv_size;

    switch (addr_format) {
        case AF_INET:
            // 32 bit IPv4 address
            tlv_size = 4;
            break;
        case AF_INET6:
            // 128 bit IPv6 address
            tlv_size = 16;
            break;
        default:
            // Unknown address type
            tlv_size = 0;
            break;
    }

    OrgLldpTlv org_tlv(subtype, tlv_size, buff + offset);

    int res = inet_pton(addr_format, addr.c_str(), (void*) org_tlv.GetValue());
    if (res < 0) {
        LOG(WARNING) << "Address cannot be converted to numeric binary form: Unknown address format argument";
    } else if (res == 0 && addr.length() != 0) {
        LOG(WARNING) << "Address cannot be converted to numeric binary form: Address string is not formatted correctly";
    }
    offset += org_tlv.GetSize();
}

void LldpAnnouncer::BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype, const std::string& interface,
                                std::string& mac_addr) {
    // Build an organizationally specific tlv with the port index and mac address
    uint8_t port;
    if (interface == kETH0_NAME) {
        port = 0;
    } else if (interface == kETH1_NAME) {
        port = 1;
    } else {
        // Unknown interface, return
        return;
    }

    // Convert mac address to numeric form
    std::vector<uint8_t> bytes;
    StringToNumeric(mac_addr, bytes);

    // Create TLV, length is 7 since 1 byte is used for the port and 6 bytes are used for the mac address
    OrgLldpTlv org_tlv(subtype, 7, buff + offset);
    uint8_t* value_ptr = org_tlv.GetValue();

    // Set port
    *value_ptr = port;
    ++value_ptr;

    // Set mac address
    for (auto& byte : bytes) {
        *value_ptr = byte;
        ++value_ptr;
    }

    offset += org_tlv.GetSize();
}

void LldpAnnouncer::StringToNumeric(std::string& src, std::vector<uint8_t>& dest) {
    dest.clear();
    for (unsigned int i = 0; i < src.length();) {
        uint8_t byte = 0;

        for (int j = 1; j >= 0; ++i) {
            if (i == src.length()) {
                break;
            } else if (src[i] >= '0' && src[i] <= '9') {
                byte += (src[i] - '0') << (4 * j);
            } else if (src[i] >= 'a' && src[i] <= 'f') {
                byte += (src[i] - 'a' + 10) << (4 * j);
            } else if (src[i] >= 'A' && src[i] <= 'F') {
                byte += (src[i] - 'A' + 10) << (4 * j);
            } else {
                continue;
            }
            --j;
        }

        dest.push_back(byte);
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
