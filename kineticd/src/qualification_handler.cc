#include <chrono>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include "thread.h"
#include "glog/logging.h"
#include "popen_wrapper.h"
#include "qualification_handler.h"
#include "mem/DynamicMemory.h"
#include "smrdisk/DriveEnv.h"
#include "key_generator.h"
#include "ati_ste_test.h"
#include "rdt_test.h"
#include "butterfly_seek_test.h"
#include "id_od_test.h"
#include "sequential_write_test.h"

using ::qual_kin::AtiSteTest;
using ::qual_kin::RdtTest;
using ::qual_kin::ButterflySeekTest;
using ::qual_kin::IdOdTest;
using ::qual_kin::SequentialWriteTest;

namespace com {
namespace seagate {
namespace kinetic {

const std::string QualificationHandler::kQUAL_PATTERN = "qual.";
const std::string QualificationHandler::kTEST_PATTERN = "run_test.";
const std::string QualificationHandler::kRESULTS_PATTERN = "get_results.";

const std::string QualificationHandler::kATI_STE_TEST = "ati_ste";
const std::string QualificationHandler::kBUTTERFLY_TEST = "butterfly_seek";
const std::string QualificationHandler::kID_OD_TEST = "id_od";
const std::string QualificationHandler::kRDT_TEST = "rdt";
const std::string QualificationHandler::kSEQUENTIAL_WRITE_TEST = "sequential_write";

const std::string QualificationHandler::kFILL_SELF = "fill_self";
const std::string QualificationHandler::kGET_STATUS = "get_status";
const std::string QualificationHandler::kABORT_TEST = "abort_test";
const std::string QualificationHandler::kOUTPUT_PATH = "/tmp/";
const std::string QualificationHandler::kRESULTS_NAME = "_qualification_results.txt";
const std::string QualificationHandler::kSTATUS_FILENAME = "qualification_status.txt";

QualificationHandler::QualificationHandler(KeyValueStoreInterface& primary_data_store,
                                           DeviceInformationInterface& device_information)
    : primary_data_store_(primary_data_store), device_information_(device_information), id_(0), test_running_(false) {}

bool QualificationHandler::HandleRequest(std::string& device_name, std::string& value) {
    // Strip the qualification pattern prefix
    StripPrefix(device_name, kQUAL_PATTERN);

    if (device_name == kGET_STATUS) {
        GetStatus(value);
    } else if (device_name == kABORT_TEST) {
        AbortTest(value);
    } else if (device_name == kFILL_SELF) {
        FillSelf(value);
    } else if (std::regex_match(device_name, std::regex(kTEST_PATTERN + "(.*)"))) {
        StripPrefix(device_name, kTEST_PATTERN);
        RunTest(device_name, value);
    } else if (std::regex_match(device_name, std::regex(kRESULTS_PATTERN + "(.*)"))) {
        StripPrefix(device_name, kRESULTS_PATTERN);
        try {
            GetResults(value, std::stoi(device_name));
        } catch(const std::invalid_argument& ia) {
            value = "Error: results ID must be an unsigned int";
        }
    } else {
        // Device name does not match any commands
        return false;
    }

    return true;
}

void QualificationHandler::RunTest(std::string& test_name, std::string& value) {
    VLOG(3) << "Run test: " << test_name;
    unsigned int id = GetID();
    std::shared_ptr<BaseTest> test;

    // Check if a test is already running
    if (test_running_) {
        value = "Error: a test is already running";
        return;
    }

    // Find test and run it
    if (test_name == kATI_STE_TEST) {
        test = std::shared_ptr<BaseTest>(new AtiSteTest(id));
    } else if (test_name == kBUTTERFLY_TEST) {
        test = std::shared_ptr<BaseTest>(new ButterflySeekTest(id));
    } else if (test_name == kID_OD_TEST) {
        test = std::shared_ptr<BaseTest>(new IdOdTest(id));
    } else if (test_name == kRDT_TEST) {
        test = std::shared_ptr<BaseTest>(new RdtTest(id));
    } else if (test_name == kSEQUENTIAL_WRITE_TEST) {
        test = std::shared_ptr<BaseTest>(new SequentialWriteTest(id));
    } else {
        value = "Error: unknown test";
        return;
    }

    // Build thread arguments
    thread_args_.primary_data_store = &primary_data_store_;
    thread_args_.test = test;
    thread_args_.server = server_;
    thread_args_.test_running = &test_running_;

    // Create thread to run test
    int res = pthread_create(&thread_, NULL, ExecuteTest, (void*)&thread_args_);
    if (res != 0) {
        PLOG(ERROR) << "pthread create failed";
        value = "Error: failed with code " + std::to_string(res);
    } else {
        value = "Success: running with ID " + std::to_string(id);
    }
}

void QualificationHandler::FillSelf(std::string& value) {
    VLOG(3) << "Fill self";

    // Check if a test is already running
    if (test_running_) {
        value = "Error: a test is already running";
        return;
    }

    // Build thread arguments
    thread_args_.primary_data_store = &primary_data_store_;
    thread_args_.server = server_;
    thread_args_.test_running = &test_running_;
    thread_args_.device_information = &device_information_;

    // Create thread
    int res = pthread_create(&thread_, NULL, ExecuteFill, (void*)&thread_args_);
    if (res != 0) {
        PLOG(ERROR) << "pthread create failed";
        value = "Error: failed with code " + std::to_string(res);
    } else {
        value = "Success: filling self";
    }
}

void QualificationHandler::GetStatus(std::string& value) {
    VLOG(3) << "Get status";

    // Read from status file
    std::ifstream status_file(kOUTPUT_PATH + kSTATUS_FILENAME);
    if (status_file.is_open()) {
        std::stringstream buffer;
        buffer << status_file.rdbuf();
        status_file.close();
        value = buffer.str();
    } else {
        value = "Error: unable to open status file";
    }
}

void QualificationHandler::GetResults(std::string& value, unsigned int id) {
    VLOG(3) << "Get results: " << id;

    // Get file name associated with the ID
    std::string results_name = GetFileName(id);

    // Read from results file
    std::ifstream results_file(results_name);
    if (results_file.is_open()) {
        std::stringstream buffer;
        buffer << results_file.rdbuf();
        results_file.close();
        value = buffer.str();
    } else {
        value = "Error: unable to open results file, make sure that ID is valid";
    }
}

void QualificationHandler::AbortTest(std::string& value) {
    VLOG(3) << "Abort test";

    // If no tests are running, set value and return
    if (!test_running_) {
        value = "Success";
        return;
    }

    // Try to cancel thread
    int res = pthread_cancel(thread_);
    if (res != 0) {
        PLOG(ERROR) << "pthread cancel failed";
        value = "Error: failed with code " + std::to_string(res);
        return;
    }

    // Change state to store corrupt and set value
    server_->StateChanged(StateEvent::STORE_CORRUPT);
    test_running_ = false;
    value = "Success";
}

unsigned int QualificationHandler::GetID() {
    return id_++;
}

std::string QualificationHandler::GetFileName(unsigned int& id) {
    return kOUTPUT_PATH + std::to_string(id) + kRESULTS_NAME;
}

void QualificationHandler::StripPrefix(std::string& str, std::string prefix) {
    str.erase(0, prefix.length());
}

void* QualificationHandler::ExecuteTest(void* args_ptr) {
    struct ThreadArgs* args = (struct ThreadArgs*) args_ptr;

    // Close database and go into qualification state
    args->primary_data_store->Close();
    args->server->StateChanged(StateEvent::QUALIFICATION);
    *(args->test_running) = true;

    // Run baseline test and then test itself
    if (!args->test->RunBaselineTest()) {
        LOG(WARNING) << "Baseline test failed";
    } else if (!args->test->RunTest()) {
        LOG(WARNING) << "Test failed";
    }

    // Go into store corrupt state
    args->server->StateChanged(StateEvent::STORE_CORRUPT);
    *(args->test_running) = false;

    return NULL;
}

void* QualificationHandler::ExecuteFill(void* args_ptr) {
    chrono::microseconds delta;
    chrono::high_resolution_clock::time_point start, end;

    struct ThreadArgs* args = (struct ThreadArgs*) args_ptr;

    // Go into qualification state and do other qualification setup
    args->server->StateChanged(StateEvent::QUALIFICATION);
    *(args->test_running) = true;
    KeyValueStoreInterface* key_value_store = args->primary_data_store;
    DeviceInformationInterface* dev_info = args->device_information;
    UpdateStatus("Portion full: 0");

    // Create value
    void* data = NULL;
    if (posix_memalign(&data, 4096, kVALUE_SIZE) < 0) {
        args->server->StateChanged(StateEvent::RESTORED);
        *(args->test_running) = false;
        UpdateStatus("Error: unable to allocate memory for value");
        return NULL;
    }
    memset(data, 92, kVALUE_SIZE);

    // Create token
    std::tuple<int64_t, int64_t> token{0, 0}; // NOLINT

    // Create key generator
    KeyGenerator key_gen(kKEY_SIZE);

    std::string key;
    std::string exit_status;
    LevelDBData* value;
    float portion_full;
    bool exit_fill = false;
    unsigned int fill_step_size = 10000;

    while (!exit_fill) {
        start = std::chrono::high_resolution_clock::now();
        for (unsigned int i = 0; i < fill_step_size && !exit_fill; ++i) {
            key = key_gen.GetNextKey();
            value = PackValue((char*) data);
            switch (key_value_store->Put(key, (char*) value, false, token)) {
                case StoreOperationStatus_SUCCESS:
                    break;
                case StoreOperationStatus_NO_SPACE:
                    delete [] value->header;
                    exit_status = "Successfully filled drive";
                    VLOG(3) << exit_status;
                    exit_fill = true;
                    break;
                default:
                    delete [] value->header;
                    exit_status = "Failed to persist key/value in database during fill";
                    exit_fill = true;
                    LOG(ERROR) << exit_status;
                    break;
            }
        }
        delta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start); // NOLINT

        if (!exit_fill) {
            dev_info->GetPortionFull(&portion_full);
            UpdateStatus("Portion full: " + std::to_string(portion_full));
            double throughput = (double) kVALUE_SIZE * (double) fill_step_size / (double) delta.count();
            VLOG(3) <<  "Fill self: " << throughput << " MB/s - " << portion_full << " full";
        }
    }

    // Go into ready state
    args->server->StateChanged(StateEvent::RESTORED);
    *(args->test_running) = false;
    UpdateStatus(exit_status);

    // Clean up memory
    free(data);

    return NULL;
}

LevelDBData* QualificationHandler::PackValue(char* value) {
    InternalValueRecord internal_value_record;

    LevelDBData* myData = new LevelDBData();

    myData->type = LevelDBDataType::MEM_INTERNAL;

    myData->data = value;
    myData->dataSize = (int)kVALUE_SIZE;

    std::string value_str = "";
    internal_value_record.set_value(value_str);
    internal_value_record.set_version("version");
    internal_value_record.set_tag("tag");
    internal_value_record.set_algorithm(1);
    std::string packed_value;

    if (!internal_value_record.SerializeToString(&packed_value)) {
        LOG(ERROR) << "PackValue Serialization failed!";
    }
    myData->headerSize = packed_value.size();
    myData->header = new char[myData->headerSize];
    memcpy(myData->header, packed_value.data(), packed_value.size());

    return myData;
}

void QualificationHandler::UpdateStatus(std::string status) {
    VLOG(3) << "Update status";
    std::ofstream status_file;
    status_file.open(kOUTPUT_PATH + kSTATUS_FILENAME, std::ofstream::out);  //  | std::ofstream::app
    if (status_file.is_open()) {
        status_file << status << "\n";
        status_file.close();
    } else {
        LOG(ERROR) << "Unable to write to status file";
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
