#include <iostream>
#include <chrono>
#include <algorithm>
#include "includes/id_od_test.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using namespace std::chrono; // NOLINT

namespace qual_kin {

IdOdTest::IdOdTest(int id) : RdtTest(id) {}

bool IdOdTest::ButterflySeek(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    double percent_per_step = (target_percent - percent_complete_) / kBUTTERFLY_LOOPS;

    for (unsigned int i = 0; i < kBUTTERFLY_LOOPS; ++i) {
        if (!ButterflySeekTest::MainWorkload(kSTART_ZONE, kHIGHEST_ZONE, percent_complete_ + percent_per_step)) {
            return false;
        }

        if (!GetLogs()) {
            return false;
        }
    }

    return true;
}

bool IdOdTest::RunRdt(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    high_resolution_clock::time_point start = high_resolution_clock::now();

    // Rough estimation that the 500 hour test will take 3 loops
    double percent_per_loop = (target_percent - percent_complete_) / 3;

    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < kLOOP_TIME) {
        double new_target = std::min(percent_complete_ + percent_per_loop, target_percent);

        if (!MainWorkload(low_zone, high_zone, new_target)) {
            return false;
        }
    }

    return true;
}

bool IdOdTest::RunTest(double target_percent) {
    WriteResults("Starting ID OD test");

    const uint32_t n_zones = (kHIGHEST_ZONE - kSTART_ZONE) / 20;  // 5% of disk
    double percent_per_step = (target_percent - percent_complete_) / kMAIN_LOOPS;

    for (unsigned int i = 0; i < kMAIN_LOOPS; ++i) {
        if (i % 2 == 0) {
            // Run RDT on 5% OD
            if (!RunRdt(kSTART_ZONE, kSTART_ZONE + n_zones, percent_complete_ + percent_per_step)) {
                WriteResults("RDT on 5% OD failed");
                return false;
            }
        } else {
            // Run RDT on 5% ID
            if (!RunRdt(kHIGHEST_ZONE - n_zones, kHIGHEST_ZONE, percent_complete_ + percent_per_step)) {
                WriteResults("RDT on 5% ID failed");
                return false;
            }
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();
    WriteResults("ID OD test completed");

    return true;
}

}  // namespace qual_kin
