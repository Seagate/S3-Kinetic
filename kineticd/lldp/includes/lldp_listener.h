#ifndef KINETIC_LLDP_INCLUDES_LLDP_LISTENER_H_
#define KINETIC_LLDP_INCLUDES_LLDP_LISTENER_H_

#include <cstdint>
#include "lldp_socket.h"

namespace lldp_kin {

class LldpListener {
    public:
        explicit LldpListener(const std::string& interface);
        bool ListenForConfig();

    private:
        bool ParseMessage(uint8_t* buff, const ssize_t& size);
        bool ParseConfigurationTlv(uint8_t* buff, const uint16_t& length);
        bool SetBondingConfig(int config);
        int ReadConfig();

        static const uint8_t kBOND_TIMEOUT = 60;
        static const uint8_t kORGANIZATIONALLY_SPECIFIC_TYPE = 127;
        static const uint32_t kXYRATEX_OUI = 20684;
        static const std::string kFILE_PATH;

        std::string interface_;
        LldpSocket sock_;
        int prev_config_;
        bool config_received_;
};

} // namespace lldp_kin

#endif  // KINETIC_LLDP_INCLUDES_LLDP_LISTENER_H_
