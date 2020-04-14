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
            authorizer_, "database.db", "/proc/stat", "sda", "temperature-path",
            "fsize", FLAGS_kineticd_start_log) {}

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
