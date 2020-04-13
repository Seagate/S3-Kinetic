#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "gtest/gtest.h"
#include "kinetic/incoming_value.h"
#include "kinetic/outgoing_value.h"

#include "outgoing_value.h"

namespace com {
namespace seagate {
namespace kinetic {

static const std::string kFileName = "test_file_value";
static const std::string kValue = "abc";

class OutgoingFileValueTest : public ::testing::Test {
    protected:
    virtual void SetUp() {
        // Create a temporary file and fill it with a short string
        int fd = open(kFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        ASSERT_NE(-1, fd);
        ASSERT_EQ(kValue.size(), static_cast<size_t>(write(fd, kValue.data(), kValue.size())));
        ASSERT_EQ(0, close(fd));

        // Provide the tests with a file descriptor suitable for reading
        fd_ = open(kFileName.c_str(), O_RDONLY);
    }

    virtual void TearDown() {
        ASSERT_EQ(0, unlink(kFileName.c_str()));
    }

    int fd_;
};

TEST_F(OutgoingFileValueTest, SizeWorks) {
    OutgoingFileValue value(fd_);
    ASSERT_EQ(kValue.size(), value.size());
}

TEST_F(OutgoingFileValueTest, TransferToSocketWorks) {
    // Create a pipe and transfer the value to it
    OutgoingFileValue value(fd_);
    int output_pipe[2];
    int err;
    ASSERT_EQ(0, pipe(output_pipe));
    ASSERT_TRUE(value.TransferToSocket(output_pipe[1], &err));

    // Verify that the pipe now contains the original string
    char *raw_result = new char[kValue.size()];
    ASSERT_EQ(kValue.size(), static_cast<size_t>(read(output_pipe[0], raw_result, kValue.size())));
    std::string result(raw_result, kValue.size());
    ASSERT_EQ(kValue, result);
    delete[] raw_result;

    ASSERT_EQ(0, close(output_pipe[0]));
    ASSERT_EQ(0, close(output_pipe[1]));
}

TEST_F(OutgoingFileValueTest, ToStringWorks) {
    OutgoingFileValue value(fd_);
    std::string s;
    int err;
    ASSERT_TRUE(value.ToString(&s, &err));
    ASSERT_EQ(kValue, s);
}

TEST_F(OutgoingFileValueTest, TransferToSocketWorksRepeatedly) {
    // Create a pipe and transfer the value to it
    OutgoingFileValue value(fd_);
    int output_pipe[2];
    int err;
    ASSERT_EQ(0, pipe(output_pipe));
    ASSERT_TRUE(value.TransferToSocket(output_pipe[1], &err));

    // Read the result out of the pipe
    char *raw_result = new char[kValue.size()];
    ASSERT_EQ(kValue.size(), static_cast<size_t>(read(output_pipe[0], raw_result, kValue.size())));

    // Transfer the value a second time and verify that it worked
    ASSERT_TRUE(value.TransferToSocket(output_pipe[1], &err));
    ASSERT_EQ(kValue.size(), static_cast<size_t>(read(output_pipe[0], raw_result, kValue.size())));
    std::string result(raw_result, kValue.size());
    ASSERT_EQ(kValue, result);

    delete[] raw_result;

    ASSERT_EQ(0, close(output_pipe[0]));
    ASSERT_EQ(0, close(output_pipe[1]));
}

TEST_F(OutgoingFileValueTest, ToStringWorksRepeatedly) {
    OutgoingFileValue value(fd_);
    std::string s1, s2;
    int err;
    ASSERT_TRUE(value.ToString(&s1, &err));
    ASSERT_TRUE(value.ToString(&s2, &err));
    ASSERT_EQ(kValue, s2);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
