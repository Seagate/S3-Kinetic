#include <iostream>
#include <chrono>
#include <random>
#include <time.h>
#include <string.h>
#include "includes/sequential_write_test.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using namespace std::chrono;  // NOLINT

namespace qual_kin {

SequentialWriteTest::SequentialWriteTest(int id) : BaseTest(id) {}

bool SequentialWriteTest::WriteLoop(double target_percent) {
    high_resolution_clock::time_point start = high_resolution_clock::now();
    double elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
    double delta = target_percent - percent_complete_;

    while (elapsed < kWRITE_LOOP_TIME) {
        if (!SequentialFill(data_size_, data_buffer_, 0)) {
            return false;
        }

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ += delta * std::min(elapsed/kWRITE_LOOP_TIME, 1.0);
        UpdateStatus();
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool SequentialWriteTest::TimedRandomWriteRead() {
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

    return true;
}

bool SequentialWriteTest::TimedSequentialWrite() {
    WriteResults("Sequential Write Loop");
    uint32_t zone = kSTART_ZONE;
    high_resolution_clock::time_point start = high_resolution_clock::now();

    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < kSEQUENTIAL_WRITE_TIME) {
        if (write_zone(zac_kin_, zone, data_size_, data_buffer_) <= 0) {
            WriteResults("Sequential Write failed");
            return false;
        }

        if (zone >= kHIGHEST_ZONE) {
            zone = kSTART_ZONE;
        } else {
            ++zone;
        }
    }

    return true;
}

bool SequentialWriteTest::TimedSequentialRead() {
    WriteResults("Sequential Read Loop");
    uint32_t zone = kSTART_ZONE;
    high_resolution_clock::time_point start = high_resolution_clock::now();

    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < kSEQUENTIAL_READ_TIME) {
        if (read(read_fd_, zone_to_lba(zone), data_size_, data_buffer_) <= 0) {
            WriteResults("Sequential Read failed");
            return false;
        }

        if (zone >= kHIGHEST_ZONE) {
            zone = kSTART_ZONE;
        } else {
            ++zone;
        }
    }

    return true;
}

bool SequentialWriteTest::RunTest(double target_percent) {
    WriteResults("Starting Sequential Write test");

    high_resolution_clock::time_point start = high_resolution_clock::now();

    // Rough estimation that the 90 day test will take 12 loops
    double percent_per_loop = (target_percent - percent_complete_) / 12;

    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < kTEST_TIME) {
        double new_target = std::min(percent_complete_ + percent_per_loop, target_percent);

        if (!WriteLoop(new_target) || !GetLogs()) {
            return false;
        }

        if (!SequentialRead(data_size_, data_buffer_, 0) || !GetLogs()) {
            return false;
        }

        if (!TimedRandomWriteRead() || !GetLogs()) {
            return false;
        }

        if (!TimedSequentialWrite() || !GetLogs()) {
            return false;
        }

        if (!TimedSequentialRead() || !GetLogs()) {
            return false;
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();
    WriteResults("Sequential Write test completed");

    return true;
}

}  // namespace qual_kin
