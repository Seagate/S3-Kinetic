#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "spliceable_value.h"

namespace com {
namespace seagate {
namespace kinetic {

class SpliceableValueTest : public ::testing::Test {
    protected:
    // Create a pipe and fill it with the given string
    void MakeInputPipe(const std::string &s, int *fd) {
        int input_pipe[2];
        ASSERT_EQ(0, pipe(input_pipe));
        ASSERT_EQ(static_cast<int>(s.size()), write(input_pipe[1], s.data(), s.size()));
        ASSERT_EQ(0, close(input_pipe[1]));
        *fd = input_pipe[0];
    }

    // Create a temporary file and return a file descriptor pointing to it
    void MakeTemporaryFile(int *fd) {
        char filename[] = "./test_XXXXXX";
        int output_fd = mkstemp(filename);
        ASSERT_NE(-1, output_fd);
        ASSERT_EQ(0, unlink(filename));
        *fd = output_fd;
    }
};

TEST_F(SpliceableValueTest, TransferToFileWorks) {
    int input_fd;
    MakeInputPipe("ab", &input_fd);
    SpliceableValue value(input_fd, 1);

    int output_fd;
    MakeTemporaryFile(&output_fd);

    // Transfer one byte from the pipe to the file
    ASSERT_TRUE(value.TransferToFile(output_fd));

    // The file should now contain a single byte 'a'
    ASSERT_EQ(0, lseek(output_fd, 0, SEEK_SET));
    char a;
    ASSERT_EQ(1, read(output_fd, &a, 1));
    ASSERT_EQ('a', a);
    ASSERT_EQ(0, read(output_fd, &a, 1));   // we should hit EOF
    ASSERT_EQ(0, close(output_fd));

    // The pipe should now contain just the byte 'b'
    char b;
    ASSERT_EQ(1, read(input_fd, &b, 1));
    ASSERT_EQ('b', b);
    ASSERT_EQ(0, read(input_fd, &b, 1));   // we should hit EOF
    ASSERT_EQ(0, close(input_fd));
}

TEST_F(SpliceableValueTest, ToStringWorks) {
    int input_fd;
    MakeInputPipe("ab", &input_fd);
    SpliceableValue value(input_fd, 1);

    // Call ToString() and check that we get just "a"
    std::string s;
    ASSERT_TRUE(value.ToString(&s));
    ASSERT_EQ(1u, s.size());
    ASSERT_EQ("a", s);

    // The pipe should now contain just the byte 'b'
    char b;
    ASSERT_EQ(1, read(input_fd, &b, 1));
    ASSERT_EQ('b', b);
    ASSERT_EQ(0, read(input_fd, &b, 1));   // we should hit EOF
    ASSERT_EQ(0, close(input_fd));
}

TEST_F(SpliceableValueTest, ConsumeWorks) {
    int input_fd;
    MakeInputPipe("ab", &input_fd);
    SpliceableValue value(input_fd, 1);

    // Call Consume() and check that the byte 'a' disappears from the pipe
    value.Consume();
    char b;
    ASSERT_EQ(1, read(input_fd, &b, 1));
    ASSERT_EQ('b', b);
    ASSERT_EQ(0, read(input_fd, &b, 1));   // we should hit EOF
    ASSERT_EQ(0, close(input_fd));
}

TEST_F(SpliceableValueTest, TransferToFileDepletesObject) {
    int input_fd;
    MakeInputPipe("ab", &input_fd);
    SpliceableValue value(input_fd, 1);

    int output_fd;
    MakeTemporaryFile(&output_fd);

    ASSERT_TRUE(value.TransferToFile(output_fd));
    ASSERT_FALSE(value.TransferToFile(output_fd));

    ASSERT_EQ(0, close(input_fd));
    ASSERT_EQ(0, close(output_fd));
}

TEST_F(SpliceableValueTest, ToStringDepletesObject) {
    int input_fd;
    MakeInputPipe("ab", &input_fd);
    SpliceableValue value(input_fd, 1);

    std::string s;
    ASSERT_TRUE(value.ToString(&s));
    ASSERT_FALSE(value.ToString(&s));

    ASSERT_EQ(0, close(input_fd));
}

TEST_F(SpliceableValueTest, ConsumeDepletesObject) {
    int input_fd;
    MakeInputPipe("ab", &input_fd);
    SpliceableValue value(input_fd, 1);

    value.Consume();
    std::string s;
    ASSERT_FALSE(value.ToString(&s));

    ASSERT_EQ(0, close(input_fd));
}

TEST_F(SpliceableValueTest, TransferToFileFailsOnUnexpectedEof) {
    int input_fd;
    MakeInputPipe("a", &input_fd);
    // Create SpliceableValue with size greater than what's actually available
    SpliceableValue value(input_fd, 2);

    int output_fd;
    MakeTemporaryFile(&output_fd);
    ASSERT_FALSE(value.TransferToFile(output_fd));
}

TEST_F(SpliceableValueTest, ToStringFailsOnUnexpectedEof) {
    int input_fd;
    MakeInputPipe("a", &input_fd);
    // Create SpliceableValue with size greater than what's actually available
    SpliceableValue value(input_fd, 2);

    std::string s;
    ASSERT_FALSE(value.ToString(&s));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
