#include <vector>
#include <fstream>
#include <chrono>
#include "includes/lldp_listener.h"
#include "includes/lldp_tlv.h"

using namespace std::chrono; // NOLINT

namespace lldp_kin {

const std::string LldpListener::kFILE_PATH = "/mnt/util/mlag.txt";

LldpListener::LldpListener(const std::string& interface)
    : interface_(interface), sock_({interface}), config_received_(false) {
        prev_config_ = ReadConfig();
        sock_.SetTarget(interface_);
    }

bool LldpListener::ListenForConfig() {
    high_resolution_clock::time_point start = high_resolution_clock::now();
    ssize_t bytes;
    uint8_t* buff;

    // Continually loop and read LLDP messages until a new bonding configuration is set
    while (1) {
        bytes = 0;
        buff = sock_.Receive(bytes);

        if (bytes > 0 && ParseMessage(buff, bytes)) {
            // We received a new configuration, return true
            return true;
        } else if (bytes < 0) {
            // Error on read, return false
            return false;
        }

        // If we haven't received a config, the previous config was 1 (bonded), and we've exceeded the timeout,
        // reconfigure to use 0 (non-bonded)
        if (!config_received_ && prev_config_ == 1 &&
            duration_cast<seconds>(high_resolution_clock::now() - start).count() >= kBOND_TIMEOUT) {
            return SetBondingConfig(0);
        }
    }
}

bool LldpListener::ParseMessage(uint8_t* buff, const ssize_t& size) {
    uint8_t type;
    uint16_t length;
    uint32_t seagate_oui = 0;
    ssize_t to_parse = size;

    // Convert individual bytes into a 32 bit unsigned to simplify conditional below
    seagate_oui += OrgLldpTlv::kSEAGATE_OUI_0 << 16;
    seagate_oui += OrgLldpTlv::kSEAGATE_OUI_1 << 8;
    seagate_oui += OrgLldpTlv::kSEAGATE_OUI_2;

    while (to_parse > 0) {
        type = *buff >> 1;
        length = ((*buff & 0x1) << 8) + *(buff + 1);
        to_parse -= 2 + length;
        buff += 2;

        if (type == kORGANIZATIONALLY_SPECIFIC_TYPE) {
            // Convert the three oui bytes to a 32 bit unsigned
            uint32_t oui = (*buff << 16) + (*(buff + 1) << 8) + *(buff + 2);

            // Check if the OUI matches Seagte or Xyratex
            if (oui == seagate_oui || oui == kXYRATEX_OUI) {
                // Move pointer to the subtype field
                buff += 3;
                length -= 3;

                // If subtype is 1, TLV contains bonding configuration
                if (*buff == 1 && ParseConfigurationTlv(buff + 1, length - 1)) {
                    return true;
                }
            }
        } else if (type == 0 && length == 0) {
            // End TLV, break out of parse loop
            break;
        }

        // Move pointer to the next TLV
        buff += length;
    }

    return false;
}

bool LldpListener::ParseConfigurationTlv(uint8_t* buff, const uint16_t& length) {
    // TLV should be 4 bytes long per the spec, if it is not then the TLV is invalid
    if (length != 4) {
        return false;
    }

    // First 3 bytes are reserved so skip them
    buff += 3;

    // Bit 0 indicates LACP support, bit 1 indicates MLAG support, and bit 2 indicates MLAG enabled
    // All three bits must be set for the drive to be able to bond its interfaces
    int config;
    if ((*buff & 0x07) == 0x07) {
        // Enable bonding
        config = 1;
    } else {
        // Disable bonding
        config = 0;
    }

    config_received_ = true;
    return SetBondingConfig(config);
}

bool LldpListener::SetBondingConfig(int config) {
    // Only write to file if the config has changed
    if (config == prev_config_) {
        return false;
    } else {
        std::ofstream file;
        file.open(kFILE_PATH);
        file << std::to_string(config) << "\n";
        file.close();
        prev_config_ = config;
        return true;
    }
}

int LldpListener::ReadConfig() {
    // Read existing config
    std::ifstream file;
    file.open(kFILE_PATH);

    if (file.is_open()) {
        std::string config;
        file >> config;
        return std::stoi(config);
    } else {
        return 0;
    }
}

} // namespace lldp_kin
