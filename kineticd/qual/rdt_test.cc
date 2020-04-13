#include <iostream>
#include <chrono>
#include <random>
#include <time.h>
#include <algorithm>
#include <string.h>
#include "includes/rdt_test.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using namespace std::chrono; // NOLINT

namespace qual_kin {

RdtTest::RdtTest(int id) : ButterflySeekTest(id) {}

bool RdtTest::SequentialLoop(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Sequential Loop");
    high_resolution_clock::time_point start = high_resolution_clock::now();
    double elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
    double delta = target_percent - percent_complete_;

    while (elapsed < kSEQUENTIAL_LOOP_TIME) {
        // Sequential read loop
        for (uint32_t zone = low_zone; zone <= high_zone; ++zone) {
            uint64_t lba = zone_to_lba(zone);
            if (read(read_fd_, lba, data_size_, data_buffer_) <= 0) {
                WriteResults("Sequential Loop failed on read");
                return false;
            }
        }

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ += delta * std::min(elapsed/kSEQUENTIAL_LOOP_TIME, 1.0);
        UpdateStatus();

        // Sequential write loop
        for (uint32_t zone = low_zone; zone <= high_zone; ++zone) {
            if (write_zone(zac_kin_, zone, data_size_, data_buffer_) <= 0) {
                WriteResults("Sequential Loop failed on write");
                return false;
            }
        }

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ += delta * std::min(elapsed/kSEQUENTIAL_LOOP_TIME, 1.0);
        UpdateStatus();
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::RandomWrite(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Random Write Loop");
    ResetAllZones();
    std::default_random_engine generator(time(NULL));
    std::uniform_int_distribution<uint32_t> distribution(low_zone, high_zone);
    high_resolution_clock::time_point start = high_resolution_clock::now();
    double elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
    double delta = target_percent - percent_complete_;

    while (elapsed < kRANDOM_WRITE_TIME) {
        for (uint32_t i = 0; i < kOPS_PER_ANALYZE; ++i) {
            uint32_t zone = distribution(generator);
            uint64_t lba = GetWritePtr(zone);

            // Write lba
            if (write(zac_kin_, lba, kMIN_WRITE_SIZE, data_buffer_) <= 0) {
                WriteResults("Random Write failed");
                return false;
            }

            UpdateWritePtr(zone, kMIN_WRITE_SIZE / kLBA_SIZE);
        }

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ = delta * std::min(elapsed/kRANDOM_WRITE_TIME, 1.0);
        UpdateStatus();
    }

    if (zac_kin_.FlushCacheAta() != 0) {
        std::cout << "Flush cache failed" << std::endl;
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::RandomRead(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Random Read Loop");
    std::default_random_engine generator(time(NULL));
    std::uniform_int_distribution<uint64_t> distribution(zone_to_lba(low_zone), zone_to_lba(high_zone + 1));
    high_resolution_clock::time_point start = high_resolution_clock::now();
    double elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
    double delta = target_percent - percent_complete_;

    while (elapsed < kRANDOM_READ_TIME) {
        for (uint32_t i = 0; i < kOPS_PER_ANALYZE; ++i) {
            uint64_t lba = distribution(generator);
            if (read(read_fd_, lba, kLBA_SIZE, data_buffer_) <= 0) {
                WriteResults("Random Read failed");
                return false;
            }
        }

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ = delta * std::min(elapsed/kRANDOM_READ_TIME, 1.0);
        UpdateStatus();
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::SequentialVerify(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    // Sequentially verify all LBAs
    WriteResults("Sequential Verify Loop");

    for (uint32_t zone = low_zone; zone <= high_zone; ++zone) {
        uint64_t lba = zone_to_lba(zone);
        size_t to_verify = zone < kSTART_SMR_ZONE ? kZONE_SIZE / kLBA_SIZE : GetWritePtr(zone) - lba;
        size_t verified;

        while (to_verify > 0) {
            verified = read_verify(zac_fd_, zac_ata_, lba, to_verify);

            if (verified <= 0) {
                WriteResults("Sequential Verify failed");
                return false;
            } else {
                lba += verified;
                to_verify -= verified;
            }
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::RandomWriteVerify(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Random Write Verify Loop");
    ResetAllZones();
    std::default_random_engine generator(time(NULL));
    std::uniform_int_distribution<uint32_t> distribution(low_zone, high_zone);
    high_resolution_clock::time_point start = high_resolution_clock::now();
    double elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
    double delta = target_percent - percent_complete_;

    while (elapsed < kRANDOM_WRITE_VERIFY_TIME) {
        for (uint32_t i = 0; i < kOPS_PER_ANALYZE; ++i) {
            uint32_t zone = distribution(generator);
            uint64_t lba = GetWritePtr(zone);

            // Write lba
            if (write(zac_kin_, lba, kMIN_WRITE_SIZE, data_buffer_) <= 0) {
                WriteResults("Random Write Verify failed on write");
                return false;
            }

            // Flush write
            if (zac_kin_.FlushCacheAta() != 0) {
                std::cout << "Flush cache failed" << std::endl;
                return false;
            }

            // Verify lba
            if (read_verify(zac_fd_, zac_ata_, lba, kMIN_WRITE_SIZE / kLBA_SIZE) <= 0) {
                WriteResults("Random Write Verify failed on verify");
                return false;
            }

            UpdateWritePtr(zone, kMIN_WRITE_SIZE / kLBA_SIZE);
        }

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ = delta * std::min(elapsed/kRANDOM_WRITE_VERIFY_TIME, 1.0);
        UpdateStatus();
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::ButterflySeek(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    return ButterflySeekTest::RunTest(target_percent);
}

bool RdtTest::RandomWriteRead(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    WriteResults("Random Write Read Loop");
    ResetAllZones();
    std::default_random_engine generator(time(NULL));
    std::uniform_int_distribution<uint32_t> distribution(low_zone, high_zone);
    high_resolution_clock::time_point start = high_resolution_clock::now();
    double elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
    double delta = target_percent - percent_complete_;

    uint8_t* read_buff = allocate_buffer(kMIN_WRITE_SIZE, 0);

    while (elapsed < kRANDOM_WRITE_READ_TIME) {
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

        elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();
        percent_complete_ = delta * std::min(elapsed/kRANDOM_WRITE_READ_TIME, 1.0);
        UpdateStatus();
    }

    free(read_buff);
    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::MainWorkload(uint32_t low_zone, uint32_t high_zone, double target_percent) {
    double percent_per_test = (target_percent - percent_complete_) / 8;

    if (!RandomWrite(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!SequentialVerify(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!RandomWriteVerify(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!SequentialLoop(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!RandomRead(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!ButterflySeek(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!RandomWriteRead(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    if (!SequentialVerify(low_zone, high_zone, percent_complete_ + percent_per_test) || !GetLogs()) {
        return false;
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool RdtTest::RunTest(double target_percent) {
    WriteResults("Starting TCRDT test");

    high_resolution_clock::time_point start = high_resolution_clock::now();

    // Rough estimation that the 2,000 hour test will take 12 loops
    double percent_per_loop = (target_percent - percent_complete_) / 12;

    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < kTOTAL_RUN_TIME) {
        double new_target = std::min(percent_complete_ + percent_per_loop, target_percent);

        if (!MainWorkload(kSTART_ZONE, kHIGHEST_ZONE, new_target)) {
            return false;
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();
    WriteResults("TCRDT test completed");

    return true;
}

}  // namespace qual_kin
