#include <vector>

#include "gtest/gtest.h"

#include "domain.h"
#include "mock_authorizer.h"
#include "device_information.h"
#include "command_line_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::testing::Return;
using ::testing::_;

class DeviceInformationTest : public ::testing::Test {
    protected:
    DeviceInformationTest()
        : authorizer_(), device_information_(
            authorizer_, "database.db", FLAGS_proc_stat_path,
            FLAGS_store_test_device.substr(FLAGS_store_device.find_last_of('/') + 1), "temperature-path",
            FLAGS_preused_file_path, FLAGS_kineticd_start_log, 8000000000000) {}

    MockAuthorizer authorizer_;
    DeviceInformation device_information_;
};

TEST_F(DeviceInformationTest, AuthorizeReturnsFalseIfUnderlyingAuthorizationFails) {
    RequestContext request_context;
    EXPECT_CALL(authorizer_, AuthorizeGlobal(2, Domain::kGetLog, _))
        .WillOnce(Return(false));
    EXPECT_FALSE(device_information_.Authorize(2, request_context));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
