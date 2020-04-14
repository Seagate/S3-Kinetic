#include <iostream>
#include <chrono>
#include <random>
#include <time.h>
#include <algorithm>
#include <string.h>
#include "includes/butterfly_seek_test.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using namespace std::chrono; // NOLINT

namespace qual_kin {

ButterflySeekTest::ButterflySeekTest(int id) : BaseTest(id) {}

bool ButterflySeekTest::TimedRandomWriteRead(double target_percent) {
    WriteResults("Random Write Read Loop");
    ResetAllZones();
    std::default_random_engine generator(time(NULL));
    std::uniform_int_distribution<uint32_t> distribution(kSTART_ZONE, kHIGHEST_ZONE);
    high_resolution_clock::time_point start = high_resolution_clock::now();

    uint8_t* read_buff = allocate_buffer(kMIN_WRITE_SIZE, 0);

    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < kRANDOM_WRITE_READ_TIME) {
        for (uint32_t i = 0; i < kOPS_PER_ANALYZE; ++i) {
            uint32_t zone = distribution(generator);
            uint64_t lba = GetWritePtr(zone);

            // Write lba
            if (write(zac_kin_, lba, kMIN_WRITE_SIZE, data_buffer_) <= 0) {
                WriteResults("Random Write Read failed on write");
                return false;
            }

            // Flush write
            if (zac_kin_.FlushCacheAta() != 0) {
                std::cout << "Flush cache failed" << std::endl;
                return false;
            }

            // Read back lba
            if (read(read_fd_, lba, kMIN_WRITE_SIZE, read_buff) <= 0) {
                WriteResults("Random Write Read failed on read");
                return false;
            }

            // Compare write and read buffers
            if (memcmp(data_buffer_, read_buff, kMIN_WRITE_SIZE) != 0) {
                WriteResults("Random Write Read failed on data compare");
                std::cout << "Read did not match write!" << std::endl;
                return false;
            }

            UpdateWritePtr(zone, kMIN_WRITE_SIZE / kLBA_SIZE);
        }
    }

    free(read_buff);
    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool ButterflySeekTest::ButterflyWrite(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Butterfly Write");
    ResetAllZones();
    uint32_t middle = low_zone + (high_zone - low_zone) / 2;
    uint32_t low = middle - 1;
    uint32_t high = middle + 1;

    // Write middle
    if (write(zac_kin_, zone_to_lba(middle), kBUTTERFLY_WRITE_SIZE, data_buffer_) <= 0) {
        return false;
    }

    // Flush write
    if (zac_kin_.FlushCacheAta() != 0) {
        std::cout << "Flush cache failed" << std::endl;
        return false;
    }

    uint32_t i = 0;
    while (low > low_zone || high < high_zone) {
        low = middle - 1 > (2 * i) ? middle - 1 - 2 * i : 0;
        high = 2 * i + middle + 1;

        // Truncate if low is less than the lowest zone
        if (low < low_zone) {
            low = low_zone;
        }

        // Truncate if high is greater than the highest zone
        if (high > high_zone) {
            high = high_zone;
        }

        // Write high
        if (write(zac_kin_, zone_to_lba(high), kBUTTERFLY_WRITE_SIZE, data_buffer_) <= 0) {
            return false;
        }

        // Flush write
        if (zac_kin_.FlushCacheAta() != 0) {
            std::cout << "Flush cache failed" << std::endl;
            return false;
        }

        // Write low
        if (write(zac_kin_, zone_to_lba(low), kBUTTERFLY_WRITE_SIZE, data_buffer_) <= 0) {
            return false;
        }

        // Flush write
        if (zac_kin_.FlushCacheAta() != 0) {
            std::cout << "Flush cache failed" << std::endl;
            return false;
        }

        ++i;
    }

    if (!GetLogs()) {
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

// Zone-aligned implementation
bool ButterflySeekTest::ButterflyReadZone(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Butterfly Read");
    uint32_t middle = low_zone + (high_zone - low_zone) / 2;
    uint32_t low = middle - 1;
    uint32_t high = middle + 1;

    // Read middle
    if (read(read_fd_, middle, kBUTTERFLY_READ_SIZE, data_buffer_) <= 0) {
        return false;
    }

    uint32_t i = 0;
    while (low > low_zone || high < high_zone) {
        low = middle - 1 > (2 * i) ? middle - 1 - 2 * i : 0;
        high = 2 * i + middle + 1;

        // Truncate if low is less than the lowest zone
        if (low < low_zone) {
            low = low_zone;
        }

        // Truncate if high is greater than the highest zone
        if (high > high_zone) {
            high = high_zone;
        }

        // Read high
        if (read(read_fd_, high, kBUTTERFLY_READ_SIZE, data_buffer_) <= 0) {
            return false;
        }

        // Read low
        if (read(read_fd_, low, kBUTTERFLY_READ_SIZE, data_buffer_) <= 0) {
            return false;
        }

        ++i;
    }

    if (!GetLogs()) {
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool ButterflySeekTest::ButterflyReadLba(uint64_t low_lba, uint64_t high_lba, double target_percent) {
    WriteResults("Butterfly Read");
    uint64_t middle = low_lba + (high_lba - low_lba) / 2;
    uint64_t low = middle - 1;
    uint64_t high = middle + 1;

    // Read middle
    if (read(read_fd_, middle, kBUTTERFLY_READ_SIZE, data_buffer_) <= 0) {
        return false;
    }

    uint64_t i = 0;
    while (low > low_lba || high < high_lba) {
        low = middle - 1 > (2 * i) ? middle - 1 - 2 * i : 0;
        high = 2 * i + middle + 1;

        // Truncate if low is less than the lowest lba
        if (low < low_lba) {
            low = low_lba;
        }

        // Truncate if high is greater than the highest lba
        if (high > high_lba) {
            high = high_lba;
        }

        // Read high
        if (read(read_fd_, high, kBUTTERFLY_READ_SIZE, data_buffer_) <= 0) {
            return false;
        }

        // Read low
        if (read(read_fd_, low, kBUTTERFLY_READ_SIZE, data_buffer_) <= 0) {
            return false;
        }

        ++i;
    }

    if (!GetLogs()) {
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

// EMC uses read for the seek portion of the test if the drive interface is SATA
bool ButterflySeekTest::ButterflySeek(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    return ButterflyReadZone(low_zone, high_zone, target_percent);
}

bool ButterflySeekTest::Verify(double target_percent) {
    double percent_per_step = (target_percent - percent_complete_) / 2;

    if (!SequentialRead(data_size_, data_buffer_, percent_complete_ + percent_per_step) || !GetLogs()) {
        return false;
    }

    if (!TimedRandomWriteRead(percent_complete_ + percent_per_step) || !GetLogs()) {
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool ButterflySeekTest::MainWorkload(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    double percent_per_step = (target_percent - percent_complete_) / 6;

    if (!ButterflySeek(low_zone, high_zone, percent_complete_ + percent_per_step)) {
        WriteResults("Butterfly Seek failed");
        return false;
    }

    if (!Verify(percent_complete_ + percent_per_step)) {
        return false;
    }

    if (!ButterflyWrite(low_zone, high_zone, percent_complete_ + percent_per_step)) {
        WriteResults("Butterfly Write failed");
        return false;
    }

    if (!Verify(percent_complete_ + percent_per_step)) {
        return false;
    }

    if (!ButterflyReadZone(low_zone, high_zone, percent_complete_ + percent_per_step)) {
        WriteResults("Butterfly Read failed");
        return false;
    }

    if (!Verify(percent_complete_ + percent_per_step)) {
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool ButterflySeekTest::RunTest(double target_percent) {
    WriteResults("Starting Butterfly Seek test");

    double percent_per_step = (target_percent - percent_complete_) / kLOOPS;

    for (unsigned int i = 0; i < kLOOPS; ++i) {
        if (i == 0) {
            // Run workload on full disk
            if (!MainWorkload(kSTART_ZONE, kHIGHEST_ZONE, percent_complete_ + percent_per_step)) {
                WriteResults("Butterfly Seek full disk failed");
                return false;
            }
        } else if (i == 1) {
            // Run workload on 10% OD
            const uint32_t n_zones = (kHIGHEST_ZONE - kSTART_ZONE)/10;
            if (!MainWorkload(kSTART_ZONE, kSTART_ZONE + n_zones, percent_complete_ + percent_per_step)) {
                WriteResults("Butterfly Seek 10% OD failed");
                return false;
            }
        } else {
            // Run workload on 10% ID
            const uint32_t n_zones = (kHIGHEST_ZONE - kSTART_ZONE)/10;
            if (!MainWorkload(kHIGHEST_ZONE - n_zones, kHIGHEST_ZONE, percent_complete_ + percent_per_step)) {
                WriteResults("Butterfly Seek 10% ID failed");
                return false;
            }
        }

        if (!GetLogs()) {
            return false;
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();
    WriteResults("Butterfly Seek test completed");

    return true;
}

}  // namespace qual_kin
