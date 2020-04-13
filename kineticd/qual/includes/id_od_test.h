#ifndef KINETIC_QUAL_INCLUDES_ID_OD_TEST_H_
#define KINETIC_QUAL_INCLUDES_ID_OD_TEST_H_
#include "rdt_test.h"
#include "drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

// Class responsible for the ID OD test.
//
// The ID OD procedure is as follows:
//   1) Run RDT test sequence on 5% OD.
//   2) Run RDT test sequence on 5% ID.
class IdOdTest : public RdtTest {
 public:
    explicit IdOdTest(int id);

    // Runs the ID OD test, incrementing the percent complete up to the target percent.
    virtual bool RunTest(double target_percent = 100);

 protected:
    // Runs the Butterfly Seek test on the specified zone range.
    virtual bool ButterflySeek(uint32_t low_zone, uint32_t high_zone, double target_percent);

 private:
    // Runs the RDT test on the specified zone range.
    bool RunRdt(uint32_t low_zone, uint32_t high_zone, double target_percent);

    // Static constant for test duration.
#ifndef SIMPLIFIED
    static const uint32_t kLOOP_TIME = 48 * 60 * 60;  // 500 hours
    static const uint8_t kBUTTERFLY_LOOPS = 3;
    static const uint8_t kMAIN_LOOPS = 4;
#else
    static const uint32_t kLOOP_TIME = 60;            // 1 minute
    static const uint8_t kBUTTERFLY_LOOPS = 1;
    static const uint8_t kMAIN_LOOPS = 2;
#endif
};

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_ID_OD_TEST_H_
