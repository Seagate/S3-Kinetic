#ifndef KINETIC_LLDP_INCLUDES_LLDP_SOCKET_H_
#define KINETIC_LLDP_INCLUDES_LLDP_SOCKET_H_

#define ETH_P_LLDP 0x88cc

#include <string.h>
#include <cstdint>
#include <net/if.h>
#include <vector>
#include <unordered_map>
#include <netpacket/packet.h>

namespace lldp_kin {

class LldpSocket {
    public:
        explicit LldpSocket(const std::vector<std::string>& iface_names);
        ~LldpSocket();
        std::vector<uint8_t> SetTarget(const std::string& target);
        uint8_t* GetPayloadBuffer();
        void SetPayloadSize(uint16_t size);
        ssize_t SendBuffer();
        uint8_t* Receive(ssize_t& bytes);

    private:
        void BuildEthernetHeader();
        void* SeekToPayload();

        static const uint16_t kMAX_FRAME_SIZE = 1522;
        static const uint8_t kMULTICAST_MAC0 = 0x01;
        static const uint8_t kMULTICAST_MAC1 = 0x80;
        static const uint8_t kMULTICAST_MAC2 = 0xC2;
        static const uint8_t kMULTICAST_MAC3 = 0x00;
        static const uint8_t kMULTICAST_MAC4 = 0x00;
        static const uint8_t kMULTICAST_MAC5 = 0x0E;

        void* buff_;
        int sock_fd_;
        uint16_t packet_size_;
        std::vector<uint8_t> current_mac_;
        std::unordered_map<std::string, struct ifreq> ifaces_;
        struct sockaddr_ll dest_address_;
};

} // namespace lldp_kin

#endif  // KINETIC_LLDP_INCLUDES_LLDP_SOCKET_H_
