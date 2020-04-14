#ifndef KINETIC_LLDP_ANNOUNCER_H_
#define KINETIC_LLDP_ANNOUNCER_H_

#include <string.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "lldp_socket.h"
#include "runnable_interface.h"
#include "announcer_interface.h"

using ::lldp_kin::LldpSocket;
using namespace com::seagate::common; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class LldpAnnouncer : public AnnouncerInterface, public RunnableInterface {
    public:
        LldpAnnouncer(std::vector<std::string>& iface_names,
                      NetworkInterfaces& network_interfaces,
                      DeviceInformationInterface& device_information);
        virtual ~LldpAnnouncer() {
        }
        virtual bool Configure();
        virtual void Announce();
        virtual void run();

        // Interface names
        static const std::string kBOND_NAME;
        static const std::string kETH0_NAME;
        static const std::string kETH1_NAME;

    private:
        void BuildLldpdu(const std::string& interface, std::vector<std::string>& addresses,
                         std::vector<uint8_t>& mac_addr);
        void BuildChassisIdTlv(uint16_t& offset, uint8_t* buff, std::vector<uint8_t>& mac_addr);
        void BuildPortIdTlv(uint16_t& offset, uint8_t* buff, const std::string& interface);
        void BuildTimeToLiveTlv(uint16_t& offset, uint8_t* buff);
        void BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype);
        void BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype, std::vector<std::string*>& strings);
        void BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype, std::string& addr, int addr_format);
        void BuildOrgTlv(uint16_t& offset, uint8_t* buff, uint8_t subtype, const std::string& interface,
                         std::string& mac_addr);
        bool LoadDriveIdentification();
        void StringToNumeric(std::string& src, std::vector<uint8_t>& dest);

        static const uint16_t kCHASSIS_ID_LENGTH = 7;
        static const uint16_t kPORT_ID_LENGTH = 5;
        static const uint16_t kTTL_LENGTH = 2;
        static const uint16_t kORGANIZATIONALLY_SPECIFIC_LENGTH = 511;
        static const uint8_t kTIME_TO_LIVE = 120;
        static const uint8_t kFAST_XMIT_PERIOD = 5;         // Fast transmit period in seconds
        static const uint8_t kNORMAL_XMIT_PERIOD = 30;      // Normal transmit period in seconds
        static const uint8_t kFAST_XMITS = 3;               // Number of fast transmits before slowing down

        std::unordered_map<std::string, std::vector<std::string>> iface_map_;
        NetworkInterfaces& network_interfaces_;
        DeviceInformationInterface& device_information_;
        LldpSocket sock_;
        std::string drive_serial_number_;
        std::vector<uint8_t> drive_wwn_;
        std::string drive_vendor_;
        std::string drive_model_;
        std::string drive_firmware_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_LLDP_ANNOUNCER_H_
