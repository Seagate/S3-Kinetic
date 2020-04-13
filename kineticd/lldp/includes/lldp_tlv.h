#ifndef KINETIC_LLDP_INCLUDES_LLDP_TLV_H_
#define KINETIC_LLDP_INCLUDES_LLDP_TLV_H_

#include <string>
#include <cstdint>

namespace lldp_kin {

enum class TlvType {
    END,
    CHASSIS_ID,
    PORT_ID,
    TIME_TO_LIVE,
    ORGANIZATIONALLY_SPECIFIC
};

class LldpTlv {
    public:
        LldpTlv(TlvType type, uint16_t length, uint8_t* buff);
        virtual uint8_t* GetValue();
        uint16_t GetSize();
        void SetType(TlvType type);
        void SetLength(uint16_t length);

    protected:
        uint8_t* buff_;
        uint16_t size_;
};

class SubtypedLldpTlv : public LldpTlv {
    public:
        SubtypedLldpTlv(TlvType type, uint16_t length, uint8_t* buff);
        SubtypedLldpTlv(TlvType type, uint8_t subtype, uint16_t length, uint8_t* buff);
        virtual void SetSubtype(uint8_t subtype);
        virtual uint8_t* GetValue();
};

class OrgLldpTlv : public SubtypedLldpTlv {
    public:
        OrgLldpTlv(uint16_t length, uint8_t* buff);
        OrgLldpTlv(uint8_t subtype, uint16_t length, uint8_t* buff);
        virtual void SetSubtype(uint8_t subtype);
        virtual uint8_t* GetValue();
        virtual void SetString(uint16_t& offset, std::string& to_set);

        static const uint8_t kSEAGATE_OUI_0 = 0x00;
        static const uint8_t kSEAGATE_OUI_1 = 0x11;
        static const uint8_t kSEAGATE_OUI_2 = 0xC6;

    private:
        void SetOui();
};

} // namespace lldp_kin

#endif  // KINETIC_LLDP_INCLUDES_LLDP_TLV_H_
