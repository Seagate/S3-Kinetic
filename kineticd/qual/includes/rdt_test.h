#ifndef KINETIC_QUAL_INCLUDES_RDT_TEST_H_
#define KINETIC_QUAL_INCLUDES_RDT_TEST_H_
#include "butterfly_seek_test.h"
#include "drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

// Class responsible for the Reliability Demonstration test.
//
// The RDT procedure is as follows:
//   1) Random write for 1 day.
//   2) Sequential verify full disk.
//   3) Random write/verify for 1 day.
//   4) Sequential read/write full disk for 2 days.
//   5) Random read for 1 day.
//   6) Butterfly seek.
//   7) Random write/read for 1 day.
//   8) Sequential verify full disk.
//   9) Repeat steps 1-8 for 2000 hours.
class RdtTest : public ButterflySeekTest {
 public:
    explicit RdtTest(int id);

    // Runs the RDT test, incrementing the percent complete up to the target percent.
    virtual bool RunTest(double target_percent = 100);

 protected:
    // Runs the Butterfly Seek test on the specified zone range.
    virtual bool ButterflySeek(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Runs the main RDT workload (steps 1-6 described above) on the specified zone range.
    bool MainWorkload(uint32_t low_zone, uint32_t high_zone, double target_percent);

 private:
    // Full sequential write followed by full sequential read repeated for 48 hours.
    bool SequentialLoop(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Random writes for 1 day.
    bool RandomWrite(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Random reads for 1 day.
    bool RandomRead(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Sequential verify of full disk.
    bool SequentialVerify(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Random write/verify for 1 day.
    bool RandomWriteVerify(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Random write/read for 1 day.
    bool RandomWriteRead(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Static constants for test durations.
#ifndef SIMPLIFIED
    static const uint32_t kSEQUENTIAL_LOOP_TIME = 48 * 60 * 60;      // 48 hours
    static const uint32_t kRANDOM_READ_TIME = 24 * 60 * 60;          // 24 hours
    static const uint32_t kRANDOM_WRITE_TIME = 24 * 60 * 60;         // 24 hours
    static const uint32_t kRANDOM_WRITE_READ_TIME = 24 * 60 * 60;    // 24 hours
    static const uint32_t kRANDOM_WRITE_VERIFY_TIME = 24 * 60 * 60;  // 24 hours
    static const uint32_t kTOTAL_RUN_TIME = 2000 * 60 * 60;          // 2000 hours
#else
    static const uint32_t kSEQUENTIAL_LOOP_TIME = 15;      // 15 seconds
    static const uint32_t kRANDOM_READ_TIME = 15;          // 15 seconds
    static const uint32_t kRANDOM_WRITE_TIME = 15;         // 15 seconds
    static const uint32_t kRANDOM_WRITE_READ_TIME = 15;    // 15 seconds
    static const uint32_t kRANDOM_WRITE_VERIFY_TIME = 15;  // 15 seconds
    static const uint32_t kTOTAL_RUN_TIME = 15;            // 15 seconds
#endif
};

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_RDT_TEST_H_
