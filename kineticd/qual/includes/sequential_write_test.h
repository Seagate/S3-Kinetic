#ifndef KINETIC_QUAL_INCLUDES_SEQUENTIAL_WRITE_TEST_H_
#define KINETIC_QUAL_INCLUDES_SEQUENTIAL_WRITE_TEST_H_
#include "base_test.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

// Class responsible for the Sequential Write test.
//
// The Sequential Write procedure is as follows:
//   1) Sequential write for 1 week.
//   2) Sequential read for 2 hours.
//   3) Random write/read for 2 hours.
//   4) Sequential write for 2 hours.
//   5) Sequential read for 2 hours.
//   6) Repeat steps 1-6 for 90 days.
class SequentialWriteTest : public BaseTest {
 public:
    explicit SequentialWriteTest(int id);

    // Runs the Sequential Write test, incrementing the percent complete up to the target percent.
    bool RunTest(double target_percent = 100);

 private:
    // Primary sequential write loop that runs for 1 week.
    bool WriteLoop(double target_percent);

    // 2 hour random write/read workload.
    bool TimedRandomWriteRead();

    // 2 hour sequential write workload.
    bool TimedSequentialWrite();

    // 2 hour sequential read workload.
    bool TimedSequentialRead();

    // Static constants for test durations.
#ifndef SIMPLIFIED
    static const uint32_t kWRITE_LOOP_TIME = 7 * 24 * 60 * 60;    // 1 week
    static const uint32_t kRANDOM_WRITE_READ_TIME = 2 * 60 * 60;  // 2 hours
    static const uint32_t kSEQUENTIAL_WRITE_TIME = 2 * 60 * 60;   // 2 hours
    static const uint32_t kSEQUENTIAL_READ_TIME = 2 * 60 * 60;    // 2 hours
    static const uint32_t kTEST_TIME = 90 * 24 * 60 * 60;         // 90 days
#else
    static const uint32_t kWRITE_LOOP_TIME = 15;         // 15 seconds
    static const uint32_t kRANDOM_WRITE_READ_TIME = 15;  // 15 seconds
    static const uint32_t kSEQUENTIAL_WRITE_TIME = 15;   // 15 seconds
    static const uint32_t kSEQUENTIAL_READ_TIME = 15;    // 15 seconds
    static const uint32_t kTEST_TIME = 15;               // 15 seconds
#endif
};

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_SEQUENTIAL_WRITE_TEST_H_
