#include <string.h>
#include "includes/lldp_tlv.h"

namespace lldp_kin {

LldpTlv::LldpTlv(TlvType type, uint16_t length, uint8_t* buff)
    : buff_(buff), size_(-1) {
        SetType(type);
        SetLength(length);
    }

void LldpTlv::SetType(TlvType type) {
    uint8_t t;

    switch (type) {
        case TlvType::CHASSIS_ID:
            t = 1;
            break;
        case TlvType::PORT_ID:
            t = 2;
            break;
        case TlvType::TIME_TO_LIVE:
            t = 3;
            break;
        case TlvType::ORGANIZATIONALLY_SPECIFIC:
            t = 127;
            break;
        default:
            t = 0;
            break;
    }

    *buff_ = t << 1;
}

void LldpTlv::SetLength(uint16_t length) {
    uint16_t mask;

    // Set bit 0 of byte 0
    mask = 1 << 9;
    *buff_ = *buff_ | (length & mask);

    // Set byte 1
    mask = ~0 >> 8;
    *(buff_ + 1) = (uint8_t) (length & mask);

    // Set size member to length + 2
    size_ = length + 2;
}

uint8_t* LldpTlv::GetValue() {
    return buff_ + 2;
}

uint16_t LldpTlv::GetSize() {
    return size_;
}

SubtypedLldpTlv::SubtypedLldpTlv(TlvType type, uint16_t length, uint8_t* buff) : LldpTlv(type, length, buff) {}

SubtypedLldpTlv::SubtypedLldpTlv(TlvType type, uint8_t subtype, uint16_t length, uint8_t* buff)
    : LldpTlv(type, length, buff) {
        SetSubtype(subtype);
    }

uint8_t* SubtypedLldpTlv::GetValue() {
    return buff_ + 3;
}

void SubtypedLldpTlv::SetSubtype(uint8_t subtype) {
    *(buff_ + 2) = subtype;
}

OrgLldpTlv::OrgLldpTlv(uint16_t length, uint8_t* buff)
    : SubtypedLldpTlv(TlvType::ORGANIZATIONALLY_SPECIFIC, length, buff) {
        SetOui();
    }

OrgLldpTlv::OrgLldpTlv(uint8_t subtype, uint16_t length, uint8_t* buff)
    : SubtypedLldpTlv(TlvType::ORGANIZATIONALLY_SPECIFIC, length + 4, buff) {
        SetOui();
        SetSubtype(subtype);
    }

void OrgLldpTlv::SetOui() {
    // Set bytes 2-4 to Seagate's OUI
    *(buff_ + 2) = kSEAGATE_OUI_0;
    *(buff_ + 3) = kSEAGATE_OUI_1;
    *(buff_ + 4) = kSEAGATE_OUI_2;
}

void OrgLldpTlv::SetSubtype(uint8_t subtype) {
    *(buff_ + 5) = subtype;
}

uint8_t* OrgLldpTlv::GetValue() {
    return buff_ + 6;
}

void OrgLldpTlv::SetString(uint16_t& offset, std::string& to_set) {
    uint16_t string_len = to_set.length();
    uint8_t* val_ptr = GetValue();
    memset((void*)(val_ptr + offset), string_len, 1);
    ++offset;
    strncpy((char*) val_ptr + offset, to_set.c_str(), string_len);
    offset += string_len;
}

} // namespace lldp_kin
