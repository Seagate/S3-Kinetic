#ifndef KINETIC_QUAL_INCLUDES_DRIVE_HELPER_H_
#define KINETIC_QUAL_INCLUDES_DRIVE_HELPER_H_
#include <sys/types.h>
#include "zac_mediator.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {
#define ROUNDUP(x, y)  ((((x) + (y) - 1) / (y)) * (y))

// Drive specific constants for Lamarr
static const uint8_t kSTART_SMR_ZONE = 64;      // Beginning of smr zones
static const uint8_t kSUPERBLOCK0 = 62;
static const uint8_t kSUPERBLOCK1 = 63;
static const uint32_t kLBA_SIZE = 512;          // Lba size in bytes
static const uint32_t kZONE_SIZE = 268435456;   // Zone size in bytes
static const uint32_t kLBAS_TRACK_SMR = 3160;   // Lbas per track for smr zones
static const uint32_t kLBAS_TRACK_CONV = 4216;  // Lbas per track for conventional zones
static const uint16_t kMIN_WRITE_SIZE = 4096;   // Smallest possible write size

#ifndef SIMPLIFIED
static const uint8_t kSTART_ZONE = 3;           // 1st zone that can be written
static const uint32_t kHIGHEST_ZONE = 29806;    // Highest zone that can be written
#else
static const uint8_t kSTART_ZONE = 48;
static const uint32_t kHIGHEST_ZONE = 80;
#endif

static const uint32_t kALLIGN = 4096;
static const uint16_t kSMART_DATA_SIZE = 512;
static const char kDEVICE_NAME[] = "/dev/sda";

// Returns the first LBA of the specified zone
inline uint64_t zone_to_lba(const uint32_t zone_id) { return zone_id << 19; }

// Allocates memaligned memory, sets it to the value provided, and returns a pointer to the first byte.
// It is the caller's responsibility to free the memory after use.
uint8_t* allocate_buffer(size_t data_size, int value);

// Reads the specified number of bytes from the drive starting at the lba provided into the buffer.
// Returns the number of bytes read.
size_t read(int fd, uint64_t lba, size_t count, void* buffer);

// Writes data starting at the specified lba. Returns the number of bytes written.
size_t write(ZacMediator& zac_kin, uint64_t lba, size_t count, void* data);

// Writes data to the specified zone. Returns the number of bytes written.
size_t write_zone(ZacMediator& zac_kin, const uint32_t& zone, size_t data_size, void* data);

// Corrupts (writes arbitrary data) to the zone specified.
bool corrupt_zone(ZacMediator& zac_kin, const uint32_t& zone);

// Corrupts kinetic's superblocks to gaurantee that all user data is lost.
bool corrupt_superblocks(ZacMediator& zac_kin);

// Sends ATA read verify command to the drive with the lba and count provided. Returns lbas read.
size_t read_verify(int& fd, AtaCmdHandler& ata_cmd_handler, uint64_t lba, uint32_t count);

// Sends ATA smart read data command to the drive. Returns true if command was successful.
bool smart_read_data(int& fd, AtaCmdHandler& ata_cmd_handler);

// Tests read and write functions.
void test_read_write(int fd, ZacMediator& zac_kin);

// Tests write zone function.
void test_write_zone(int fd, ZacMediator& zac_kin);

// Prints the data buffer to stdout.
void print_databuf(AtaCmdHandler& ata_cmd_handler, size_t buff_size);

// Prints the sense buffer to stdout.
void print_sensebuf(AtaCmdHandler& ata_cmd_handler);

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_DRIVE_HELPER_H_
