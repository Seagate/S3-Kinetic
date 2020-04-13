#ifndef KINETIC_QUALIFICATION_HANDLER_H_
#define KINETIC_QUALIFICATION_HANDLER_H_

#include "server.h"
#include "key_value_store_interface.h"
#include "internal_value_record.pb.h"
#include "device_information_interface.h"
#include "base_test.h"

using ::qual_kin::BaseTest;

namespace com {
namespace seagate {
namespace kinetic {

struct ThreadArgs {
    KeyValueStoreInterface* primary_data_store;
    DeviceInformationInterface* device_information;
    std::shared_ptr<BaseTest> test;
    Server* server;
    bool* test_running;
};

class QualificationHandler {
 public:
    explicit QualificationHandler(KeyValueStoreInterface& primary_data_store,
                                  DeviceInformationInterface& device_information);
    bool HandleRequest(std::string& device_name, std::string& value);
    void SetServer(Server* server) {
        server_ = server;
    }

    static void* ExecuteTest(void* args_ptr);
    static void* ExecuteFill(void* args_ptr);
    static LevelDBData* PackValue(char* value);
    static void UpdateStatus(std::string status);

 private:
    void GetStatus(std::string& value);
    void GetResults(std::string& value, unsigned int id);
    void RunTest(std::string& test_name, std::string& value);
    void FillSelf(std::string& value);
    void AbortTest(std::string& value);
    unsigned int GetID();
    std::string GetFileName(unsigned int& id);
    void StripPrefix(std::string& str, std::string prefix);

    static const size_t kKEY_SIZE = 32;
    static const size_t kVALUE_SIZE = 1048576;
    static const std::string kQUAL_PATTERN;
    static const std::string kTEST_PATTERN;
    static const std::string kRESULTS_PATTERN;
    static const std::string kGET_STATUS;
    static const std::string kABORT_TEST;
    static const std::string kATI_STE_TEST;
    static const std::string kBUTTERFLY_TEST;
    static const std::string kID_OD_TEST;
    static const std::string kRDT_TEST;
    static const std::string kSEQUENTIAL_WRITE_TEST;
    static const std::string kFILL_SELF;
    static const std::string kRESULTS_NAME;
    static const std::string kSTATUS_FILENAME;
    static const std::string kOUTPUT_PATH;

    KeyValueStoreInterface& primary_data_store_;
    DeviceInformationInterface& device_information_;
    Server* server_;
    pthread_t thread_;
    unsigned int id_;
    bool test_running_;
    struct ThreadArgs thread_args_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_QUALIFICATION_HANDLER_H_
