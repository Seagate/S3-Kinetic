#include <unistd.h>

#include "gtest/gtest.h"

#include "outgoing_value.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::OutgoingStringValue;

TEST(NullableOutgoingValueTest, NullValueHasZeroSize) {
    NullableOutgoingValue value;
    ASSERT_EQ(0u, value.size());
}

TEST(NullableOutgoingValueTest, TransferToSocketSendsZeroBytesForNullValue) {
    // Create a new value and write it into a pipe
    NullableOutgoingValue value;
    int pipe_fds[2];
    int err;
    ASSERT_EQ(0, pipe(pipe_fds));
    ASSERT_TRUE(value.TransferToSocket(pipe_fds[1], &err));
    ASSERT_EQ(0, close(pipe_fds[1]));

    // Verify that we immediately see end-of-file when we read from the pipe
    char c;
    ASSERT_EQ(0, read(pipe_fds[0], &c, sizeof(c)));
    ASSERT_EQ(0, close(pipe_fds[0]));
}

TEST(NullableOutgoingValueTest, ToStringReturnsEmptyForNullValue) {
    NullableOutgoingValue value;
    std::string s("s");
    int err;
    ASSERT_TRUE(value.ToString(&s, &err));
    ASSERT_EQ("", s);
}

TEST(NullableOutgoingValueTest, SizeDelegatesToWrappedPointer) {
    NullableOutgoingValue value;
    OutgoingStringValue *string_value = new OutgoingStringValue("abc");
    value.set_value(string_value);
    ASSERT_EQ(3u, value.size());
}

TEST(NullableOutgoingValueTest, TransferToSocketDelegatesToWrappedPointer) {
    NullableOutgoingValue value;
    OutgoingStringValue *string_value = new OutgoingStringValue("abc");
    value.set_value(string_value);

    // Write the value into a pipe
    int pipe_fds[2];
    int err;
    ASSERT_EQ(0, pipe(pipe_fds));
    ASSERT_TRUE(value.TransferToSocket(pipe_fds[1], &err));
    ASSERT_EQ(0, close(pipe_fds[1]));

    // Verify that the pipe contains the string "abc"
    char abc[3];
    ASSERT_EQ(sizeof(abc), static_cast<size_t>(read(pipe_fds[0], abc, sizeof(abc))));
    ASSERT_EQ('a', abc[0]);
    ASSERT_EQ('b', abc[1]);
    ASSERT_EQ('c', abc[2]);
    ASSERT_EQ(0, close(pipe_fds[0]));
}

TEST(NullableOutgoingValueTest, ToStringDelegatesToWrappedPointer) {
    NullableOutgoingValue value;
    OutgoingStringValue *string_value = new OutgoingStringValue("abc");
    value.set_value(string_value);

    std::string s;
    int err;
    ASSERT_TRUE(value.ToString(&s, &err));
    ASSERT_EQ("abc", s);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
