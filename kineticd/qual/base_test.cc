#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fcntl.h>
#include "includes/base_test.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

const std::string BaseTest::kSMART_CMD = "smartctl /dev/sda -A -l sataphy";
const std::string BaseTest::kOUTPUT_PATH = "/tmp/";
const std::string BaseTest::kRESULTS_NAME = "qualification_results.txt";
const std::string BaseTest::kSTATUS_FILENAME = "qualification_status.txt";

BaseTest::BaseTest(int id)
    : id_(id), percent_complete_(0), zac_kin_(&zac_ata_), write_ptrs_(kHIGHEST_ZONE - kSTART_ZONE + 1, 0) {
        // Open device
        zac_fd_ = zac_kin_.OpenDevice(kDEVICE_NAME);

        // Open device for read
        read_fd_ = open(kDEVICE_NAME, (O_DIRECT | O_RDWR));

        // Allocate read/write buffer
        data_size_ = kZONE_SIZE;
        data_buffer_ = allocate_buffer(data_size_, 170);

        // Write initial status
        UpdateStatus();
    }

BaseTest::~BaseTest() {
    zac_kin_.CloseDevice();

    if (data_buffer_) {
        free(data_buffer_);
    }
}

bool BaseTest::RunBaselineTest() {
    WriteResults("Starting baseline test");

#ifndef SIMPLIFIED
    if (!corrupt_superblocks(zac_kin_)) {
        WriteResults("Unable to corrupt superblocks");
        return false;
    }

    // Do a sequential fill
    if (!SequentialFill(data_size_, data_buffer_, 0)) {
        return false;
    }

    // Do a sequential read
    if (!SequentialRead(data_size_, data_buffer_, 0)) {
        return false;
    }
#endif

    WriteResults("Baseline test completed");

    return true;
}

void BaseTest::UpdateStatus() {
    std::ofstream status_file(kOUTPUT_PATH + kSTATUS_FILENAME);
    if (status_file.is_open()) {
        status_file << "ID: " << std::to_string(id_) << "\nPercent Complete: " << std::to_string(percent_complete_);
        status_file.close();
    }
}

void BaseTest::WriteResults(std::string msg) {
    std::ofstream results_file(GetResultsName(), std::ofstream::app);
    FormatMessage(msg);

    if (results_file.is_open()) {
        results_file << msg << "\n";
        results_file.close();
    }
}

bool BaseTest::SequentialFill(size_t data_size, void* data, double target_percent) {
    WriteResults("Sequential Write");
    double percent_per_loop = (target_percent - percent_complete_)/(kHIGHEST_ZONE - kSTART_ZONE);

    for (uint32_t zone = kSTART_ZONE; zone <= kHIGHEST_ZONE; ++zone) {
        if (write_zone(zac_kin_, zone, data_size, data) <= 0) {
            WriteResults("Sequential Write failed");
            return false;
        }

        if (target_percent != 0) {
            percent_complete_ += percent_per_loop;
            UpdateStatus();
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool BaseTest::SequentialRead(size_t data_size, void* data, double target_percent) {
    WriteResults("Sequential Read");
    double percent_per_loop = (target_percent - percent_complete_)/(kHIGHEST_ZONE - kSTART_ZONE);
    uint64_t lba;

    for (uint32_t zone = kSTART_ZONE; zone <= kHIGHEST_ZONE; ++zone) {
        lba = zone_to_lba(zone);
        if (read(read_fd_, lba, data_size, data) <= 0) {
            WriteResults("Sequential Read failed");
            return false;
        }

        if (target_percent != 0) {
            percent_complete_ += percent_per_loop;
            UpdateStatus();
        }
    }

    percent_complete_ = target_percent;
    UpdateStatus();

    return true;
}

bool BaseTest::GetLogs() {
    FILE *res = popen(kSMART_CMD.c_str(), "r");
    char buff[512];

    // Execute smartctl command
    if (!res) {
        WriteResults("Get logs failed");
        return false;
    }

    // Write output to stringstream
    std::stringstream ss;
    while (fgets(buff, sizeof(buff), res) != NULL) {
        ss << buff;
    }
    pclose(res);

    // Cut off the header from smartctl output
    std::string smart = ss.str();
    smart.erase(0, smart.find("ID#"));

    // Create and format header string
    std::string header = "SMART Log:";
    FormatMessage(header);

    // Write header and smartctl output to file
    std::ofstream results_file(GetResultsName(), std::ofstream::app);
    if (results_file.is_open()) {
        results_file << header << "\n" << smart;
        results_file.close();
        return true;
    } else {
        WriteResults("Get logs failed");
        return false;
    }
}

std::string BaseTest::GetResultsName() {
    return std::string(kOUTPUT_PATH + std::to_string(id_) + "_" + kRESULTS_NAME);
}

void BaseTest::FormatMessage(std::string& msg) {
    std::stringstream ss;
    std::time_t end_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string time_stamp = std::ctime(&end_time);  // NOLINT
    time_stamp.erase(time_stamp.length() - 1);     // Cut off newline character

    ss << percent_complete_ << "% " << time_stamp << "] " << msg;
    msg = ss.str();
}

bool BaseTest::ResetAllZones() {
    if (zac_kin_.ResetAllZones() != 0) {
        std::cout << "Reset all zones failed" << std::endl;
        return false;
    }

    if (zac_kin_.FlushCacheAta() != 0) {
        std::cout << "Flush cache failed" << std::endl;
        return false;
    }

    for (auto& it : write_ptrs_) {
        it = 0;
    }

    return true;
}

uint64_t BaseTest::GetWritePtr(uint32_t& zone_id) {
    uint32_t idx = zone_id - kSTART_ZONE;
    return write_ptrs_[idx] + zone_to_lba(zone_id);
}

void BaseTest::UpdateWritePtr(uint32_t& zone_id, size_t offset) {
    uint32_t idx = zone_id - kSTART_ZONE;
    write_ptrs_[idx] += offset;
}

}  // namespace qual_kin
