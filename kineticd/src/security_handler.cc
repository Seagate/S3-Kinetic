#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include "security_handler.h"

#include "glog/logging.h"
#include "openssl/sha.h"
#include "product_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

// Public modulus for the rsa decryption
const int SecurityHandler::LODHEADER_SIZE = 0x40;

const uint8_t SecurityHandler::KINETIC_MODULUS[] = {
    0xde, 0x73, 0x06, 0x15, 0xff, 0xe3, 0x56, 0x8a,
    0xc5, 0xb3, 0x16, 0x25, 0xed, 0x8b, 0xe5, 0x1f,
    0x62, 0x4e, 0xab, 0xb0, 0xbc, 0x7f, 0xaf, 0x1c,
    0xf1, 0x08, 0x09, 0xe1, 0x05, 0xfe, 0xe8, 0xb3,
    0x4e, 0x99, 0xd8, 0xd2, 0xf0, 0x9e, 0x1b, 0xae,
    0xa7, 0xcc, 0x19, 0x1d, 0x84, 0x6f, 0x65, 0xe6,
    0xc5, 0x67, 0x33, 0xc6, 0xec, 0xa4, 0xdb, 0x09,
    0x4d, 0x9c, 0x77, 0x24, 0x89, 0xb3, 0x87, 0x87,
    0xc1, 0x65, 0xf9, 0x6c, 0xa5, 0xf4, 0x26, 0xc7,
    0xd3, 0xa5, 0xfc, 0xc4, 0x28, 0xba, 0x25, 0x15,
    0x58, 0x28, 0xc6, 0x18, 0x56, 0x4a, 0x60, 0xdb,
    0xb0, 0xe7, 0x31, 0xa0, 0x2b, 0x38, 0xff, 0x60,
    0xf3, 0x99, 0x58, 0xdb, 0xe8, 0x78, 0xa3, 0x48,
    0xe3, 0x03, 0x36, 0x57, 0x53, 0x60, 0x37, 0x8f,
    0x19, 0x66, 0x35, 0x70, 0x0f, 0x5f, 0x56, 0xb9,
    0xe2, 0xac, 0xca, 0x87, 0x29, 0xe3, 0x28, 0xe1,
    0x7e, 0xda, 0x6e, 0x93, 0xee, 0xdd, 0x52, 0x08,
    0x43, 0x4d, 0x78, 0x85, 0x4b, 0xc7, 0x3f, 0x6f,
    0xe9, 0x82, 0xdb, 0xb3, 0x71, 0xb7, 0x8a, 0x0e,
    0x20, 0xdb, 0x09, 0xde, 0xda, 0x2c, 0x43, 0x1a,
    0xa6, 0xfd, 0x62, 0x05, 0x98, 0xb7, 0x86, 0x63,
    0xb0, 0x81, 0xc4, 0x78, 0x32, 0x26, 0x9c, 0xa4,
    0x26, 0x76, 0x57, 0xd6, 0x66, 0x7f, 0xca, 0xab,
    0x02, 0x9c, 0xd0, 0x32, 0x01, 0x37, 0xdc, 0x69,
    0x9d, 0x8d, 0x3e, 0xe2, 0x88, 0x8e, 0x2d, 0x11,
    0xa7, 0xa9, 0x11, 0x0b, 0x51, 0xc7, 0xa3, 0x54,
    0x99, 0x44, 0x08, 0xfb, 0x90, 0x6a, 0x7b, 0x16,
    0x50, 0x40, 0x69, 0x59, 0x8c, 0xad, 0x67, 0x41,
    0x4b, 0x09, 0xb3, 0xe4, 0x2b, 0xcf, 0x1f, 0x48,
    0xaf, 0x4e, 0xd9, 0x9b, 0x80, 0xbc, 0x03, 0x04,
    0xa2, 0x40, 0x77, 0x0e, 0x17, 0x16, 0xe4, 0x9c,
    0xcf, 0xa6, 0xf7, 0xdc, 0xe3, 0x2a, 0x0a, 0x91
};

#if defined(PRODUCT_LOMBARDKV)
const uint8_t SecurityHandler::KINETIC_MODULUS_KEY0[] = {
    0xb0, 0x5e, 0x67, 0x00, 0x43, 0xa7, 0xdd, 0x6b,
    0x93, 0xf7, 0x79, 0x53, 0xb8, 0x3f, 0x4d, 0xb2,
    0x39, 0x4e, 0x32, 0xaf, 0xb8, 0xbe, 0xea, 0xd3,
    0xb6, 0x09, 0xbf, 0x22, 0x54, 0x98, 0xf3, 0xa4,
    0x90, 0x87, 0x04, 0x6c, 0x71, 0x55, 0x90, 0xe1,
    0xa5, 0xf0, 0x11, 0xbf, 0x02, 0xb1, 0x22, 0x53,
    0x43, 0x83, 0xb3, 0xba, 0xa1, 0x1b, 0x37, 0x65,
    0x99, 0xaa, 0x2a, 0xb5, 0xd0, 0x42, 0x9a, 0xce,
    0x7f, 0xda, 0x21, 0x12, 0x53, 0x5d, 0x23, 0x10,
    0x41, 0x7d, 0xdc, 0x5f, 0xe5, 0x15, 0x03, 0xb8,
    0xea, 0x7d, 0x48, 0x03, 0xbd, 0xf5, 0xbe, 0x8e,
    0x2e, 0xae, 0x4f, 0x28, 0x9e, 0x81, 0xd0, 0x20,
    0x18, 0x50, 0x60, 0xce, 0x5d, 0xf9, 0x79, 0x48,
    0x10, 0xd6, 0x9f, 0xd5, 0xbe, 0xbc, 0x6c, 0x9b,
    0x0e, 0xec, 0x02, 0xfb, 0xe7, 0xe3, 0x47, 0xcc,
    0x2b, 0x30, 0x66, 0x42, 0xe6, 0x5e, 0xcf, 0x30,
    0x4c, 0x1c, 0xfb, 0x66, 0x99, 0xa4, 0x64, 0x98,
    0xc4, 0xb2, 0x6e, 0x71, 0xfb, 0xc6, 0x6c, 0x41,
    0x7f, 0xfb, 0x8d, 0xae, 0xc9, 0x94, 0xa9, 0x7e,
    0xf7, 0xcd, 0x91, 0x1d, 0x7d, 0xc7, 0xd2, 0xe4,
    0x00, 0xeb, 0x0b, 0x9f, 0xa6, 0x41, 0x41, 0x01,
    0x05, 0xec, 0x5c, 0x4f, 0x4f, 0xf6, 0x87, 0x8a,
    0xdd, 0xad, 0xc8, 0x09, 0xcf, 0xe6, 0x19, 0x35,
    0x9d, 0x52, 0x64, 0x63, 0x78, 0xfd, 0x5f, 0xa9,
    0x32, 0x45, 0x51, 0x0d, 0x0e, 0xd6, 0xb8, 0x30,
    0x7e, 0x4d, 0x73, 0xf7, 0x42, 0x6e, 0x28, 0xb0,
    0x84, 0x24, 0x34, 0x7a, 0x5b, 0x43, 0x20, 0x7d,
    0x75, 0x8a, 0xe8, 0x03, 0x12, 0x45, 0x2a, 0xce,
    0x24, 0x12, 0x4a, 0xc6, 0xb7, 0x40, 0xd7, 0x95,
    0xea, 0xf8, 0x9e, 0x9e, 0xdf, 0x92, 0x26, 0x35,
    0x21, 0xf3, 0xbf, 0xef, 0x65, 0x4a, 0xf6, 0x4b,
    0x46, 0x0f, 0xa3, 0xe5, 0x97, 0x06, 0xda, 0x1d
};

const uint8_t SecurityHandler::KINETIC_MODULUS_KEY1[] = {
    0xb8, 0x25, 0x44, 0xa8, 0xf5, 0x7f, 0x30, 0x39,
    0x49, 0xc8, 0x62, 0x2e, 0xb4, 0xb2, 0x2f, 0x90,
    0x57, 0xa1, 0xcd, 0x57, 0x09, 0x7c, 0xf0, 0xe8,
    0x2b, 0xf3, 0x7b, 0x0a, 0xc9, 0x31, 0xc0, 0xf7,
    0x6d, 0x95, 0x68, 0x27, 0xfc, 0x8c, 0xa2, 0x36,
    0x4b, 0x58, 0x29, 0x50, 0x0a, 0xb3, 0x66, 0x93,
    0x79, 0xc2, 0x53, 0x57, 0x51, 0x9c, 0x35, 0x63,
    0xda, 0xdc, 0x17, 0x61, 0x83, 0xcb, 0xd6, 0x94,
    0x51, 0x3a, 0xce, 0x8f, 0x9c, 0x9a, 0x22, 0x1c,
    0x37, 0x72, 0x0e, 0xb6, 0x30, 0x2c, 0x25, 0x95,
    0x97, 0xae, 0x64, 0xe6, 0x70, 0x7e, 0xbd, 0x00,
    0x3f, 0xda, 0x4c, 0x97, 0xcc, 0x13, 0x9c, 0x09,
    0xfa, 0x83, 0x18, 0xde, 0x53, 0x27, 0xd7, 0x1b,
    0x0d, 0xba, 0x56, 0x67, 0xb5, 0xdc, 0x10, 0x97,
    0xd0, 0x61, 0x30, 0xba, 0x34, 0x6d, 0x7e, 0xf3,
    0xd3, 0x88, 0x7e, 0x2e, 0x2f, 0x6c, 0xf5, 0xb3,
    0xc1, 0xb8, 0x2f, 0x80, 0xba, 0xea, 0xee, 0xf9,
    0x67, 0xba, 0x7c, 0x7f, 0x4d, 0xe7, 0x44, 0xc6,
    0xa7, 0x34, 0x1f, 0xec, 0xaa, 0xd9, 0xc2, 0x46,
    0x24, 0x24, 0xfc, 0x5d, 0xa5, 0xe3, 0x36, 0xb4,
    0x09, 0x0a, 0x58, 0x39, 0x27, 0xce, 0x96, 0xc3,
    0x36, 0xc2, 0x86, 0x70, 0xd8, 0x0f, 0x00, 0x27,
    0x28, 0x0d, 0x13, 0xaf, 0x7d, 0xf2, 0x9e, 0xce,
    0x68, 0xec, 0xbf, 0x75, 0x9b, 0xc4, 0x94, 0x07,
    0x98, 0xee, 0xac, 0x91, 0x80, 0xa5, 0x56, 0x32,
    0x37, 0xef, 0x8f, 0xf8, 0x8b, 0x27, 0x15, 0xae,
    0xd1, 0x90, 0x88, 0x14, 0x76, 0xb7, 0x87, 0x1c,
    0x71, 0xb8, 0x94, 0x1f, 0x54, 0xe3, 0xd5, 0x47,
    0x22, 0x30, 0x86, 0xcf, 0x4f, 0xf0, 0x73, 0xcb,
    0x5a, 0x13, 0x95, 0xa6, 0x9e, 0xe1, 0xfe, 0x54,
    0xbb, 0xdb, 0xa6, 0x73, 0x08, 0x93, 0x29, 0x2a,
    0x8d, 0x22, 0x08, 0xd0, 0x1d, 0x92, 0x5e, 0xc7
};


// Reserved for China Builds
#elif PRODUCT_LOMBARDKV_CHINA
const uint8_t SecurityHandler::KINETIC_MODULUS_KEY0[] = {
    0xbf, 0x2c, 0x5f, 0x27, 0xa4, 0x67, 0xd5, 0x3c,
    0xc0, 0xa2, 0xee, 0xaa, 0xdd, 0xb7, 0x87, 0xa1,
    0xb1, 0x90, 0x96, 0x1a, 0x00, 0xab, 0x2a, 0x27,
    0x05, 0x81, 0xbc, 0x91, 0xdc, 0x99, 0x89, 0xf2,
    0xd7, 0x1e, 0x22, 0xa2, 0xc7, 0xac, 0xeb, 0x05,
    0x41, 0xa4, 0x56, 0xd7, 0xe6, 0x6a, 0x6b, 0xdb,
    0x0d, 0xae, 0x9f, 0xac, 0x2b, 0xac, 0xfd, 0xfb,
    0x1a, 0xfc, 0xa3, 0xc4, 0x8b, 0x05, 0x22, 0x40,
    0xe3, 0x32, 0x16, 0x12, 0x7c, 0xea, 0x0b, 0xc6,
    0x21, 0x00, 0x43, 0xb2, 0xf4, 0x2c, 0xa3, 0x6d,
    0x0c, 0x3f, 0xc9, 0xab, 0x43, 0xc4, 0x1d, 0x2c,
    0x66, 0x76, 0x7e, 0x39, 0xdb, 0xfa, 0x77, 0xda,
    0xb1, 0xae, 0x60, 0xf0, 0xd6, 0x45, 0x90, 0x55,
    0x48, 0xcf, 0x53, 0x50, 0x03, 0x55, 0x78, 0x3f,
    0xae, 0x26, 0x51, 0x2f, 0xbf, 0xdb, 0x54, 0x43,
    0xf4, 0x4e, 0x05, 0x6e, 0xda, 0x79, 0x72, 0x68,
    0xcf, 0xba, 0x3a, 0xde, 0x69, 0x5a, 0xf6, 0xc1,
    0x42, 0xcc, 0x7c, 0x22, 0x78, 0xdf, 0x42, 0x80,
    0xf7, 0xb0, 0x6a, 0xfe, 0x98, 0x37, 0x7f, 0xe3,
    0xcd, 0x0f, 0xb9, 0x6d, 0xcd, 0xa0, 0x57, 0x04,
    0x4c, 0x1c, 0x68, 0xa9, 0x86, 0xd8, 0xe5, 0x40,
    0xe3, 0xc9, 0xdc, 0x3b, 0x2e, 0xdc, 0x86, 0xce,
    0x1a, 0x3a, 0xb3, 0x8c, 0x5a, 0xfa, 0x0d, 0x04,
    0x10, 0xbd, 0x61, 0x6f, 0x5e, 0xab, 0x46, 0x73,
    0x45, 0x86, 0xa8, 0x69, 0x0f, 0x56, 0xeb, 0xf3,
    0x75, 0xbf, 0x56, 0x8e, 0xf3, 0x7b, 0x7f, 0x45,
    0x2b, 0x3c, 0x2c, 0xb1, 0x6b, 0xf7, 0xad, 0xfa,
    0xfb, 0xc0, 0x19, 0xe8, 0x56, 0xad, 0xdd, 0xb7,
    0xd7, 0x4b, 0x72, 0x88, 0x62, 0x23, 0x8a, 0x35,
    0xe8, 0x93, 0x7d, 0x85, 0x4b, 0x8f, 0x50, 0x92,
    0x16, 0x30, 0x04, 0xa6, 0x17, 0x53, 0x72, 0x25,
    0xb6, 0xa4, 0x6d, 0xe1, 0x7b, 0x0a, 0x10, 0x51
};

const uint8_t SecurityHandler::KINETIC_MODULUS_KEY1[] = {
    0xe0, 0x4a, 0x57, 0xff, 0xf4, 0xfd, 0xc4, 0x6c,
    0x87, 0x4f, 0x84, 0xb6, 0xc0, 0xcd, 0x11, 0xf4,
    0xfe, 0x9b, 0x5f, 0x53, 0xbf, 0xe5, 0x4e, 0x17,
    0x07, 0xe7, 0x9a, 0x06, 0x04, 0xad, 0x95, 0x27,
    0xda, 0x53, 0x2e, 0xbb, 0x1c, 0xe9, 0xa5, 0xb4,
    0x5a, 0x6d, 0x4e, 0x5d, 0x52, 0xec, 0x51, 0x14,
    0xcc, 0x6b, 0xbf, 0x02, 0xeb, 0x82, 0x50, 0xb7,
    0x66, 0xa4, 0x52, 0x58, 0x44, 0x2a, 0xa1, 0xbe,
    0x21, 0xbd, 0x33, 0xac, 0x86, 0xaa, 0x91, 0x0b,
    0xad, 0xee, 0x43, 0x5f, 0xaf, 0x3f, 0x98, 0x3f,
    0x05, 0x3d, 0x21, 0x41, 0x1f, 0xcf, 0x13, 0x20,
    0x74, 0xaf, 0x95, 0x59, 0x21, 0x84, 0x40, 0x3d,
    0x61, 0x2d, 0x44, 0x64, 0xb8, 0xbb, 0x5f, 0x0e,
    0x45, 0xb3, 0xb1, 0xab, 0xe0, 0x89, 0x32, 0x53,
    0x21, 0x11, 0xab, 0x32, 0x9d, 0x7b, 0x5c, 0x62,
    0x2f, 0xba, 0x0e, 0x88, 0x1c, 0xf3, 0x95, 0x6b,
    0xb0, 0x7a, 0xde, 0xa2, 0x3c, 0x2c, 0xff, 0xf8,
    0x2a, 0x99, 0x44, 0xcf, 0xd7, 0x78, 0x73, 0x99,
    0xaf, 0xce, 0xc0, 0x98, 0xaa, 0xb7, 0x1a, 0xf9,
    0x36, 0xb9, 0x44, 0xab, 0x38, 0x33, 0x41, 0x4f,
    0x9a, 0x30, 0x7e, 0x07, 0x27, 0x27, 0xf5, 0xdf,
    0x23, 0x07, 0x41, 0x52, 0x84, 0xea, 0x9c, 0xdf,
    0x01, 0x9a, 0x60, 0x02, 0x4d, 0x20, 0xd5, 0x1b,
    0xea, 0xaa, 0xd0, 0x63, 0xdc, 0xd3, 0x94, 0x84,
    0x58, 0x62, 0xb6, 0xcd, 0x77, 0xf2, 0x77, 0x78,
    0x91, 0xf7, 0x14, 0x00, 0x85, 0x8b, 0xb4, 0x72,
    0x9c, 0x33, 0xf3, 0x7f, 0xf8, 0x55, 0xbb, 0x3a,
    0x18, 0xa3, 0xbb, 0x48, 0x50, 0xcb, 0x85, 0xe8,
    0x61, 0xdc, 0x3b, 0xd0, 0x7a, 0x67, 0x28, 0xb2,
    0xeb, 0xd7, 0x69, 0x6a, 0x2e, 0x5a, 0x6f, 0x6b,
    0xbc, 0x79, 0x5b, 0x55, 0x8f, 0x0f, 0xc8, 0xe3,
    0x26, 0x2b, 0xaf, 0xf0, 0x93, 0x77, 0x18, 0x0d
};

#elif PRODUCT_LAMARRKV || PRODUCT_LAMARRKV_QUAL || PRODUCT_LAMARRKV_ARMADALP || defined(PRODUCT_X86)
const uint8_t SecurityHandler::KINETIC_MODULUS_KEY0[] = {
    0xdf, 0xb7, 0xb7, 0x7b, 0xaa, 0x2f, 0xf9, 0xb4,
    0xaf, 0x76, 0x50, 0x82, 0xa3, 0x62, 0xd5, 0x19,
    0xac, 0xf6, 0x99, 0xc9, 0x62, 0xfc, 0xdb, 0x93,
    0xc7, 0xd2, 0x32, 0xdb, 0x8a, 0x5f, 0x5c, 0xd6,
    0x66, 0xa8, 0x68, 0x93, 0x0b, 0x1f, 0xfd, 0x9d,
    0xba, 0x60, 0xd7, 0xed, 0xec, 0x7a, 0x80, 0x29,
    0x16, 0xf5, 0x27, 0xce, 0x4d, 0x07, 0xad, 0x03,
    0xe6, 0x29, 0x79, 0x04, 0x45, 0xe0, 0xb0, 0x98,
    0x41, 0xfe, 0x6d, 0xfe, 0x91, 0x79, 0x9d, 0x09,
    0x59, 0x49, 0xa7, 0xf8, 0xbf, 0xc6, 0xf0, 0xe8,
    0x71, 0x68, 0xd7, 0x23, 0x54, 0x19, 0x41, 0xd4,
    0x15, 0x1e, 0x18, 0xe0, 0x7c, 0x11, 0xc2, 0x0c,
    0xed, 0xa7, 0xd5, 0xab, 0xd1, 0x9e, 0x1d, 0xb7,
    0xc7, 0x6a, 0x7f, 0x3f, 0x37, 0xc2, 0x55, 0x74,
    0x78, 0x71, 0xd8, 0xa0, 0xc5, 0x19, 0x7d, 0xb2,
    0x82, 0x61, 0xb7, 0xf6, 0x99, 0xac, 0x07, 0xb4,
    0x50, 0xac, 0xbc, 0x7d, 0x14, 0x3b, 0x55, 0x9c,
    0x3a, 0x81, 0x9c, 0xfc, 0x87, 0x59, 0xa7, 0xd1,
    0xc4, 0xed, 0x64, 0x8a, 0xa7, 0x57, 0x58, 0x11,
    0x91, 0xa1, 0xc9, 0x34, 0x77, 0xb5, 0x0e, 0x94,
    0x00, 0x0c, 0x4e, 0x64, 0xec, 0x7c, 0x3b, 0x66,
    0xff, 0x88, 0xf5, 0xa1, 0x50, 0xd4, 0x54, 0xb4,
    0xd0, 0x75, 0x98, 0x70, 0x6a, 0x65, 0x9f, 0x11,
    0xa1, 0xdb, 0x2b, 0x87, 0x1b, 0x17, 0x35, 0xc8,
    0x36, 0x08, 0xae, 0xaa, 0xd3, 0xb4, 0xc3, 0xd9,
    0xb6, 0xb4, 0x00, 0x46, 0xdf, 0xc0, 0xc8, 0x7d,
    0x58, 0x8d, 0x7e, 0x1b, 0x77, 0xbd, 0x16, 0xc2,
    0xb6, 0x89, 0x44, 0xda, 0x80, 0xa5, 0xc0, 0x94,
    0x43, 0x69, 0x89, 0xbf, 0xae, 0x87, 0x24, 0x87,
    0x4d, 0x06, 0x45, 0x96, 0xd8, 0x47, 0x94, 0x8d,
    0x83, 0x49, 0xdb, 0x7d, 0x46, 0x26, 0x59, 0x20,
    0xd9, 0x3f, 0x59, 0x65, 0xfe, 0xb5, 0x59, 0x2f
};

const uint8_t SecurityHandler::KINETIC_MODULUS_KEY1[] = {
    0xd6, 0x11, 0x33, 0xa9, 0x5f, 0x71, 0x03, 0x1c,
    0xbc, 0xe4, 0xb5, 0x24, 0x8c, 0x1e, 0x61, 0xf3,
    0x3a, 0x50, 0x41, 0x1b, 0xb5, 0xd8, 0xcb, 0xe1,
    0x89, 0x72, 0xf8, 0x9c, 0x86, 0x2a, 0x3a, 0xc3,
    0x48, 0x6d, 0xdf, 0x29, 0xe7, 0x6f, 0x59, 0xaa,
    0x53, 0x48, 0xea, 0xa7, 0xba, 0x2f, 0xca, 0xb8,
    0x25, 0x02, 0xa7, 0xca, 0xb5, 0x95, 0x13, 0x7c,
    0x38, 0x68, 0x99, 0xb8, 0xd5, 0x0b, 0xc6, 0x4f,
    0x22, 0xef, 0x10, 0xfe, 0x2a, 0xc6, 0xf7, 0x80,
    0x73, 0x4c, 0xb3, 0x54, 0x88, 0x96, 0xd6, 0xc6,
    0xce, 0x7c, 0xc8, 0xe2, 0x87, 0xc2, 0xb1, 0x39,
    0xd7, 0xb5, 0xd6, 0x05, 0xea, 0x77, 0xb7, 0x5f,
    0x47, 0x57, 0xeb, 0x8a, 0x27, 0x79, 0xf8, 0x35,
    0x84, 0xc8, 0x39, 0xe6, 0x5e, 0xcf, 0x17, 0x52,
    0x7e, 0x7e, 0x22, 0x89, 0xe0, 0x6c, 0xb6, 0x88,
    0x7e, 0xf0, 0xf4, 0xe8, 0xae, 0xcf, 0xed, 0xa9,
    0xbf, 0xee, 0xf6, 0x01, 0x4e, 0x3d, 0xa7, 0xc1,
    0x76, 0x83, 0x9f, 0xef, 0x2e, 0x47, 0x5a, 0x99,
    0xa3, 0x45, 0xee, 0xb7, 0x15, 0xb3, 0xdb, 0x42,
    0x38, 0xe6, 0xc6, 0x9d, 0x9c, 0x7c, 0xec, 0xa2,
    0xdb, 0x2e, 0x65, 0x13, 0x5a, 0x64, 0x06, 0x38,
    0x3e, 0x78, 0x36, 0x84, 0xc9, 0x13, 0x21, 0x4c,
    0x20, 0x34, 0xee, 0xc8, 0xd3, 0x28, 0x08, 0x65,
    0xa6, 0xed, 0xae, 0x25, 0x2d, 0xfd, 0xab, 0xb5,
    0xd9, 0x00, 0x5b, 0x5d, 0x1f, 0x44, 0x84, 0x20,
    0x3e, 0xc9, 0x82, 0x76, 0xab, 0xbd, 0x11, 0x99,
    0x8d, 0x1c, 0x30, 0x7a, 0xf8, 0x29, 0xd8, 0x96,
    0xfb, 0xb0, 0xb5, 0x12, 0x7c, 0x2a, 0x7a, 0x9d,
    0x7d, 0x3f, 0x8b, 0xee, 0x40, 0xe4, 0x08, 0x2f,
    0xad, 0xc3, 0x5f, 0x52, 0xdc, 0x88, 0xe9, 0x64,
    0x9f, 0x38, 0x42, 0x9f, 0xe8, 0x15, 0xf8, 0xc6,
    0xf3, 0x83, 0x60, 0x43, 0x9d, 0x22, 0x75, 0xb1
};

// const uint8_t SecurityHandler::KINETIC_MODULUS_LAMARR2[] = {
//     0xe7, 0xcc, 0x74, 0x90, 0x8a, 0x48, 0xae, 0x39,
//     0xcb, 0xfc, 0xbf, 0x01, 0xe1, 0x44, 0x8e, 0x5c,
//     0xc0, 0x98, 0xac, 0xdc, 0xdf, 0x62, 0x1b, 0xcd,
//     0x8f, 0x51, 0x83, 0x8f, 0xf5, 0xd3, 0x06, 0xe2,
//     0x6b, 0x02, 0x5d, 0x0c, 0xf2, 0xf7, 0xa8, 0x06,
//     0x4a, 0xcd, 0x54, 0x08, 0xe3, 0x24, 0x13, 0x6d,
//     0x49, 0x27, 0x6d, 0xd4, 0xf3, 0xa3, 0xe9, 0x89,
//     0xcf, 0x5c, 0x68, 0x0b, 0xdb, 0xc2, 0x82, 0x0f,
//     0xfe, 0x78, 0x55, 0xb9, 0x7c, 0xc7, 0xad, 0xad,
//     0xf5, 0x85, 0x13, 0x82, 0x06, 0xe2, 0xe1, 0x6e,
//     0x1f, 0x2e, 0x54, 0x08, 0x6e, 0xc4, 0x83, 0xfe,
//     0x7d, 0x9a, 0x4f, 0x11, 0x24, 0x63, 0x00, 0x8a,
//     0x09, 0xf5, 0x90, 0xec, 0x08, 0xa6, 0xd4, 0x38,
//     0x12, 0x19, 0xe8, 0x6b, 0xa0, 0x2f, 0xec, 0x70,
//     0x61, 0x14, 0xb3, 0xeb, 0x4e, 0x47, 0x5f, 0x6c,
//     0x81, 0x29, 0x0e, 0xb6, 0x01, 0x13, 0xb3, 0x54,
//     0x5e, 0x97, 0xe6, 0x95, 0x5e, 0x10, 0xce, 0x55,
//     0x74, 0x9f, 0xa9, 0x10, 0xd6, 0x5a, 0xe1, 0xb6,
//     0x27, 0xa8, 0x64, 0xda, 0x7b, 0x29, 0x5d, 0x5a,
//     0x52, 0x7c, 0x11, 0xda, 0xbd, 0x64, 0x47, 0x7a,
//     0xdd, 0x8e, 0x2e, 0x2b, 0x39, 0xd9, 0x69, 0x45,
//     0x99, 0x34, 0x06, 0xe3, 0x68, 0x8e, 0xce, 0xe6,
//     0x9f, 0xe5, 0x1c, 0x5b, 0xdd, 0x05, 0x7c, 0x02,
//     0x28, 0xa7, 0x5b, 0x24, 0x63, 0x62, 0x0a, 0x69,
//     0xab, 0x82, 0xa1, 0x20, 0x7a, 0x9d, 0xd7, 0x0c,
//     0x7f, 0x24, 0xc7, 0xf1, 0x64, 0xc7, 0xda, 0xd8,
//     0x0e, 0x93, 0x65, 0x4d, 0x07, 0x23, 0x61, 0xa6,
//     0x73, 0xc3, 0xc5, 0x39, 0x24, 0x64, 0xbd, 0x67,
//     0xd9, 0x7c, 0xf5, 0x49, 0xbe, 0x08, 0x68, 0xe5,
//     0xea, 0x78, 0xc6, 0xac, 0xa5, 0x4b, 0xed, 0xa0,
//     0x42, 0xdc, 0x87, 0x42, 0x44, 0x83, 0x61, 0x43,
//     0xc7, 0x16, 0x20, 0x77, 0x4e, 0xa9, 0x83, 0x6b
// };

// const uint8_t SecurityHandler::KINETIC_MODULUS_LAMARR3[] = {
//     0xd1, 0xc2, 0x28, 0xf5, 0xad, 0x2e, 0xce, 0x4f,
//     0x77, 0x87, 0x98, 0x27, 0xf7, 0xc0, 0xf5, 0xc6,
//     0x8b, 0x60, 0x70, 0x49, 0x9b, 0xe2, 0x39, 0x31,
//     0x06, 0xb6, 0x2f, 0x7e, 0xf2, 0x09, 0x80, 0x6d,
//     0x98, 0x28, 0xff, 0x7b, 0x8f, 0x99, 0xa7, 0x1d,
//     0xaa, 0xb7, 0xdc, 0xb8, 0x20, 0x03, 0xf2, 0xdc,
//     0x8b, 0x40, 0x0d, 0x94, 0xc6, 0x52, 0xb0, 0x1c,
//     0xe8, 0x9d, 0x91, 0x4f, 0x7b, 0x75, 0x60, 0x23,
//     0x3a, 0xf2, 0xb0, 0x33, 0x4d, 0xbe, 0xd6, 0x41,
//     0x17, 0xc3, 0x7d, 0x11, 0x77, 0xdd, 0xa1, 0xe5,
//     0x24, 0x5d, 0xdf, 0xb7, 0xeb, 0x7a, 0xee, 0x85,
//     0x06, 0x09, 0x5f, 0xa7, 0xe7, 0x8c, 0xbe, 0x0b,
//     0x81, 0xe0, 0x43, 0xd0, 0x74, 0x59, 0x86, 0x16,
//     0xe3, 0x6b, 0x15, 0xdc, 0x2a, 0x19, 0x73, 0xa1,
//     0x79, 0x07, 0x6a, 0x50, 0x6e, 0x98, 0x83, 0x70,
//     0x43, 0x7e, 0x02, 0x8e, 0x9a, 0x81, 0xf6, 0xb1,
//     0xf1, 0x2d, 0x19, 0x4b, 0x41, 0x36, 0xbc, 0x45,
//     0x01, 0xf8, 0x42, 0x02, 0x6e, 0xc7, 0x0c, 0xc3,
//     0x81, 0xc6, 0xd4, 0x3d, 0x39, 0xf4, 0x42, 0x00,
//     0x5e, 0x7c, 0xd3, 0x64, 0x72, 0xb9, 0xa9, 0xde,
//     0xf2, 0xc2, 0x8a, 0xe8, 0x1a, 0xba, 0x52, 0x63,
//     0x0b, 0x5a, 0x24, 0x41, 0xbb, 0x9b, 0xe4, 0x16,
//     0x00, 0xec, 0x98, 0xf1, 0x83, 0xbe, 0x5a, 0x60,
//     0x73, 0x6e, 0x38, 0xc8, 0xce, 0xac, 0x37, 0x2c,
//     0x8b, 0x30, 0x54, 0x72, 0x60, 0xe0, 0x6e, 0x56,
//     0x29, 0x71, 0x91, 0x2e, 0xcc, 0xa9, 0xfa, 0x4e,
//     0x65, 0x5e, 0xa6, 0xea, 0x52, 0x41, 0x23, 0xfc,
//     0x19, 0x15, 0x94, 0x45, 0x18, 0x51, 0x0c, 0x26,
//     0xec, 0x56, 0x2a, 0xdc, 0xa7, 0xb5, 0x8a, 0xbc,
//     0xbb, 0xb7, 0xce, 0x2b, 0xb4, 0x5c, 0x4d, 0xaf,
//     0x97, 0xf9, 0x0b, 0x6d, 0x75, 0x33, 0x14, 0xd2,
//     0x4a, 0x42, 0x3a, 0xdf, 0x6b, 0xa1, 0x60, 0x1d
// };

#endif
// Reserved for China Builds End

// Public exponent for rsa decryption
const uint32_t SecurityHandler::KINETIC_PUBLIC_EXPONENT = 65537;

// Used for padding in encoding message
const uint8_t SecurityHandler::SHA2_PADDING_ID[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
    0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};

// Modulus for testing purposes
const uint8_t SecurityHandler::KATKeyModulus[] = {
    0xC2, 0xD9, 0x0B, 0xAB, 0x0C, 0x99, 0x4F, 0x3E,
    0xD7, 0x43, 0x67, 0x49, 0x9A, 0xE2, 0xA5, 0x93,
    0x13, 0x11, 0x68, 0xC7, 0x5B, 0x22, 0x7C, 0x19,
    0x01, 0xDF, 0xA2, 0xF8, 0xC5, 0xDE, 0x09, 0x8F,
    0xE9, 0xFF, 0x51, 0x20, 0xF7, 0x0D, 0x15, 0x53,
    0x31, 0xB3, 0x0A, 0x4A, 0xF6, 0x77, 0xCA, 0xBA,
    0x84, 0x30, 0x1C, 0xA1, 0x75, 0x49, 0x1A, 0xFF,
    0x98, 0x80, 0xF9, 0x9B, 0x3E, 0x43, 0x7C, 0x91,
    0x30, 0xF5, 0xBD, 0xE5, 0x79, 0xB3, 0x5B, 0x58,
    0xEB, 0x3A, 0x21, 0xA7, 0x2E, 0x3C, 0x30, 0x7C,
    0xDE, 0xB9, 0x1D, 0x62, 0xEE, 0xAF, 0x95, 0xFE,
    0x58, 0xF5, 0x60, 0xCF, 0x84, 0x77, 0xF0, 0xE8,
    0x88, 0x03, 0x43, 0x98, 0x7B, 0x7C, 0x3A, 0xF7,
    0x0B, 0xEC, 0xCA, 0x65, 0x2F, 0x00, 0x42, 0x92,
    0xC9, 0x5E, 0x5B, 0xF5, 0xA1, 0x64, 0x53, 0xB6,
    0x67, 0x06, 0x1F, 0xC0, 0x46, 0xEF, 0x7A, 0xA0,
    0xA2, 0xEC, 0xC0, 0xBB, 0x95, 0xF3, 0xC1, 0x6E,
    0xC4, 0x5C, 0x9B, 0x10, 0xC5, 0x23, 0xF8, 0x80,
    0xAF, 0x04, 0xA9, 0x2F, 0xCA, 0x7E, 0x51, 0xE2,
    0x93, 0xA2, 0xAD, 0x59, 0x8A, 0xB2, 0x92, 0x90,
    0x76, 0x15, 0x43, 0xEE, 0x05, 0x49, 0xDC, 0x77,
    0xF2, 0x64, 0xA8, 0x93, 0x40, 0x52, 0xC6, 0x43,
    0x31, 0x7F, 0x00, 0xA6, 0xED, 0x8C, 0x6B, 0xE6,
    0x3B, 0x55, 0x75, 0x44, 0xE0, 0xF9, 0xA9, 0x02,
    0xC4, 0x07, 0x8C, 0x8D, 0x5C, 0x4B, 0x37, 0xAF,
    0x3B, 0x9D, 0x19, 0xAD, 0x29, 0x4C, 0x12, 0xA3,
    0xA8, 0xF1, 0xFB, 0xBD, 0xDB, 0x90, 0x42, 0x15,
    0x5E, 0x4E, 0x98, 0xB3, 0x1D, 0xEC, 0x2A, 0xCC,
    0x0C, 0x54, 0x84, 0x51, 0x75, 0x26, 0xC6, 0x22,
    0x98, 0x04, 0x7B, 0xB7, 0x62, 0x6F, 0x29, 0xC4,
    0xD9, 0xAB, 0xB1, 0x04, 0x32, 0xD1, 0xC6, 0xF5,
    0x0D, 0xC1, 0x46, 0xE5, 0xFB, 0xD2, 0xC9, 0x31
};

// PKCS Signature of Message[] to decyrpt for testing
const uint8_t SecurityHandler::KATPkcsSignature[] = {
    0x3D, 0xD1, 0x01, 0x28, 0x19, 0xEE, 0x50, 0xD2,
    0xFE, 0x78, 0x83, 0x5A, 0x2C, 0x4C, 0x85, 0x19,
    0xE2, 0x8D, 0x74, 0x8F, 0xC8, 0x53, 0xE4, 0x25,
    0x05, 0x4E, 0xA7, 0x28, 0x32, 0x7F, 0x72, 0x4E,
    0x97, 0x07, 0x83, 0xCB, 0xBF, 0x86, 0xB9, 0x61,
    0x9A, 0xC8, 0xD5, 0x34, 0x03, 0xD5, 0xFD, 0xBA,
    0x28, 0x4A, 0xDC, 0x7D, 0x6A, 0x90, 0x12, 0xCA,
    0xF6, 0xD6, 0x12, 0xE1, 0xE9, 0xA4, 0x9E, 0xC2,
    0x8A, 0x44, 0xB3, 0x7D, 0xD2, 0xC4, 0x9F, 0x8D,
    0x25, 0x41, 0x56, 0x9E, 0xD9, 0xDA, 0x94, 0x2A,
    0xEB, 0x2A, 0xBB, 0xF5, 0xA1, 0xE9, 0x6C, 0x4F,
    0xCB, 0xA6, 0xDE, 0x4F, 0x67, 0x0A, 0x23, 0x5E,
    0xA9, 0xC5, 0x42, 0x7B, 0x8D, 0x82, 0x07, 0xA9,
    0x3F, 0xFC, 0xF1, 0x16, 0xA0, 0xB2, 0xC9, 0x40,
    0x86, 0x3A, 0xE2, 0x80, 0xC1, 0xAD, 0xD7, 0x44,
    0x3E, 0x27, 0x28, 0x67, 0xE1, 0xFB, 0xEC, 0x9C,
    0x19, 0x67, 0xB1, 0xD0, 0xE3, 0x55, 0xF3, 0x0A,
    0x4B, 0x25, 0xEE, 0x9A, 0x9F, 0x65, 0xC9, 0xA9,
    0x5D, 0x7E, 0x52, 0x38, 0x41, 0xB9, 0x58, 0xCB,
    0x24, 0xEB, 0xBD, 0xE7, 0x05, 0x60, 0x61, 0x7E,
    0x8B, 0x14, 0x98, 0x4C, 0x1E, 0xF6, 0x8B, 0x33,
    0xF2, 0x86, 0x6F, 0x55, 0xBD, 0x32, 0x40, 0x74,
    0x81, 0x5D, 0x6E, 0x98, 0x0A, 0xD6, 0xB0, 0x80,
    0x26, 0x07, 0xBD, 0x35, 0x4E, 0x67, 0xA1, 0xEC,
    0x0C, 0x46, 0x6C, 0xF7, 0xC2, 0x3F, 0x94, 0x37,
    0x77, 0x02, 0xAB, 0x9A, 0xFB, 0xB3, 0xF9, 0xB4,
    0x67, 0xA8, 0x1B, 0x7A, 0x59, 0x57, 0x89, 0x9D,
    0x63, 0xB6, 0xA6, 0xF1, 0x09, 0x96, 0x56, 0x87,
    0x04, 0x11, 0x09, 0x90, 0xB7, 0x78, 0x69, 0xFF,
    0x7E, 0xD8, 0xE2, 0xD0, 0x03, 0x64, 0x7B, 0xD4,
    0x58, 0x20, 0xED, 0x3A, 0x6D, 0xA8, 0x66, 0xDF,
    0x09, 0x65, 0xD8, 0x86, 0x8B, 0xE5, 0x5C, 0xB7
};

SecurityHandler::SecurityHandler() {}

bool SecurityHandler::VerifySecuritySignature(struct LODHeader *thumbprint_lodHeader,
  struct LODHeader *security_signature, struct LODHeader *security_info) {
  #ifdef FIRMWARE_SIGNING_ENABLED
  bool status = true;
  const uint32_t encoded_message_length = (LODHEADER_SIZE * 3) + thumbprint_lodHeader->input_size;
  // input to be computed with hash and padded
  unsigned char signature[security_signature->input_size];
  unsigned char encoded_message[encoded_message_length];

  memcpy(signature, security_signature->input.c_str(), security_signature->input_size);
  // security info header
  memcpy(encoded_message, security_info->header.c_str(), LODHEADER_SIZE);
  // thumbprint header
  memcpy(encoded_message + LODHEADER_SIZE, thumbprint_lodHeader->header.c_str(), LODHEADER_SIZE);
  // thumbprint data
  memcpy(encoded_message + LODHEADER_SIZE * 2, thumbprint_lodHeader->input.c_str(),
    thumbprint_lodHeader->input_size);
  // security signature header
  memcpy(encoded_message + LODHEADER_SIZE * 2 + thumbprint_lodHeader->input_size,
    security_signature->header.c_str(), LODHEADER_SIZE);

  std::vector<const uint8_t *> moduli;
  moduli.push_back(KINETIC_MODULUS_KEY0);
  moduli.push_back(KINETIC_MODULUS_KEY1);
  #if defined(PRODUCT_LAMARRKV) || defined(PRODUCT_X86) || defined(PRODUCT_LAMARRKV_QUAL) || defined(PRODUCT_LAMARRKV_ARMADALP)
  moduli.push_back(KINETIC_MODULUS);
  #endif

  // Try pkcs verification on each modulus or until we are successful
  for (std::vector<const uint8_t *>::iterator modulus = moduli.begin();
      modulus != moduli.end(); ++modulus) {
    status = SecuritySignaturePKCS(signature, encoded_message, encoded_message_length, (*modulus));
    // Success break out
    if (status) {
      break;
    }
  }

  return status;
  #else
  return false;
  #endif
}

bool SecurityHandler::SecuritySignaturePKCS(unsigned char* signature,
  unsigned char* encoded_message, const uint32_t encoded_message_length,
  const uint8_t * modulus) {
  bool status = true;
  CBNCTX cbn_context[1];
  byte computed_hash[RSA_2048_BYTES];
  byte decrypted_signature[RSA_2048_BYTES];

  // Compute hash and perform required padding
  ROM_PadPkcs21((byte *)encoded_message, encoded_message_length,
    (byte *)computed_hash, RSA_2048_BYTES);

  CBNCTX *ctx;
  RSAPUB public_key;
  CBN bignum;
  uint16_t oplen;

  // Initialize the CBN context
  CbnInitializeContext(cbn_context, 0, &ctx);

  // setup public key
  public_key.flags = 0;
  public_key.flags |= RSAFLAGS_modulus;
  public_key.flags |= RSAFLAGS_publicExponent;

  CbnFromBEBytes(&public_key.modulus, (byte*)modulus, RSA_2048_BYTES, ctx);
  public_key.publicExponent = KINETIC_PUBLIC_EXPONENT;

  // Perform RSA public key operation
  memset(&bignum, 0 , sizeof(bignum));
  oplen = (uint16_t) RSA_2048_BYTES;

  if (!status || CbnFromBEBytes(&bignum, (byte*) signature,
    RSA_2048_BYTES, ctx) != CBN_ERROR_NONE) {
      status = false;
      VLOG(1) << "Failed CbnFromBEBytes";//NO_SPELL
      LOG(ERROR) << "Security verification failed";
  } else if (RsaPublicRaw(&bignum, &public_key, ctx)) {
      status = false;
      VLOG(1) << "Failed RsaPublicRaw";//NO_SPELL
      LOG(ERROR) << "Security verification failed";
  } else if (CbnToBEBytes((byte *) decrypted_signature,
    &oplen, &bignum, ctx) != CBN_ERROR_NONE) {
      status = false;
      VLOG(1) << "Failed CbnToBEBytes";//NO_SPELL
      LOG(ERROR) << "Security verification failed";
  } else if (memcmp(decrypted_signature, computed_hash, RSA_2048_BYTES)) {
      status = false;
      VLOG(1) << "Signature does not match";
      LOG(ERROR) << "Security verification failed";
  }

  return status;
}

// Test PKCS implementation with a hardcoded message
bool SecurityHandler::KAT_PKCSVerify() {
    const byte message[] = "Hello world";
    const uint32_t message_length = sizeof(message) - 1;
    const uint32_t modulus_size_in_bytes = RSA_2048_BYTES;

    CBNCTX CBNContext[1];
    byte computed_padded_hash[modulus_size_in_bytes];
    byte decrypted_signature[modulus_size_in_bytes];
    bool status = true;

    // Compute hash and perform required padding.
    ROM_PadPkcs21((byte *)message, message_length, (byte *)computed_padded_hash,
      modulus_size_in_bytes);

    // Perform RSA Public Key operation on the PKCS signature and store result in
    // decrypted_signature[].
    CBNCTX *ctx;
    RSAPUB public_key;
    CBN bignum;
    uint16_t oplen;

    // Initialize the CBN context
    CbnInitializeContext(CBNContext, 0, &ctx);

    // Initialize the public key
    public_key.flags  = 0;
    public_key.flags |= RSAFLAGS_modulus;
    public_key.flags |= RSAFLAGS_publicExponent;

    CbnFromBEBytes(&public_key.modulus, (byte *)KATKeyModulus,
      modulus_size_in_bytes, ctx);
    public_key.publicExponent = KINETIC_PUBLIC_EXPONENT;

    // Perform RSA public key operation
    memset(&bignum, 0, sizeof(bignum));
    oplen = (uint16_t) modulus_size_in_bytes;

    if (!status || CbnFromBEBytes(&bignum, (byte *)KATPkcsSignature,
      modulus_size_in_bytes, ctx) != CBN_ERROR_NONE) {
      VLOG(1) << "Failed CbnFromBEBytes";//NO_SPELL
      LOG(ERROR) << "Security verification failed";
      status = false;
    } else if (RsaPublicRaw(&bignum, &public_key, ctx)) {
      VLOG(1) << "Failed RsaPublicRaw";//NO_SPELL
      LOG(ERROR) << "Security verification failed";
      status = false;
    } else if (CbnToBEBytes( (byte *)decrypted_signature, &oplen,
      &bignum, ctx) != CBN_ERROR_NONE) {
      VLOG(1) << "Failed CbnToBEBytes";//NO_SPELL
      LOG(ERROR) << "Security verification failed";
      status = false;
    } else if (memcmp( decrypted_signature, computed_padded_hash,
      modulus_size_in_bytes)) {
      VLOG(1) << "Signature does not match";
      LOG(ERROR) << "Security verification failed";
      status = false;
    }

    // Deallocate temporary variables and structures
    CbnReleaseDigits(&bignum, ctx);
    RsaReleasePublicKey(CBNContext, 0, &public_key);

    return status;
}

//Performs PKCS padding on the given message.
void SecurityHandler::ROM_PadPkcs21(byte *message, uint32_t message_length,
  byte *padded_hash, uint32_t emLen) {
    SHA256_CTX SHA256Context;
    uint32_t psLen;

    SHA256_Init(&SHA256Context);
    SHA256_Update(&SHA256Context, (void *) message, message_length);
    memset(padded_hash, 0, emLen);
    padded_hash[1] = 1;
    // Number of 0xFF bytes in pad
    psLen = emLen - (sizeof(SHA2_PADDING_ID) + SHA256_DIGEST_LENGTH) - 3;
    memset(padded_hash + 2, 0xFF, psLen);
    memcpy(padded_hash + psLen + 3, SHA2_PADDING_ID, sizeof(SHA2_PADDING_ID));
    SHA256_Final((uint8_t *) (padded_hash + psLen + 3 + sizeof(SHA2_PADDING_ID)), &SHA256Context);
}


// Perform RSA public key operation on preformated buffer.
// The input and output is in num.
// Note the the digit array in num may be changed.
int SecurityHandler::RsaPublicRaw(CBN *num, RSAPUB *key, CBNCTX *ctx) {
    CBN result;
    int status = CBN_ERROR;

    result.digits = NULL;

    // Compute Montgomery parameter if needed.
    if ((key->flags & RSAFLAGS_montN0) == 0) {
      if (CbnNi0(&key->montN0, &key->modulus, ctx) != CBN_ERROR_NONE) {
         goto Done;
      }
      key->flags |= RSAFLAGS_montN0;
    }

    result.digits = NULL;
    if (key->publicExponent == EXPF0) {
      status = CbnModMult(&result, num, num, &key->modulus, ctx);
      if (status) {
         goto Done;
      }
      status = CbnModMult(num, &result, num, &key->modulus, ctx);
      if (status) {
         goto Done;
      }
    } else {
      CBN exp;
      exp.digits = NULL;
      status = CbnFourthFromDigit2(&exp, (uint32_t)key->publicExponent, ctx);
      if (status != 0) {
         goto Done;
      }
      status = CbnModExp(num, num, &exp, &key->modulus, key->montN0, ctx);
      CbnReleaseDigits(&exp, ctx);
      if (status) {
         goto Done;
      }
    }

  Done:
    CbnReleaseDigits(&result, ctx);
    return status;
}

// Release all the CBN from a public key.
void SecurityHandler::RsaReleasePublicKey(CBNCTX *CBNContext, uint8_t session,
  RSAPUB * key) {
  CBNCTX * ctx;
  M_StackOverflow(session, return);

  CbnGetContext(CBNContext, session, &ctx);
  key->flags = 0;
  CbnReleaseDigits(&(key->modulus), ctx);
  key->publicExponent = 0;
  key->montN0 = 0;
}

// Initialize the CBN context for a given session
int SecurityHandler::CbnInitializeContext(CBNCTX *CBNContext, uint8_t session,
  CBNCTX **pCtx) {
  memset(&CBNContext[session], 0, sizeof(CBNCTX));

  // Setup free digit lists for calculations involved in decrypting message
  // Double sized digits
  CBNContext[session].headDoubleSized = NULL;
  for (int i = 0; i < NUMDOUBLESIZED; i++) {
      M_MovePtrToDigits(&(CBNContext[session].doubleSized[i][0]),
                      CBNContext[session].headDoubleSized);
      CBNContext[session].headDoubleSized = &(CBNContext[session].doubleSized[i][0]);
  }

  // Full sized digits
  CBNContext[session].headFullSized = NULL;
  for (int i = 0; i < NUMFULLSIZED; i++) {
    M_MovePtrToDigits(&(CBNContext[session].fullSized[i][0]),
                      CBNContext[session].headFullSized);
    CBNContext[session].headFullSized = &(CBNContext[session].fullSized[i][0]);
  }

  // Half sized digits
  CBNContext[session].headHalfSized = NULL;
  for (int i = 0; i < NUMHALFSIZED; i++) {
    M_MovePtrToDigits(&(CBNContext[session].halfSized[i][0]),
                      CBNContext[session].headHalfSized);
    CBNContext[session].headHalfSized = &(CBNContext[session].halfSized[i][0]);
  }

  // Fourth sized digits
  CBNContext[session].headFourthSized = NULL;
  for (int i = 0; i < NUMFOURTHSIZED; i++) {
    M_MovePtrToDigits(&(CBNContext[session].fourthSized[i][0]),
                      CBNContext[session].headFourthSized);
    CBNContext[session].headFourthSized = &(CBNContext[session].fourthSized[i][0]);
  }

  CBNContext[session].initialized = true;
  CBNContext[session].session = session;

  *pCtx = &CBNContext[session];
  return CBN_ERROR_NONE;
}

// Get and optionally initialize the CBN context.
int SecurityHandler::CbnGetContext(CBNCTX *CBNContext, uint8_t session,
  CBNCTX **pCtx) {
  M_StackOverflow(session, return 0);

  if (!CBNContext[session].initialized) {
    CbnInitializeContext(CBNContext, session, pCtx);
  }

  *pCtx = &CBNContext[session];
  return CBN_ERROR_NONE;
}

// Create a double-, full- or half-sized CBN from an array of
// unsigned bytes with the most significant byte first (Big Endian).
// The choice of CBN size is based on the length of the array.
// The length of the array must be a multiple of the digit size.
// For example, if a DIGIT is 2 bytes, then len must be a multiple of 2.
int SecurityHandler::CbnFromBEBytes(CBN *out, byte *pIn, uint16_t nInLen,
  CBNCTX *ctx) {
    int status = CBN_ERROR;
    uint16_t width;
    unsigned char * pInByte;
    unsigned char * pDigit;
    M_StackOverflow(ctx->Session, return CBN_ERROR);

    if (nInLen > (sizeof(DIGIT) * CBNDOUBLESIZE)) {
      status = CBN_ERROR_BUFFER_OVERFLOW;
      goto Done;
    }

    width = nInLen / sizeof(DIGIT);
    if (nInLen != (sizeof(DIGIT) * width)) {
        goto Done;
    }

    out->digits = NULL;
    status = CbnGrabDigitsAndZero(out, width, ctx);

    if (status != 0) {
        goto Done;
    }

    // pDigit starts at top, pInByte starts at bottom.
    pDigit = (unsigned char *)(out->digits);
    pInByte = pIn+nInLen;
    for (int i = 0; i < nInLen; i++) {
        *pDigit++= *--pInByte;
    }

  Done:
    return status;
}

// Allocates and returns a CBN zero of the required size
int SecurityHandler::CbnGrabDigitsAndZero(CBN *bNum, uint16_t width, CBNCTX *ctx) {
  int status;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  status = CbnGrabDigits(bNum, width, ctx);
  if ( status != 0 ) {
      return status;
  }
  // We may have grabbed width larger than width argument.
  for (int i = 0; i < bNum->width + 2; i++) {
      bNum->digits[i] = 0;
  }

  bNum->negative = false;
  return CBN_ERROR_NONE;
}

// Grab a digit array for a CBN.
// It fills in num->digits with location of array.
// Based on width it will grab the right sized buffer.
// If num has the right size array already, do nothing, else
// release the digit array.
// Returns status.  It calls error routine if none available.
// It sets num->negative to false, and
// num->width to width.
int SecurityHandler::CbnGrabDigits(CBN *num, uint16_t width, CBNCTX *ctx) {
  M_StackOverflow( ctx->Session, return CBN_ERROR );

  if (width <= CBNFOURTHSIZE) {
      width = CBNFOURTHSIZE;
  } else if (width <= CBNHALFSIZE) {
      width = CBNHALFSIZE;
  } else if (width <= CBNFULLSIZE) {
      width = CBNFULLSIZE;
  } else {
      width = CBNDOUBLESIZE;
  }

  num->negative = false;
  // Check if we don't need to do allocation
  if ((num->digits != NULL) && (num->width == width)) {
      // Zero overflow words.
      *(DIGIT2 *)&(num->digits[num->width]) = DIGIT2_ZERO;
      return CBN_ERROR_NONE;
  } else {
      CbnReleaseDigits(num, ctx);
  }

  num->width = width;
  if (width == CBNDOUBLESIZE) {
    if (ctx->headDoubleSized == NULL) {
       return CBN_ERROR;
    }

    num->digits = ctx->headDoubleSized;
    M_MoveDigitsToPtr(ctx->headDoubleSized, num->digits);
  } else if (width == CBNFULLSIZE) {
    if (ctx->headFullSized == NULL) {
          return CBN_ERROR;
    }

    num->digits = ctx->headFullSized;
    M_MoveDigitsToPtr(ctx->headFullSized, num->digits);
  } else if (width == CBNHALFSIZE) {
    // Get half sized
    if ( ctx->headHalfSized == NULL ) {
       return CBN_ERROR;
    }

    num->digits = ctx->headHalfSized;
    M_MoveDigitsToPtr(ctx->headHalfSized, num->digits);
  } else {
    // Get one fourth sized
    if (ctx->headFourthSized == NULL) {
       return CBN_ERROR;
    }

    num->digits = ctx->headFourthSized;
    M_MoveDigitsToPtr(ctx->headFourthSized, num->digits);
  }
  *(DIGIT2 *)&(num->digits[num->width]) = DIGIT2_ZERO;
  return CBN_ERROR_NONE;
}

// Release digit array for a CBN.
// It sets num->width to zero, and num->digits to NULL, and
// num->negative to FALSE.
// If num->digits is null, don't release.
int SecurityHandler::CbnReleaseDigits(CBN * num, CBNCTX *ctx) {
  M_StackOverflow( ctx->Session, return CBN_ERROR );
  if (num->digits == NULL) {
      num->negative = false;
      num->width = 0;
  }
  return CBN_ERROR_NONE;
}

// Computes least significant digit of negative inverse of n.
// Most operations are performed with Montgomery multiplication,
// so the caller must supply the LS digit of n inverse (ni0).
// This routine computes that value.
// ni0 = minus (inverse of n[0]) mod 2**DIGITSBITS
// The value of n must be odd.
int SecurityHandler::CbnNi0(DIGIT *ni0, CBN *n, CBNCTX *ctx) {
  DIGIT temp;
  DIGIT y;
  DIGIT x;
  DIGIT ti;  // 2**i
  DIGIT mmi; // mask for mod 2**(i+1)
  unsigned i;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  x = n->digits[0];
  y = 1;
  ti = 2;
  mmi = 3;

  for (i = 1; i < DIGITBITS; ++i) {
    temp = (DIGIT)(x*y); // Truncation OK
    temp &= mmi;
    if (ti < temp) {
       y = (DIGIT)(y + ti);
    }
    ti = (DIGIT)(ti << 1);
    mmi = (DIGIT)(( mmi << 1 ) | 0x1);
  }

  y = (DIGIT)(y ^ DIGITMASK);
  y = (DIGIT)(y + 1);
  *ni0 = y;

  return CBN_ERROR_NONE;
}

// Multiply two CBN producing a CBN.
// All inputs must be the same size.
// out = a * b mod n
// The CBN for 'out' can be the same as 'a', 'b', or 'n'.
int SecurityHandler::CbnModMult(CBN *out, CBN *a, CBN *b, CBN *n, CBNCTX *ctx) {
  int status;
  status = CbnUMult(out, a, b, ctx);

  if (status == CBN_ERROR_NONE) {
     status = CbnMod(out, out, n, ctx);
  }
  return status;
}

// Multiply two unsigned half-sized CBN producing a full-sized CBN.
// out = a * b
// The CBN for 'out' can be the same as 'a' or 'b'.  It will be resized.
int SecurityHandler::CbnUMult(CBN *out, CBN *a, CBN *b, CBNCTX *ctx) {
  DIGIT2 temp2;
  DIGIT carry;
  DIGIT bv;
  DIGIT * aDigits;
  DIGIT * bDigits;
  DIGIT * outDigits;
  int ai, bi;
  int aw, bw;
  int status = CBN_ERROR;
  CBN t;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  t.digits = NULL;
  aw = a->width;
  bw = b->width;
  status = CbnGrabDigitsAndZero(&t, (uint16_t)(aw + bw), ctx);
  if (status != 0) {
    goto Done;
  }
  outDigits = t.digits;
  aDigits = a->digits;
  bDigits = b->digits;
  for (bi = 0; bi < bw; ++bi) {
    carry = 0;
    bv = bDigits[bi];
    // TBD OK to skip zeros, since this routine is not
    // part of the core that must run in fixed time.
    if (bv == 0) {
       continue;
    }
    for (ai = 0; ai < aw; ++ai) {
       cbn_sum_mult(carry, outDigits[ai + bi], outDigits[ai + bi],
                     aDigits[ai], bv);
    }
    outDigits[ai + bi] = carry;
  }
  // Release out last in case out is same as a or b.
  CbnReleaseDigits(out, ctx);
  M_Memcpy(out, &t, sizeof(CBN));

  Done:
  if (t.digits != out->digits) {
    CbnReleaseDigits(&t, ctx);
  }
  return status;
}

// calculates a mod n
int SecurityHandler::CbnMod(CBN *aModP, CBN *a, CBN *p, CBNCTX *ctx) {
  CBN q;
  int status;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  q.digits = NULL;
  status = CbnDivide(a, p, &q, aModP, ctx);

  CbnReleaseDigits(&q, ctx);
  return status;
}

// Perform division.  Given dividend and divisor find
// quotient and remainder such that
// remainder < divisor, and
// dividend = divisor * quotient + remainder.
//
// Notes: Assumes dividend and divisor are both nonnegative numbers.
//
// The CBN for 'quotient' and 'remainder' can be the same as 'dividend1'
// or 'divisor1'.
// The 'quotient' will be the same width as 'dividend1'.
// The 'remainder' will be the same width as 'divisor1'.
//
// CbnDivide implements the multiprecision algorithm from
// "The Art of Computer Programming, VOLUME 2, Seminumerical Algorithms,
//  2nd Ed", by D. Knuth. Algorithm D, Section 4.3.1 p272-273.
int SecurityHandler::CbnDivide(CBN *dividend1, CBN *divisor1, CBN *quotient,
  CBN *remainder, CBNCTX *ctx) {
  DIGIT2 temp2;
  DIGIT  overflow;
  DIGIT shiftSize = 0, divisorLen, dividendLen, counter;
  DIGIT numDigitsLeft;
  int status = CBN_ERROR;
  DIGIT msword;
  DIGIT q;
  DIGIT *dsor;
  DIGIT *ddend;
  register CBN *dividend;
  CBN localDividend;
  register CBN *divisor;
  CBN localDivisor;
  DIGIT remainderLen, quotientLen;
  M_StackOverflow( ctx->Session, return CBN_ERROR );

  localDividend.digits = localDivisor.digits = NULL;
  /*
  * If dividend < divisor
  *       return:
  *           quotient = dividend
  *           remainder = 0
  */
  // Need local copy for shiftin dividend and divisor.
  status = CbnCopy(&localDividend, dividend1, ctx);
  if (status != 0) {
    goto Done;
  }
  localDividend.digits[localDividend.width] = 0;
  localDividend.digits[localDividend.width + 1] = 0;
  status = CbnCopy(&localDivisor, divisor1, ctx);
  if (status != 0) {
    goto Done;
  }
  localDivisor.digits[localDivisor.width] = 0;
  localDivisor.digits[localDivisor.width + 1] = 0;
  dividend = &localDividend;
  divisor  = &localDivisor;

  divisorLen = CbnNumSignificantDigits(divisor);
  dividendLen = CbnNumSignificantDigits(dividend);
  if ((divisorLen == 1) && (divisor->digits[0] == 0)) {
    // Divide by zero error.
    status = CBN_ERROR;
    goto Done;
  }

  if ((CbnUCompare(divisor, dividend) > 0)) {
    // The remainder and quotient might be same CBN as divisor1 or dividend1.
    // The 'remainder' will be the same width as 'divisor1'.
    status = CbnGrabDigitsAndZero(remainder, divisor->width, ctx);
    if (status != 0) {
       goto Done;
    }

    for (int i = 0; i <= (int)dividendLen; ++i) {
       remainder->digits[i] = dividend->digits[i];
    }
    // The 'quotient' will be the same width as 'dividend1'.
    status = CbnGrabDigitsAndZero(quotient, dividend->width, ctx);
    goto Done;
  }
  /*
  * Normalize the divisor so that the most significant bit of m.s. digit is set.
  *
  * Note is dividend = (quotient * divisor) + remainder, then
  *    2**k * dividend = (quotient * 2**k * divisor) + (2**k * remainder)
  * So at the end we need to shift the remainder to the right.
  */
  msword = divisor->digits[divisorLen - 1];
  while ((msword & DIGITMSBIT) == 0) {
    msword = (DIGIT)(msword << 1);
    shiftSize++;
  }
  /*
  * To allow for divisor of length 1 we shift over by another word.
  * The CBN digits already have enough room for this.
  */
  if (divisorLen == 1) {
     shiftSize += DIGITBITS;
  }
  if (shiftSize > 0) {
    CbnUShiftLeft(shiftSize, divisor, &divisorLen);
    CbnUShiftLeft(shiftSize, dividend, &dividendLen);
  }
  remainder->negative = false;
  remainderLen = divisorLen;
  numDigitsLeft = (DIGIT)(dividendLen - divisorLen);
  /*
  * Initialize remainder with the upper most significant bytes of the dividend.
  * Remainder is smaller than divisor.
  * However we always set the remainder width to that of the divisor,
  * which is same or bigger than the dividend at this place in the code.
  * The case where dividend < divisor is handled earlier.
  */
  // The 'remainder' will be the same width as 'divisor1'.
  status = CbnGrabDigitsAndZero(remainder, divisor->width, ctx);
  if (status != 0) {
    goto Done;
  }
  for (counter = 0; counter < divisorLen; ++counter) {
    remainder->digits[counter] = dividend->digits[counter + numDigitsLeft];
  }
  remainderLen = divisorLen;
  /*
  * Set quotient to zero.
  * Quotient can be as big as dividend if divisor is one.
  */
  // The 'quotient' will be the same width as 'dividend1'.
  status = CbnGrabDigitsAndZero(quotient, dividend->width, ctx);
  if (status != 0) {
    goto Done;
  }
  quotientLen = 1;

  dsor = &divisor->digits[divisorLen - 2];

  for (;;) {
    /*
     * Compensate for the case that the result of the previous partial division resulted
     * in a remainder whose word length is smaller than the divisor's.
     */
    while (remainderLen < divisorLen && numDigitsLeft > 0) {
       CbnUShiftLeft(DIGITBITS, quotient, &quotientLen);
       CbnUShiftLeft(DIGITBITS, remainder, &remainderLen);
       // Insert new least significant digit.
       remainder->digits[0] = dividend->digits[numDigitsLeft - 1];
       numDigitsLeft--;
    }
    /*
     * Ensure that the remainder < divisor. If not increment quotient and
     * decrement the remainder by the divisor.
     * The high bit of divisor is set, so this should only happen once.
     */
    if (CbnUCompare(divisor, remainder) <= 0) {
       status = CbnSub(remainder, remainder, divisor, ctx);
      if (status != 0) {
          // LOG(ERROR) << "Status is not 0";
          goto Done;
      }
       // remainderLen = CbnNumSignificantDigits(remainder);
       ++(quotient->digits[0]);
       continue;
    }
    if (numDigitsLeft == 0) {
        break;
    }
    /*
     * Shift remainder and quotient
     */
    CbnUShiftLeft(DIGITBITS, quotient, &quotientLen);
    CbnUShiftLeft(DIGITBITS, remainder, &remainderLen);
    // Insert new least significant digit.
    remainder->digits[0] = dividend->digits[numDigitsLeft - 1];
    numDigitsLeft--;
    /*
     * Estimate next term of quotient, form partial product, subtract from interim remainder.
     */
    ddend = &remainder->digits[remainderLen - 3];
    CbnEstimateQuotientDigit(ddend[2], ddend[1], ddend[0], dsor[1], dsor[0], &q);
    remainder->digits[remainder->width+1] = 0;
    overflow = 0;
    for (int i = 0; i <= remainder->width+1; ++i) {
       cbn_diff_mult(overflow, remainder->digits[i],
                      remainder->digits[i], q, divisor->digits[i]);
    }
    if (overflow) {
       remainder->negative = true;
    }
    counter = 2;
    while (remainder->negative && counter > 0) {
       --counter;
       --q;
       overflow = 0;
       for (int i = 0; i <= remainder->width+1; ++i) {
          cbn_sum(overflow, remainder->digits[i],
                   remainder->digits[i], divisor->digits[i]);
       }
      if (overflow) {
          remainder->negative = false;
      }
    }
    remainderLen = CbnNumSignificantDigits(remainder);
    quotient->digits[0] = q;
  }
  if (shiftSize > 0) {
    CbnUShiftRight(shiftSize, remainder, &remainderLen);
  }
  status = CBN_ERROR_NONE;

  Done:
  CbnReleaseDigits(&localDividend, ctx);
  CbnReleaseDigits(&localDivisor, ctx);

  return status;
}

// Make a copy of a CBN.
// This releases any digit array already in out.
// out = a
// The arguments must be different CBN (out and a must
// be different structures).
// This copies the first overflow word, and sets the
// second overflow word to be the sign bit.
int SecurityHandler::CbnCopy(CBN *out, CBN *a, CBNCTX *ctx) {
  DIGIT * out_digits;
  DIGIT * a_digits;
  int status;
  M_StackOverflow( ctx->Session, return CBN_ERROR );

  status = CbnGrabDigits(out, a->width, ctx);
  if (status != 0) {
    goto Done;
  }
  out_digits = out->digits;
  a_digits = a->digits;
  // Include overflow word.
  for (int i = a->width; i >= 0; --i) {
    *out_digits++ = *a_digits++;
  }
  // Second overflow word holds sign bit.
  *out_digits++ = (DIGIT)(a->negative);

  Done:
  return status;
}

// returns 1 if a is greater than b
// returns 0 if a is equal to b
// otherwise it returns -1
int SecurityHandler::CbnUCompare(CBN *a, CBN *b) {
  int aLen, bLen;
  DIGIT aD, bD;
  M_StackOverflow( SESSION1, return CBN_ERROR );

  aLen = CbnNumSignificantDigits(a);
  bLen = CbnNumSignificantDigits(b);
  if (aLen != bLen) {
    return (aLen > bLen)? 1 : -1;
  }

  for (int i = aLen - 1; i >= 0; i--) {
    aD = a->digits[i];
    bD = b->digits[i];
    if (aD == bD) {
       continue;
    }
    return (aD > bD)? 1 : - 1;
  }
  return 0;
}

// Computes:  operand * (2 ** shiftSize).
void SecurityHandler::CbnUShiftLeft(DIGIT shiftSize, CBN *operand, DIGIT *opLen) {
  register DIGIT wordShifts, i, j, length;
  register DIGIT carry, temp;
  register DIGIT *value;
  M_StackOverflow( SESSION1, return );

  if (shiftSize == 0 || (*opLen == 1 && operand->digits[0] == 0)) {
    return;
  }
  wordShifts = (DIGIT)(shiftSize / DIGITBITS);
  length = (DIGIT)(*opLen + wordShifts);
  shiftSize %= DIGITBITS;

  value = operand->digits;
  /*
  * Implement word level shift
  */
  if (wordShifts > 0) {
    for (j = (DIGIT)(length - 1); j >= wordShifts; j--) {
       value[j] = value[j - wordShifts];
    }
    for (j = 0; j < wordShifts; j++) {
       value[j] = 0;
    }
  }
  /*
  * Take care of fractional shift
  */
  if (shiftSize > 0) {
    carry = 0;
    for (i = wordShifts; i < length; i++) {
       temp = (DIGIT)(carry | (value[i] << shiftSize));
       carry = (DIGIT)(value[i] >> (DIGITBITS - shiftSize));
       value[i] = temp;
    }
    value[i] = carry;
    *opLen = (DIGIT)((carry > 0)? i + 1 : i);
  } else {
    *opLen = length;
  }
}

// Subtract two signed CBN producing a third.
// The two numbers may be different lengths and signs.
// out = a - b
// The CBN for 'out' can be the same as 'a' or 'b'.
// Return error if out is more negative than can be
// represented.
int SecurityHandler::CbnSub(CBN *out, CBN *a, CBN *b, CBNCTX *ctx) {
  DIGIT2 temp2;
  DIGIT overflow;
  DIGIT * aDigits;
  DIGIT * bDigits;
  DIGIT * tDigits;
  CBN t;
  DIGIT extension;
  int status = CBN_ERROR;
  int length;
  M_StackOverflow( ctx->Session, return CBN_ERROR );

  t.digits = NULL;

  // Note that out can be same as a or b
  // Length is the max length of a and b.
  length = (a->width > b->width) ? a->width : b->width;

  status = CbnGrabDigits(&t, (uint16_t)length, ctx);
  if (status != 0) {
    goto Done;
  }
  tDigits = t.digits;

  /* Setup second overflow word to represent sign. */
  a->digits[a->width+1] = a->negative;
  b->digits[b->width+1] = b->negative;

  aDigits = a->digits;
  bDigits = b->digits;
  // Length is now the difference in size between a and b
  length = a->width - b->width;
  if (length == 0) {
    overflow = 0;
    // Include overflow word.
    for (int i = a->width; i >= 0; --i) {
      cbn_diff(overflow, *tDigits++, *aDigits++, *bDigits++);
    }
  } else if (length > 0) {
    // a longer than b
    overflow = 0;
    // Include overflow word.
    for (int i = b->width; i >= 0; --i) {
      cbn_diff(overflow, *tDigits++, *aDigits++, *bDigits++);
    }
    extension = (DIGIT)(( b->negative ) ? DIGITMASK : 0);
    for (int i = length; i > 0; --i) {
      cbn_diff(overflow, *tDigits++, *aDigits++, extension);
    }
  } else {
    // b longer than a
    length = -length;
    overflow = 0;
    // Include overflow word.
    for (int i = a->width; i >= 0; --i) {
      cbn_diff(overflow, *tDigits++, *aDigits++, *bDigits++);
    }
    extension = (DIGIT)(( a->negative ) ? DIGITMASK : 0);
    // Include overflow word.
    for (int i = length; i > 0; --i) {
      cbn_diff(overflow, *tDigits++, extension, *bDigits++);
    }
  }
  t.digits[t.width+1] = 0;
  // Top bit of first overflow serves as sign bit.
  t.negative = (unsigned char)((t.digits[t.width] & DIGITMSBIT) ? true : false);
  CbnReleaseDigits(out, ctx);
  M_Memcpy(out, &t, sizeof(CBN));
  // Return error if underflow.
  // Negative minus positive should not be positive.
  if (a->negative && (!b->negative) && (!out->negative)) {
    status = CBN_ERROR;
  }

  Done:
  if (t.digits != out->digits) {
    CbnReleaseDigits(&t, ctx);
  }
  return status;
}

// Estimates the quotient
void SecurityHandler::CbnEstimateQuotientDigit(DIGIT u_2, DIGIT u_1, DIGIT u_0,
      DIGIT v_1, DIGIT v_0, DIGIT *q) {
  DIGIT  r0;
  DIGIT2 u, r, p, q0v0, U1, q0;
  M_StackOverflow( SESSION1, return );

  u = ((DIGIT2)u_2 << DIGITBITS) + u_1;
  q0 = u / v_1;
  if (q0> DIGITMASK) {
    *q = DIGITMASK;
    return;
  }

  p = q0 * v_1;

  r = u - p;
  /*
  * Decrease q0 until q0 * (v1 * b + v0) < u2 * b * b + u1 * b + u0 = U
  * By the definition of division
  *       b > v1 > u - q0 * v1 >= 0
  *       u' =  u2 * b * b + u1 * b + u0 - q0 * v1 * b = (u1 - p0) * b + u0
  *       u' =  r0 * b + u2
  *       U - q0 * v = u' - q0 * v0
  *       Thus q0 will be sized correctly when q0 * v0 <= r0 * b + u0 = U1
  */

  r0 = (DIGIT)(r & DIGITMASK);
  U1 = ((DIGIT2)r0 << DIGITBITS) + u_0;
  q0v0 = q0 * v_0;

  /* Decrease estimated quotient by one q * (v1, v0) > (u2, u1, u0) */
  while (q0v0 > U1) {
    q0--;
    r += v_1;
    if (r > DIGITMASK) {
       break;
    }
    r0 = (DIGIT)(r & DIGITMASK);
    U1 = ((DIGIT2)r0 << DIGITBITS) + u_0;
    q0v0 = q0 * v_0;
  }

  *q = (DIGIT)q0;
}

// Computes: operand / (2 ** shiftSize)
void SecurityHandler::CbnUShiftRight(DIGIT shiftSize, CBN *operand, DIGIT *opLen) {
  DIGIT j, length;
  DIGIT wordShifts;
  DIGIT carry, temp;
  DIGIT *value;
  M_StackOverflow(SESSION1, return);

  if (shiftSize == 0) {
     return;
  }
  wordShifts = (DIGIT)(shiftSize / DIGITBITS);
  if (wordShifts >= *opLen) {
    // Set operand to zero if shift too long.
    operand->negative = false;
    *opLen = 1;
    for (int i = 0; i < operand->width; ++i) {
       operand->digits[i] = 0;
    }
    return;
  }
  length = (DIGIT)(*opLen - wordShifts);
  shiftSize %= DIGITBITS;
  value = operand->digits;
  /*
  * Do word level shift if any
  */
  if (wordShifts > 0) {
    for (j = 0; j < length; j++) {
       value[j] = value[j + wordShifts];
    }
  }
  /*
  * Zero out the digits above the new most significant digit
  */
  for (j = length; j < operand->width; j++) {
    value[j] = 0;
  }
  /*
  * Now do the bit level shifting
  */
  if (shiftSize > 0) {
    carry = 0;
    for (int i = (int)length - 1; i >= 0; i--) {
       temp = (DIGIT)(carry | (value[i] >> shiftSize));
       carry = (DIGIT)(value[i] << (DIGITBITS - shiftSize));
       value[i] = temp;
    }
  }
  /* Recompute size. */
  *opLen = (DIGIT)((value[length - 1] > 0)? length : length - 1);
}

// Fill in the given array of bytes with the value of the given CBN.
// The MSB of the CBN is stored first in the array.
// This does NOT include the overflow digits.
int SecurityHandler::CbnToBEBytes(byte *out, uint16_t *pOutLen, CBN *in, CBNCTX *ctx) {
  int status = CBN_ERROR;
  unsigned char * pOutByte;
  unsigned char * pDigit;

  M_StackOverflow(ctx->Session, return CBN_ERROR);

  if (*pOutLen < (uint16_t)(sizeof(DIGIT) * in->width)) {
    status = CBN_ERROR_BUFFER_OVERFLOW;
    goto Done;
  }
  status = CBN_ERROR_NONE;

  // pDigit starts at top, pOutByte starts at bottom.
  pDigit = (unsigned char *)&(in->digits[in->width]);
  pOutByte = out;
  for (int i = *pOutLen = (uint16_t)(in->width*sizeof(DIGIT)); i > 0; --i) {
    *pOutByte++ = *--pDigit;
  }

  Done:
  return status;
}

// Create a fourth-sized CBN from a long.
int SecurityHandler::CbnFourthFromDigit2(CBN * num, DIGIT2 value, CBNCTX *ctx) {
  return CbnSizedFromDigit2(num, value, ctx, CBNFOURTHSIZE);
}

// Create an explicitly-sized CBN from a long.
int SecurityHandler::CbnSizedFromDigit2(CBN * num, DIGIT2 value, CBNCTX * ctx,
  uint16_t width) {
  DIGIT * digits;
  DIGIT fill;
  int status;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  status = CbnGrabDigits(num, width, ctx);
  if (status != 0) {
    return status;
  }
  num->digits[0] = cbn_digit2_low(value);

  value = cbn_digit2_high(value);
  num->digits[1] = (DIGIT)value;
  if (value & DIGITMSBIT) {
    // LOG(INFO) << "In this conditional";
    num->negative = true;
    fill = DIGITMASK;
  } else {
    fill = 0;
  }

  digits = &(num->digits[2]);

  for (int i = (num->width - 2) + 1; i > 0; --i) {
    *digits++ = fill;
  }

  return status;
}

// Exponentiate a half-sized CBN producing a half-sized CBN.
// out = a ** b mod n
// The operation is performed with Montgomery multiplication,
// so the caller must supply the LS digit of n inverse (ni0).
// This must run in a time independent of the value of input a.
// The CBN for 'out' can be the same as 'a', 'b', or 'n'.
int SecurityHandler::CbnModExp(CBN *out, CBN *a, CBN *b, CBN *n, DIGIT ni0, CBNCTX *ctx) {
  int status;
  CCTXMODEXP contCtx;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  status = CbnModExpStart(&contCtx, a, b, n, ni0, ctx);
  while (status == CBN_CONTINUE) {
    status = CbnModExpContinue(&contCtx, out);
  }
  return status;
}

// Start a ModExp calculation.  See the matching continue routine.
// Exponentiate a half-sized CBN producing a half-sized CBN.
// out = a ** b mod n
// The operation is performed with Montgomery multiplication,
// so the caller must supply the LS digit of n inverse (ni0).
// This must run in a time independent of the value of input a.
// This returns CBN_CONTINUE if OK, or CBN_ERROR if problems.
int SecurityHandler::CbnModExpStart(CCTXMODEXP *cctx, CBN *a, CBN *b, CBN *n, DIGIT ni0,
  CBNCTX *ctx) {
  int status = CBN_ERROR;
  M_StackOverflow( ctx->Session, return CBN_ERROR );

  cctx->stage = 0;
  cctx->counter = 0;
  cctx->a = a;
  cctx->b = b;
  cctx->n = n;
  cctx->ni0 = ni0;
  cctx->t.digits = NULL;
  cctx->ctx = ctx;

  status = CBN_CONTINUE;
  return status;
}

// Continue a ModExp calculation.
// It return CBN_ERROR_NONE when complete, or
// CBN_CONTINUE if the routine must be called again.
// If there is a problem it returns CBN_ERROR.
// Exponentiate a half-sized CBN producing a half-sized CBN.
// out = a ** b mod n
// See the matching start routine.
int SecurityHandler::CbnModExpContinue(CCTXMODEXP *cctx, CBN *out) {
  int status = CBN_ERROR;
  int counter = 0;
  M_StackOverflow(cctx->ctx->Session, return CBN_ERROR);

  switch (cctx->stage) {
  case 0:
    cctx->t.digits = NULL;
    // Set cctx->counter to the most significant
    // exponent bit that is non-zero.

    counter += (int)((cctx->b->width *DIGITBITS) -1);
    for (cctx->counter = (DIGIT)((cctx->b->width * DIGITBITS) - 1);
          cctx->counter > 0;
          --cctx->counter) {
      if (CbnBitIsSet(cctx->b, cctx->counter)) {
          break;
      }
       counter--;
    }

    if (!CbnBitIsSet(cctx->b, cctx->counter)) {
       // The exponent b is zero.  We are done.
       status = CbnFromDigit2(out, 1, cctx->ctx);
       goto Done;
    }
    // Compute the Montgomery reduction am of the input a
    cctx->am.digits = NULL;
    status = CbnMonRed(&cctx->am, cctx->a, cctx->n, cctx->ctx);

    if (status != CBN_ERROR_NONE) {
       goto Done;
    }
    // Initialize t to am.
    status = CbnCopy(&cctx->t, &cctx->am, cctx->ctx);
    if (status != 0) {
       goto Done;
    }

    if (cctx->counter == 0) {
       cctx->stage = 2;
       status = CBN_CONTINUE;
    } else {
       // Decrement cctx->counter to the next least significant bit.
       --cctx->counter;
       ++cctx->stage;
       status = CBN_CONTINUE;
    }
    break;

  case 1:
    // The cctx->counter identifies the next exponent bit to process.
    // Use simple binary method for now.
    // TBD Use 4-bit windowing method with precomputed multipliers.
    // Square t

    status = CbnMonPro(&cctx->t, &cctx->t, &cctx->t,
                         cctx->n, cctx->ni0, cctx->ctx);
    if (status != 0) {
       goto Done;
    }

    if (CbnBitIsSet(cctx->b, cctx->counter)) {
      // Exponent bit is set, multiply in a
      status = CbnMonPro(&cctx->t, &cctx->t, &cctx->am,
                           cctx->n, cctx->ni0, cctx->ctx);
      if (status != 0) {
          goto Done;
      }
    }

    if (cctx->counter > 0) {
       --cctx->counter;
    } else {
       ++cctx->stage;
    }
    status = CBN_CONTINUE;
    break;
  case 2:
    // One final Montgomery product undoes the Montgomery reduction.
    CbnReleaseDigits(&cctx->am, cctx->ctx);
    status = CbnSizedFromDigit2(out, 1, cctx->ctx, cctx->t.width);
    if (status == CBN_ERROR_NONE) {
        status = CbnMonPro(out, &cctx->t, out,
                            cctx->n, cctx->ni0, cctx->ctx);
    }
    break;
  default: {}
  }

  Done:
  if ((status != CBN_CONTINUE) && (cctx->t.digits != out->digits)) {
    CbnReleaseDigits(&cctx->t, cctx->ctx);
  }

  return status;
}

// Return true if given bit position in the CBN is One.
// Bit 0 is the least significant bit.
// If the bitIndex is larger than the width of the CBN,
// this routine returns false.
int SecurityHandler::CbnBitIsSet(CBN *a, uint16_t bitIndex) {
  int status;
  uint16_t temp;
  DIGIT value;
  M_StackOverflow(SESSION1, return CBN_ERROR);

  if (a == NULL) {
    return false;
  }
  // Include overflow digit.
  if (bitIndex >= DIGITBITS * (1 + a->width)) {
    return false;
  }

  temp = bitIndex >> DIGITLOGBITS;
  value = a->digits[temp];
  temp = bitIndex & DIGITLOGMASK;
  if ((value & (1 << temp)) != 0) {
    status = true;
  } else {
    status = false;
  }

  return status;
}

// Create a full-sized CBN from a long.
int SecurityHandler::CbnFromDigit2(CBN *num, DIGIT2 value, CBNCTX *ctx) {
  // Create a full-sized CBN from a long.
  return CbnSizedFromDigit2( num, value, ctx, CBNFULLSIZE );
}

// Perform the Montgomery Reduction
// out = a * R mod n, where R = 2**|n|.
// This is useful in conjunction with
// the Montgomery product (see below).
// a and n must be the same size.
int SecurityHandler::CbnMonRed(CBN *out, CBN *a, CBN *n, CBNCTX *ctx) {
  DIGIT * tDigits;
  DIGIT * amDigits;
  CBN amNum;
  int i;

  int status = CBN_ERROR;
  M_StackOverflow(ctx->Session, return CBN_ERROR);

  // The a value can be halfsize and n fullsize.
  // Compute am = (a * 2**|n|) mod n
  amNum.digits = NULL;
  status = CbnGrabDigitsAndZero(&amNum, (2 * n->width), ctx);
  if (status != 0) {
    goto Done;
  }

  // Copy digits of a into upper half of amNum.
  amDigits = &(amNum.digits[n->width]);
  tDigits = &(a->digits[0]);
  for (i = a->width; i > 0; --i) {
    *amDigits++ = *tDigits++;
  }

  status = CbnMod(out, &amNum, n, ctx);

  Done:
  CbnReleaseDigits(&amNum, ctx);
  return status;
}

// Multiply two CBN using the Montgomery product
// All inputs must be the same size.
// out = a * b / R mod n, where R = 2**|n|.
// As this is Montgomery multiplication, the caller
// must supply ni0, the LS digit of 1/n mod R.
// This operation is particularly useful if
//  a = x * R mod n  and  b = y * R mod n, as
// it produces  out = x * y * R mod n
// without doing any divisions.
// This will run in constant time independent of input values.
// The CBN for 'out' can be the same as 'a', 'b', or 'n'.
int SecurityHandler::CbnMonPro(CBN *out, CBN *a, CBN *b, CBN *n, DIGIT ni0,
  CBNCTX *ctx) {
  register DIGIT2 temp2;
  DIGIT * tDigits;
  DIGIT * aDigits;
  int nSize;                       // Size of n
  DIGIT * bDigits;
  DIGIT * nDigits;
  CBN tNum;
  CBN aNum;
  CBN bNum;
  register DIGIT borrow;
  int status = CBN_ERROR;
  M_StackOverflow( ctx->Session, return CBN_ERROR );

  aNum.digits = bNum.digits = tNum.digits = NULL;

  status = CbnCopy(&aNum, a, ctx);
  if (status != CBN_ERROR_NONE) {
    goto Done;
  }

  //
  //   The algorithm is described below from paper by Koc, Acar, Kaliski.
  //   Coarse Operand Integration Scanning Method for Mont Mod Mult
  //

  nSize = aNum.width;


  // Allocate and zero t
  status = CbnGrabDigitsAndZero(&tNum, (uint16_t)nSize, ctx);
  if (status != CBN_ERROR_NONE) {
    goto Done;
  }

  // Create copy of b that is s wide.
  // TBD Write a common "resize and copy" function.
  status = CbnGrabDigitsAndZero(&bNum, (uint16_t)nSize, ctx);
  if (status != CBN_ERROR_NONE) {
    goto Done;
  }
  bDigits = b->digits;
  // Use tDigits as a temporary.
  tDigits = bNum.digits;
  for (int i = b->width; i > 0; --i) {
    *tDigits++ = *bDigits++;
  }
  bDigits = bNum.digits;
  tDigits = tNum.digits;
  nDigits = n->digits;
  aDigits = aNum.digits;


  tDigits[nSize] = tDigits[nSize + 1] = 0;

  MonProCore(nSize, ni0, tDigits, nDigits, aDigits, bDigits);

  // aNum is no longer needed, so use it as temp for u calculation
  aDigits = aNum.digits;
  borrow = 0;
  for (int i = 0; i <= nSize; ++i) {
    cbn_diff(borrow, aDigits[i], tDigits[i], nDigits[i]);
  }
  CbnReleaseDigits(out, ctx);
  out->width = (uint16_t)nSize;
  out->negative = false;

  if (borrow == 0) {
    out->digits = aDigits;
  } else {
    out->digits = tDigits;
  }

  Done:
  if (out->digits != tNum.digits) {
    CbnReleaseDigits(&tNum, ctx);
  }
  if (out->digits != aNum.digits) {
    CbnReleaseDigits(&aNum, ctx);
  }
  CbnReleaseDigits(&bNum, ctx);
  return status;
}

void SecurityHandler::MonProCore(int nSize, DIGIT ni0, DIGIT *tDigits,
  DIGIT *nDigits, DIGIT *aDigits, DIGIT *bDigits) {
  DIGIT m, borrow, carry;
  DIGIT2 temp2;

  // Perform multiplication and shift reduce one word at a time
  for (int i = 0; i < nSize; ++i) {
    carry = 0;
    m = bDigits[i];
    for (int j = 0; j < nSize; j++) {
       cbn_sum_mult(carry, tDigits[j], tDigits[j], aDigits[j], m);
    }
    cbn_sum(carry, tDigits[nSize], tDigits[nSize], 0);
    tDigits[nSize + 1] = carry;
    carry = 0;
    m = (DIGIT) (tDigits[0] * ni0); // Mod W is implicit
    // Use borrow as temp var to throw away unused sum
    cbn_sum_mult(carry, borrow, tDigits[0], m, nDigits[0]);
    Borrow(borrow);
    for (int j = 1; j < nSize; j++) {
       cbn_sum_mult(carry, tDigits[j - 1], tDigits[j], m, nDigits[j]);
    }
    cbn_sum(carry, tDigits[nSize - 1], tDigits[nSize], 0);
    tDigits[nSize] = (DIGIT)(tDigits[nSize + 1] + carry);
  }
}

// Returns the index of the most significant digit of bNum.
DIGIT SecurityHandler::CbnNumSignificantDigits(CBN *bNum) {
  M_StackOverflow( SESSION1, return CBN_ERROR );
  for (int i = bNum->width - 1; i > 0; i--) {
    if (bNum->digits[i] > 0) {
      return (DIGIT)(i + 1);
    }
  }
  return 1;
}

void SecurityHandler::Borrow(DIGIT n) {}

} // namespace kinetic
} // namespace seagate
} // namespace com
