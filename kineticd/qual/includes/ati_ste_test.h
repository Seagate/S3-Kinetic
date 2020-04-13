#ifndef KINETIC_QUAL_INCLUDES_ATI_STE_TEST_H_
#define KINETIC_QUAL_INCLUDES_ATI_STE_TEST_H_
#include "base_test.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

// Class responsible for the ATI/STE test.
//
// The ATI/STE procedure is as follows:
//   1) Divide drive into 16 test zones.
//   2) Within each test zone, alternate between victim and agressor tracks. One write of all zones constitutes one
//      write loop.
//   3) Every 30,000 writes, perform a sequential read to check for ATI or STE.
//   4) Perform a total of 120,000 writes to complete the test.
class AtiSteTest : public BaseTest {
 public:
    explicit AtiSteTest(int id);

    // Runs the ATI/STE test, incrementing the percent complete up to the target percent.
    virtual bool RunTest(double target_percent = 100);

 private:
    // Reads entire disk returning true if successful.
    bool ReadFull();

    // Reads the first few tracks of each SMR zone since they are the only tracks that will be affected by the test.
    bool ReadTargeted(double target_percent);

    // ATI/STE test with the sequential seek pattern of fully writing each test zone before moving on.
    bool SequentialSeek(double target_percent);

    // ATI/STE test with the long seek pattern of writting first track of each test zone, then second track, etc.
    bool LongSeek(double target_percent);

    static const uint8_t kNTEST_ZONES = 16;
    static const uint16_t kNTRACKS_TARGETED = 20;

#ifndef SIMPLIFIED
    static const uint16_t kWRITES_PER_ANALYZE = 30000;
    static const uint16_t kWRITE_LOOPS = 4;
#else
    static const uint16_t kWRITES_PER_ANALYZE = 1;
    static const uint16_t kWRITE_LOOPS = 1;
#endif
};

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_ATI_STE_TEST_H_
