#ifndef KINETIC_QUAL_INCLUDES_BASE_TEST_H_
#define KINETIC_QUAL_INCLUDES_BASE_TEST_H_
#include <vector>
#include "zac_mediator.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

// Base class for qualification tests that provides the interface for running tests as well as implements some helper
// functions that all tests need (updating status, writing results, etc.).
//
// All test classes inheret from this class and must implement the RunTest() method.
class BaseTest {
 public:
    explicit BaseTest(int id);
    virtual ~BaseTest();

    // Interface for running tests which allows QualificationHandler to operate on an abstract interface class.
    virtual bool RunTest(double target_percent = 100)=0;

    // Runs the baseline test of sequential write and read of entire disk. Returns true if the test passes.
    bool RunBaselineTest();

 protected:
    // Updates the test status (percent complete) which can be queried through a get_log([7]).
    void UpdateStatus();

    // Formats string and writes to the results file which can be queried through a get_log([7]).
    void WriteResults(std::string msg);

    // Runs a sequential fill on the drive. Returns true if fill is successful.
    bool SequentialFill(size_t data_size, void* data, double target_percent);

    // Runs a sequential read of the drive. Returns true if the reads are successful.
    bool SequentialRead(size_t data_size, void* data, double target_percent);

    // Writes the qualification logs (smart read data) to the results file. Returns query/write are successful.
    bool GetLogs();

    // Resets write pointer for all zones and updates write pointer array.
    bool ResetAllZones();

    // Returns the write pointer for the specified zone.
    uint64_t GetWritePtr(uint32_t& zone_id);

    // Updates the write pointer for the specified zone.
    void UpdateWritePtr(uint32_t& zone_id, size_t offset);

    // Constant that determines how many write/read/verify operations should be done continuously before checking
    // time. This prevents checks on runtime happening after every single operation and should improve performance.
    static const uint32_t kOPS_PER_ANALYZE = 5000;

    int id_;
    int zac_fd_;
    int read_fd_;
    double percent_complete_;
    uint8_t* data_buffer_;
    size_t data_size_;
    ZacMediator zac_kin_;
    AtaCmdHandler zac_ata_;
    std::vector<uint32_t> write_ptrs_;

 private:
    std::string GetResultsName();
    void FormatMessage(std::string& msg);

    static const std::string kSMART_CMD;
    static const std::string kOUTPUT_PATH;
    static const std::string kRESULTS_NAME;
    static const std::string kSTATUS_FILENAME;
};

}  // namespace qual_kin

#endif  // KINETIC_QUAL_INCLUDES_BASE_TEST_H_
