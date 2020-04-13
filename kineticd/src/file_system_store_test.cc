#include "gtest/gtest.h"

#include <stdlib.h>

#include "kinetic/incoming_value.h"
#include "kinetic/mock_incoming_value.h"

#include "file_system_store.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::MockIncomingValue;
using ::kinetic::IncomingStringValue;
using ::testing::_;
using ::testing::Return;

class FileSystemStoreTest : public ::testing::Test {
    protected:
    FileSystemStoreTest(): file_system_store_("test_file_system_store") {}

    virtual void SetUp() {
        ASSERT_TRUE(file_system_store_.Init(true));
    }

    virtual void TearDown() {
        RemoveTestStoreDirectory();
    }

    virtual void AssertEqual(const std::string &s, const NullableOutgoingValue &value) {
        std::string value_string;
        int err;
        ASSERT_TRUE(value.ToString(&value_string, &err));
        ASSERT_EQ(s, value_string);
    }

    void RemoveTestStoreDirectory() {
        ASSERT_NE(-1, system("rm -rf test_file_system_store"));
    }

    FileSystemStore file_system_store_;
};

TEST_F(FileSystemStoreTest, OpeningAnExistingFileSystemStoreWorks) {
    FileSystemStore new_store("test_file_system_store");
    ASSERT_TRUE(new_store.Init(true));
}

TEST_F(FileSystemStoreTest, GetOfNonexistentKeyFails) {
    NullableOutgoingValue value;
    ASSERT_FALSE(file_system_store_.Get("nonexistent", &value));
}

TEST_F(FileSystemStoreTest, GetOfExistingKeySucceeds) {
    IncomingStringValue string_value("value");
    ASSERT_TRUE(file_system_store_.Put("key", &string_value, false));
    NullableOutgoingValue value;
    ASSERT_TRUE(file_system_store_.Get("key", &value));
    AssertEqual("value", value);
}

TEST_F(FileSystemStoreTest, InitialPutsHaveFailureAtomicity) {
    MockIncomingValue value;
    EXPECT_CALL(value, TransferToFile(_)).WillOnce(Return(false));
    ASSERT_FALSE(file_system_store_.Put("key", &value, false));

    // Since the Put failed, a Get should also fail
    NullableOutgoingValue outgoing_value;
    ASSERT_FALSE(file_system_store_.Get("key", &outgoing_value));
}

TEST_F(FileSystemStoreTest, UpdatesHaveFailureAtomicity) {
    IncomingStringValue original_value("original value");
    ASSERT_TRUE(file_system_store_.Put("key", &original_value, false));

    MockIncomingValue new_value;
    EXPECT_CALL(new_value, TransferToFile(_)).WillOnce(Return(false));
    ASSERT_FALSE(file_system_store_.Put("key", &new_value, false));

    // Since the Put failed, the original value should still be present
    NullableOutgoingValue value;
    ASSERT_TRUE(file_system_store_.Get("key", &value));
    AssertEqual("original value", value);
}

TEST_F(FileSystemStoreTest, RepeatedPutsAreEffective) {
    IncomingStringValue previous_value("previous_value");
    ASSERT_TRUE(file_system_store_.Put("key", &previous_value, false));
    IncomingStringValue new_value("new_value");
    ASSERT_TRUE(file_system_store_.Put("key", &new_value, false));
    NullableOutgoingValue actual_value;
    ASSERT_TRUE(file_system_store_.Get("key", &actual_value));
    AssertEqual("new_value", actual_value);
}

TEST_F(FileSystemStoreTest, DeleteOfNonexistentKeyFails) {
    ASSERT_FALSE(file_system_store_.Delete("nonexistent"));
}

TEST_F(FileSystemStoreTest, DeleteOfExistingKeySucceeds) {
    IncomingStringValue previous_value("previous_value");
    ASSERT_TRUE(file_system_store_.Put("key", &previous_value, false));
    ASSERT_TRUE(file_system_store_.Delete("key"));
    NullableOutgoingValue value;
    ASSERT_FALSE(file_system_store_.Get("key", &value));
}

TEST_F(FileSystemStoreTest, InitCreatesDirectoryIfDirectoryMissing) {
    RemoveTestStoreDirectory();

    ASSERT_TRUE(file_system_store_.Init(true));

    IncomingStringValue string_value1("value1");
    ASSERT_TRUE(file_system_store_.Put("key1", &string_value1, false));

    NullableOutgoingValue value;
    ASSERT_TRUE(file_system_store_.Get("key1", &value));
}

TEST_F(FileSystemStoreTest, InitWorksIfDirectoryExists) {
    ASSERT_TRUE(file_system_store_.Init(true));

    IncomingStringValue string_value1("value1");
    ASSERT_TRUE(file_system_store_.Put("key1", &string_value1, false));

    NullableOutgoingValue value;
    ASSERT_TRUE(file_system_store_.Get("key1", &value));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
