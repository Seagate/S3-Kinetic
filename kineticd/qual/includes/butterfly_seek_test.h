#ifndef KINETIC_QUAL_INCLUDES_BUTTERFLY_SEEK_TEST_H_
#define KINETIC_QUAL_INCLUDES_BUTTERFLY_SEEK_TEST_H_
#include "base_test.h"
#include "drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

// Class responsible for the Butterfly Seek test.
//
// The Butterfly Seek procedure is as follows:
//   1) One block butterfly seeks followed by sequential reads and random write/reads.
//   2) One block butterfly writes followed by sequential reads and random write/reads.
//   3) One block butterfly reads followed by sequential reads and random write/reads.
//   4) Do steps 1-3 on full disk, 10% OD, and 10% ID.
class ButterflySeekTest : public BaseTest {
 public:
    explicit ButterflySeekTest(int id);

    // Runs the Butterfly Seek test, incrementing the percent complete up to the target percent.
    virtual bool RunTest(double target_percent = 100);

 protected:
    // Main butterfly seek, write, read workload that takes the low and high zones as parameters.
    bool MainWorkload(uint32_t low_zone, uint32_t high_zone, double target_percent);

 private:
    bool ButterflySeek(uint32_t low_zone, uint32_t high_zone, double target_percent);
    bool ButterflyWrite(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Butterfly read workload, zone aligned.
    bool ButterflyReadZone(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Butterfly read workload, lba aligned.
    bool ButterflyReadLba(uint64_t low_lba, uint64_t high_lba, double target_percent);

    // Verify workload of 1 pass sequential reads and 1 hour of random write/reads.
    bool Verify(double target_percent);

    // 1 hour random write/read workload.
    bool TimedRandomWriteRead(double target_percent);

    // Static constant for test duration.
#ifndef SIMPLIFIED
    static const uint32_t kRANDOM_WRITE_READ_TIME = 1 * 60 * 60;  // 1 hour
    static const uint8_t kLOOPS = 3;
#else
    static const uint32_t kRANDOM_WRITE_READ_TIME = 15;           // 15 seconds
    static const uint8_t kLOOPS = 1;
#endif

    static const uint16_t kBUTTERFLY_WRITE_SIZE = kMIN_WRITE_SIZE;
    static const uint16_t kBUTTERFLY_READ_SIZE = kLBA_SIZE;
};

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_BUTTERFLY_SEEK_TEST_H_
