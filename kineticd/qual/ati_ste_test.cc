#include <iostream>
#include <vector>
#include "includes/ati_ste_test.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

AtiSteTest::AtiSteTest(int id) : BaseTest(id) {}

bool AtiSteTest::ReadFull() {
    uint64_t lba;
    for (uint32_t zone = kSTART_ZONE; zone <= kHIGHEST_ZONE; ++zone) {
        lba = zone_to_lba(zone);
        if (read(read_fd_, lba, kZONE_SIZE, data_buffer_) <= 0) {
            return false;
        }
    }

    return true;
}

bool AtiSteTest::ReadTargeted(double target_percent) {
    WriteResults("Sequential Read");

    uint64_t lba;
    uint32_t zone;

    // Read all of the conventional zones
    for (zone = kSTART_ZONE; zone < kSTART_SMR_ZONE; ++zone) {
        lba = zone_to_lba(zone);
        if (read(read_fd_, lba, kZONE_SIZE, data_buffer_) <= 0) {
            return false;
        }
    }

    double percent_per_loop = (target_percent - percent_complete_)/(kHIGHEST_ZONE - kSTART_SMR_ZONE);

    // Read the first few tracks of each SMR zone
    for (zone = kSTART_SMR_ZONE; zone < kHIGHEST_ZONE; ++zone) {
        lba = zone_to_lba(zone);

        for (uint16_t tracks = 0; tracks < kNTRACKS_TARGETED; ++tracks) {
            if (read(read_fd_, lba, ROUNDUP(kLBAS_TRACK_SMR, kLBA_SIZE), data_buffer_) <= 0) {
                return false;
            }

            lba += ROUNDUP(kLBAS_TRACK_SMR, kLBA_SIZE);
        }

        percent_complete_ += percent_per_loop;
        UpdateStatus();
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool AtiSteTest::SequentialSeek(double target_percent) {
    double percent_per_loop = (target_percent - percent_complete_)/(3 * (kWRITES_PER_ANALYZE + 1));

    // Do main loop 4 times so that 120,000 ATI writes will have been completed
    for (unsigned int i = 0; i < kWRITE_LOOPS; ++i) {
        WriteResults("Sequential Seek Loop " + std::to_string(i + 1));

        // Write loop; each loop constitutes one write
        for (uint32_t writes = 0; writes < kWRITES_PER_ANALYZE; ++writes) {
            // Write SMR zones
            for (uint32_t zone = kSTART_SMR_ZONE; zone <= kHIGHEST_ZONE; zone += 2) {
                if (write_zone(zac_kin_, zone, kZONE_SIZE, data_buffer_) <= 0) {
                    WriteResults("Write failed");
                    return false;
                }
            }

            // Write conventional zones
            uint64_t lba;
            uint32_t track = 0;
            uint32_t bytes_per_track = kLBAS_TRACK_CONV * kLBA_SIZE;
            for (lba = zone_to_lba(kSTART_ZONE); lba < zone_to_lba(kSTART_SMR_ZONE); lba += kLBAS_TRACK_CONV) {
                // Alternate writing and skipping to create "victim/aggressor" pattern
                if (track % 2 == 0) {
                    if (write(zac_kin_, lba, bytes_per_track, data_buffer_) <= 0) {
                        WriteResults("Write failed");
                        return false;
                    }
                }

                ++track;
            }

            // Handle last partial track of the conventional zones
            if (track % 2 == 0) {
                // Write the partial track if it is an aggressor
                lba -= kLBAS_TRACK_CONV;
                size_t to_write = (zone_to_lba(kSTART_SMR_ZONE) - lba) * kLBA_SIZE;
                if (write(zac_kin_, lba, to_write, data_buffer_) <= 0) {
                    WriteResults("Write failed");
                    return false;
                }
            }

            // Flush writes
            if (zac_kin_.FlushCacheAta() != 0) {
                std::cout << "Flush cache failed" << std::endl;
                return false;
            }

            percent_complete_ += percent_per_loop;
            UpdateStatus();
        }

        if (!ReadTargeted(percent_complete_ + percent_per_loop)) {
            WriteResults("Sequential read failed");
            return false;
        }

        if (!GetLogs()) {
            return false;
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool AtiSteTest::LongSeek(double target_percent) {
    double percent_per_loop = (target_percent - percent_complete_)/(3 * (kWRITES_PER_ANALYZE + 1));
    uint64_t lba = zone_to_lba(kSTART_ZONE);
    uint32_t zones_per_test = (kHIGHEST_ZONE - kSTART_SMR_ZONE) / kNTEST_ZONES;
    std::vector<uint32_t> test_zones(kNTEST_ZONES);
    uint32_t bytes_per_track = kLBAS_TRACK_CONV * kLBA_SIZE;

    // Populate test zones array with the start zone for each test
    for (unsigned int i = 0; i < kNTEST_ZONES; ++i) {
        test_zones[i] = kSTART_SMR_ZONE + (i * zones_per_test);
    }

    // Do main loop 4 times so that 120,000 ATI writes will have been completed
    for (unsigned int i = 0; i < kWRITE_LOOPS; ++i) {
        WriteResults("Long Seek Loop " + std::to_string(i + 1));

        // Write loop; each loop constitutes one write
        for (uint32_t writes = 0; writes < kWRITES_PER_ANALYZE; ++writes) {
            for (uint32_t zone = 0; zone < zones_per_test; ++zone) {
                // Write SMR zones
                for (unsigned int idx = 0; idx < kNTEST_ZONES; ++idx) {
                    if (write_zone(zac_kin_, test_zones[idx], kZONE_SIZE, data_buffer_) <= 0) {
                        WriteResults("Write failed");
                        return false;
                    }

                    test_zones[idx] += 2;
                }

                // Write conventional zones
                if (lba < zone_to_lba(kSTART_SMR_ZONE)) {
                    if (write(zac_kin_, lba, bytes_per_track, data_buffer_) <= 0) {
                        WriteResults("Write failed");
                        return false;
                    }

                    lba += (2 * kLBAS_TRACK_CONV);
                }

                // Flush writes
                if (zac_kin_.FlushCacheAta() != 0) {
                    std::cout << "Flush cache failed" << std::endl;
                    return false;
                }
            }

            percent_complete_ += percent_per_loop;
            UpdateStatus();
        }

        if (!ReadTargeted(percent_complete_ + percent_per_loop)) {
            WriteResults("Sequential read failed");
            return false;
        }

        if (!GetLogs()) {
            return false;
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool AtiSteTest::AtiSteTest::RunTest(double target_percent) {
    WriteResults("Starting ATI/STE test");

    double seek_target_percent = percent_complete_ + (target_percent - percent_complete_) / 2;
    if (!SequentialSeek(seek_target_percent)) {
        WriteResults("Sequential seek test failed");
        return false;
    }

    if (!LongSeek(target_percent)) {
        WriteResults("Long seek test failed");
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();
    WriteResults("ATI/STE test completed");

    return true;
}

}  // namespace qual_kin
