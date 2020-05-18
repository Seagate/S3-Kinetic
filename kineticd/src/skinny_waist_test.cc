#include <stdlib.h>

#include "gtest/gtest.h"
#include "kinetic/incoming_value.h"

#include "file_system_store.h"
#include "key_value_store_interface.h"
#include "key_value_store.h"
#include "mock_authorizer.h"
#include "mock_device_information.h"
#include "mock_primary_store.h"
#include "mock_user_store.h"
#include "primary_store.h"
#include "profiler.h"
#include "skinny_waist.h"
#include "std_map_key_value_store.h"
#include "user_store.h"
#include "typed_test_helpers.h"
#include "user.h"
#include "user_store_interface.h"
#include "mock_cluster_version_store.h"
#include "limits.h"
#include "launch_monitor.h"
#include "connection_time_handler.h"
#include "command_line_flags.h"
#include "instant_secure_eraser.h"
#include "smrdisk/Disk.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingStringValue;
using ::kinetic::IncomingBuffValue;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::NiceMock;
using ::std::string;

template <class T>
class SkinnyWaistTest : public testing::Test {
    protected:
    SkinnyWaistTest():
        limits_(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000),
        user_store_(std::move(unique_ptr<CautiousFileHandlerInterface>(
                new BlackholeCautiousFileHandler())), limits_),
        authorizer_(),
        profiler_(),
        launch_monitor_(),
        file_system_store_("test_file_system_store"),
        key_value_store_(CreateKeyValueStore<T>()),
        device_information_(),
        instant_secure_eraser_(),
        primary_store_(file_system_store_, *key_value_store_, mock_cluster_version_store_,
            device_information_, profiler_, 32 * 1024, instant_secure_eraser_,
            FLAGS_preused_file_path),
        skinny_waist_("primary.db",
                FLAGS_store_partition,
                FLAGS_store_mountpoint,
                FLAGS_metadata_partition,
                FLAGS_metadata_mountpoint,
                authorizer_,
                user_store_,
                primary_store_,
                profiler_,
            mock_cluster_version_store_,
            launch_monitor_) {}

    virtual ~SkinnyWaistTest() {
        EXPECT_CALL(send_pending_status_sender_, SendAllPending(_, _));
        key_value_store_->Close();
        delete key_value_store_;
    }

    virtual void SetUp() {
        smr::Disk::initializeSuperBlockAddr(FLAGS_store_test_partition);
        InstantSecureEraserX86::ClearSuperblocks(FLAGS_store_test_partition);
        ASSERT_TRUE(key_value_store_->Init(true));
        ASSERT_TRUE(file_system_store_.Init(true));
        key_value_store_->SetListOwnerReference(&send_pending_status_sender_);

        User test_user(0, "super secret", std::list<Domain>());
        std::list<User> users;
        users.push_back(test_user);
        ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));


        // For testing purposes, let's authorize user 0 to do anything. When we
        // test authorization specifically, we'll use other user ids.
        EXPECT_CALL(authorizer_, AuthorizeKey(0, _, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(true));
        EXPECT_CALL(authorizer_, AuthorizeGlobal(0, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(true));

        // Simulate a file system with plenty of free space
        EXPECT_CALL(device_information_, GetCapacity(_, _)).WillRepeatedly(DoAll(
            SetArgPointee<0>(4000000000000L),
            SetArgPointee<1>(1000000000000L),
            Return(true)));
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_file_system_store"));
        ASSERT_NE(-1, system("rm -rf fsize"));
    }

    virtual void AssertEqual(const std::string &s, const NullableOutgoingValue &value) {
        std::string value_string;
        int err;
        ASSERT_TRUE(value.ToString(&value_string, &err));
        ASSERT_EQ(s, value_string);
    }

    Limits limits_;
    UserStore user_store_;
    MockAuthorizer authorizer_;
    Profiler profiler_;
    LaunchMonitorPassthrough launch_monitor_;
    FileSystemStore file_system_store_;
    KeyValueStoreInterface* key_value_store_;
    MockDeviceInformation device_information_;
    MockInstantSecureEraser instant_secure_eraser_;
    PrimaryStore primary_store_;
    NiceMock<MockClusterVersionStore> mock_cluster_version_store_;
    SkinnyWaist skinny_waist_;
    MockSendPendingStatusInterface send_pending_status_sender_;
};

using testing::Types;

typedef Types<KeyValueStore> Implementations;


TYPED_TEST_CASE(SkinnyWaistTest, Implementations);

TYPED_TEST(SkinnyWaistTest, GetNextReturnsNextValue) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version_a";
    primary_store_value.tag = "tag_a";
    primary_store_value.algorithm = 1;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_a, "value_a", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &value_a, false, token);

    primary_store_value.version = "version_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = 2;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_b, "value_b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    this->primary_store_.Put("b", primary_store_value, &value_b, false, token);

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetNext(0, "a", &actual_key, &actual_value, &value, request_context));
    ASSERT_EQ("b", actual_key);
    ASSERT_EQ("version_b", actual_value.version);
    this->AssertEqual("value_b", value);
    ASSERT_EQ("tag_b", actual_value.tag);
    ASSERT_EQ(2, actual_value.algorithm);
}

TYPED_TEST(SkinnyWaistTest, GetNextReturnsNextValueIfNoExactMatch) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = 2;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_b, "value_b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("b", primary_store_value, &value_b, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetNext(0, "a", &actual_key, &actual_value, &value, request_context));
    ASSERT_EQ("b", actual_key);
    ASSERT_EQ("version_b", actual_value.version);
    this->AssertEqual("value_b", value);
    ASSERT_EQ("tag_b", actual_value.tag);
    ASSERT_EQ(2, actual_value.algorithm);
}

TYPED_TEST(SkinnyWaistTest, GetNextReturnsNotFoundIfEmptyDb) {
    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;
    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetNext(0, "a", &actual_key, &actual_value, &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetNextReturnsNotFoundIfLastKey) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;
    RequestContext request_context;

    ASSERT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetNext(0, "a", &actual_key, &actual_value, &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetNextReturnsNotFoundIfNoneFollowing) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetNext(0, "b", &actual_key, &actual_value, &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetPreviousReturnsPrevValue) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version_a";
    primary_store_value.tag = "tag_a";
    primary_store_value.algorithm = 1;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_a, "value_a", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &value_a, false, (token));

    primary_store_value.version = "version_b";
    primary_store_value.tag = "tag_b";
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_b, "value_b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    primary_store_value.algorithm = 2;
    this->primary_store_.Put("b", primary_store_value, &value_b, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetPrevious(0, "b", &actual_key, &actual_value,
            &value, request_context));
    ASSERT_EQ("a", actual_key);
    ASSERT_EQ("version_a", actual_value.version);
    this->AssertEqual("value_a", value);
    ASSERT_EQ("tag_a", actual_value.tag);
    ASSERT_EQ(1, actual_value.algorithm);
}

TYPED_TEST(SkinnyWaistTest, GetPreviousReturnsPrevValueIfNoExactMatch) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = 2;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_b, "value_b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("b", primary_store_value, &value_b, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetPrevious(0, "c", &actual_key, &actual_value,
                &value, request_context));
    ASSERT_EQ("b", actual_key);
    ASSERT_EQ("version_b", actual_value.version);
    this->AssertEqual("value_b", value);
    ASSERT_EQ("tag_b", actual_value.tag);
    ASSERT_EQ(2, actual_value.algorithm);
}

TYPED_TEST(SkinnyWaistTest, GetPreviousReturnsNotFoundIfEmptyDb) {
    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetPrevious(0, "a", &actual_key, &actual_value,
            &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetPreviousReturnsNotFoundIfFirstKey) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetPrevious(0, "a", &actual_key, &actual_value,
                    &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetPreviousReturnsNotFoundIfNoneBefore) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("b", primary_store_value, &string_value, false, (token));

    std::string actual_key;
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetPrevious(0, "a", &actual_key, &actual_value,
                    &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsEmptyListIfEmptyStore) {
    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", true, "z", true, 100, false,
                &keys, request_context));

    ASSERT_TRUE(keys.empty());
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsEmptyListIfNoResults) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value, false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "q", true, "z", true, 100, false,
                &keys, request_context));

    ASSERT_TRUE(keys.empty());
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForNonInclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", false, "d", false, 100, false,
                    &keys, request_context));

    ASSERT_EQ(2UL, keys.size());
    ASSERT_EQ("b", keys[0]);
    ASSERT_EQ("c", keys[1]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForInclusiveStart) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", true, "d", false, 100, false,
            &keys, request_context));

    ASSERT_EQ(3UL, keys.size());
    ASSERT_EQ("a", keys[0]);
    ASSERT_EQ("b", keys[1]);
    ASSERT_EQ("c", keys[2]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForInclusiveEnd) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", false, "d", true, 100, false,
                &keys, request_context));

    ASSERT_EQ(3UL, keys.size());
    ASSERT_EQ("b", keys[0]);
    ASSERT_EQ("c", keys[1]);
    ASSERT_EQ("d", keys[2]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForInclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", true, "d", true, 100, false,
                &keys, request_context));

    ASSERT_EQ(4UL, keys.size());
    ASSERT_EQ("a", keys[0]);
    ASSERT_EQ("b", keys[1]);
    ASSERT_EQ("c", keys[2]);
    ASSERT_EQ("d", keys[3]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultIfDBEmpty) {
    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", true, "d", true, 100, false,
                    &keys, request_context));

    ASSERT_TRUE(keys.empty());
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForReverse) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", true, "d", true, 2, true,
                    &keys, request_context));

    ASSERT_EQ(2UL, keys.size());
    EXPECT_EQ("d", keys[0]);
    EXPECT_EQ("c", keys[1]);
}


TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForReverseNonInclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
            this->skinny_waist_.GetKeyRange(0, "a", false, "d", false, 2, true,
                        &keys, request_context));

    ASSERT_EQ(2UL, keys.size());
    EXPECT_EQ("c", keys[0]);
    EXPECT_EQ("b", keys[1]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForReverseStartInclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
            this->skinny_waist_.GetKeyRange(0, "a", true, "d", false, 3, true,
                    &keys, request_context));

    ASSERT_EQ(3UL, keys.size());
    EXPECT_EQ("c", keys[0]);
    EXPECT_EQ("b", keys[1]);
    EXPECT_EQ("a", keys[2]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReturnsCorrectResultForReverseEndInclusive) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
                this->skinny_waist_.GetKeyRange(0, "a", false, "d", true, 2, true,
                        &keys, request_context));

    ASSERT_EQ(2UL, keys.size());
    EXPECT_EQ("d", keys[0]);
    EXPECT_EQ("c", keys[1]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeHonorsLimit) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.GetKeyRange(0, "a", true, "d", true, 2, false,
                &keys, request_context));

    ASSERT_EQ(2UL, keys.size());
    ASSERT_EQ("a", keys[0]);
    ASSERT_EQ("b", keys[1]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReverseIfEndKeyNotFound) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
            this->skinny_waist_.GetKeyRange(0, "c", true, "z", false, 10, true,
                    &keys, request_context));

    ASSERT_EQ(1UL, keys.size());
    ASSERT_EQ("c", keys[0]);
}


TYPED_TEST(SkinnyWaistTest, GetKeyRangeReverseIfStartEndKeyNotFound) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
            this->skinny_waist_.GetKeyRange(0, "a", true, "z", false, 10, true,
                        &keys, request_context));

    ASSERT_EQ(2UL, keys.size());
    ASSERT_EQ("c", keys[0]);
    ASSERT_EQ("b", keys[1]);
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeReverseIfNoKeysInRange) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
            this->skinny_waist_.GetKeyRange(0, "v", true, "z", false, 10, true,
                    &keys, request_context));

    ASSERT_EQ(0UL, keys.size());
}

TYPED_TEST(SkinnyWaistTest, GetKeyRangeIfNoKeysInRange) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));

    std::vector<std::string> keys;

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
                this->skinny_waist_.GetKeyRange(0, "v", true, "z", false, 10, false,
                        &keys, request_context));

    ASSERT_EQ(0UL, keys.size());
}

TYPED_TEST(SkinnyWaistTest, MediaScanHonorsLimit) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    primary_store_value.tag = "garbage";
    IncomingStringValue string_value_a("");
    IncomingStringValue string_value_b("");
    IncomingStringValue string_value_c("");
    IncomingStringValue string_value_d("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("a", primary_store_value, &string_value_a,
                             false, (token));
    this->primary_store_.Put("b", primary_store_value, &string_value_b,
                             false, (token));
    this->primary_store_.Put("c", primary_store_value, &string_value_c,
                             false, (token));
    this->primary_store_.Put("d", primary_store_value, &string_value_d,
                             false, (token));

    std::vector<std::string> keys;
    std::string start_key_contain;
    RequestContext request_context;
    ConnectionTimeHandler timer;
    timer.SetTimeQueued(std::chrono::high_resolution_clock::now());
    timer.SetTimeOpStart();
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.
            MediaScan(0,
                      "a",
                      &start_key_contain,
                      true,
                      "d",
                      true,
                      2,
                      &keys,
                      request_context,
                      &timer));

    ASSERT_EQ(2UL, keys.size());
    ASSERT_EQ("a", keys[0]);
    ASSERT_EQ("b", keys[1]);
}


TYPED_TEST(SkinnyWaistTest, InstantSecureEraseReportsSuccessButStoreIsNotClear) {
    EXPECT_CALL(this->send_pending_status_sender_, SendAllPending(_, _));
    EXPECT_CALL(this->instant_secure_eraser_, Erase("")).WillOnce(Return(PinStatus::PIN_SUCCESS));
    EXPECT_CALL(this->instant_secure_eraser_, MountCreateFileSystem(_)).WillOnce(Return(true));

    ASSERT_EQ(StoreOperationStatus_ISE_FAILED_VAILD_DB,
                this->skinny_waist_.InstantSecureErase(""));
}

TYPED_TEST(SkinnyWaistTest, InstantSecureEraseReportsSuccessButMetadataIsNotClear) {
    EXPECT_CALL(this->send_pending_status_sender_, SendAllPending(_, _));
    EXPECT_CALL(this->instant_secure_eraser_, Erase("")).WillOnce(Return(PinStatus::PIN_SUCCESS));
    EXPECT_CALL(this->instant_secure_eraser_, MountCreateFileSystem(_)).WillOnce(Return(true));

    User user(17, "super secret", std::list<Domain>());
    std::list<User> users;
    users.push_back(user);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, this->user_store_.Put(users));
    ASSERT_EQ(StoreOperationStatus_ISE_FAILED_VAILD_DB,
                this->skinny_waist_.InstantSecureErase(""));
    ASSERT_TRUE(this->user_store_.Get(17, &user));
}

TYPED_TEST(SkinnyWaistTest, InstantSecureEraseReportsSuccessButLeavesStoreInUsableState) {
    EXPECT_CALL(this->send_pending_status_sender_, SendAllPending(_, _));
    EXPECT_CALL(this->instant_secure_eraser_, Erase("")).WillOnce(Return(PinStatus::PIN_SUCCESS));
    EXPECT_CALL(this->instant_secure_eraser_, MountCreateFileSystem(_)).WillOnce(Return(true));
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value_a("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("old", primary_store_value, &string_value_a,
                             false, (token));

    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_ISE_FAILED_VAILD_DB,
            this->skinny_waist_.InstantSecureErase(""));

    IncomingStringValue string_value_b("");
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Put(0, "new", "", primary_store_value,
                &string_value_b, false, false, request_context, (token)));

    NullableOutgoingValue value;
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Get(0, "new", &primary_store_value, request_context, &value, NULL));
}

// Authorization tests

TYPED_TEST(SkinnyWaistTest, GetAuthorizesUser) {
    PrimaryStoreValue primary_store_value;
    IncomingStringValue string_value("");
    EXPECT_CALL(this->authorizer_, AuthorizeKey(2, Domain::kRead, "000", _))
        .WillOnce(Return(false));
    NullableOutgoingValue value;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_AUTHORIZATION_FAILURE,
        this->skinny_waist_.Get(2, "000", &primary_store_value, request_context, &value, NULL));
}

TYPED_TEST(SkinnyWaistTest, GetVersionAuthorizesUser) {
    std::string version;
    EXPECT_CALL(this->authorizer_, AuthorizeKey(2, Domain::kRead, "001", _))
        .WillOnce(Return(false));
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_AUTHORIZATION_FAILURE,
        this->skinny_waist_.GetVersion(2, "001", &version, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetNextAuthorizesUser) {
    // Insert a key-value pair with key "003" and try to access it via GetNext()
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->primary_store_.Put("003", primary_store_value, &string_value,
                                 false, (token)));

    EXPECT_CALL(this->authorizer_, AuthorizeKey(2, Domain::kRead, "003", _))
        .WillOnce(Return(false));
    std::string actual_key;
    NullableOutgoingValue value;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetNext(2, "002", &actual_key, &primary_store_value,
                    &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, GetPreviousAuthorizesUser) {
    // Insert a key-value pair with key "004" and try to access it via GetPrevious()
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->primary_store_.Put("004", primary_store_value, &string_value,
                                 false, (token)));

    EXPECT_CALL(this->authorizer_, AuthorizeKey(2, Domain::kRead, "004", _))
        .WillOnce(Return(false));
    std::string actual_key;
    NullableOutgoingValue value;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.GetPrevious(2, "005", &actual_key, &primary_store_value,
                &value, request_context));
}

TYPED_TEST(SkinnyWaistTest, PutAuthorizesUser) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    EXPECT_CALL(this->authorizer_, AuthorizeKey(2, Domain::kWrite, "006", _))
        .WillOnce(Return(false));
    IncomingStringValue string_value("");
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_AUTHORIZATION_FAILURE,
        this->skinny_waist_.Put(2, "006", "version",
                                primary_store_value, &string_value, false, false,
                                request_context, (token)));
}

TYPED_TEST(SkinnyWaistTest, DeleteAuthorizesUser) {
    EXPECT_CALL(this->authorizer_, AuthorizeKey(2, Domain::kDelete, "007", _))
        .WillOnce(Return(false));
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_AUTHORIZATION_FAILURE,
        this->skinny_waist_.Delete(2, "007", "version", false,
                                   false, request_context, (token)));
}

TYPED_TEST(SkinnyWaistTest, PutWithWrongVersionReturnsError) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    primary_store_value.version = "oldversion";
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(8);
    memcpy(the_value, "oldvalue", 8);
    IncomingBuffValue value_interface(the_value, 8);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("old", primary_store_value, &value_interface,
                             false, (token));

    primary_store_value.version = "newversion";
    primary_store_value.version = "newvalue";
    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_VERSION_MISMATCH,
        this->skinny_waist_.Put(0, "old", "wrongversion", primary_store_value,
            &value_interface, false, false, request_context, (token)));

    NullableOutgoingValue value;
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Get(0, "old", &primary_store_value, request_context, &value, NULL));
    EXPECT_EQ("oldversion", primary_store_value.version);
    this->AssertEqual("oldvalue", value);
}

TYPED_TEST(SkinnyWaistTest, PutForceWithOldVersionSucceeds) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(8);
    memcpy(the_value, "oldvalue", 8);
    IncomingBuffValue value_interface(the_value, 8);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("old", primary_store_value, &value_interface,
                             false, (token));

    char *new_the_value;
    new_the_value = (char*) smr::DynamicMemory::getInstance()->allocate(8);
    memcpy(new_the_value, "newvalue", 8);
    IncomingBuffValue new_value_interface(new_the_value, 8);
    primary_store_value.version = "newversion";
    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Put(0, "old", "wrongversion", primary_store_value,
            &new_value_interface, true, false, request_context, (token)));

    NullableOutgoingValue value;
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Get(0, "old", &primary_store_value, request_context, &value, NULL));
    EXPECT_EQ("newversion", primary_store_value.version);
    this->AssertEqual("newvalue", value);
}

TYPED_TEST(SkinnyWaistTest, DeleteForceWithOldVersionSucceeds) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "oldversion";
    primary_store_value.algorithm = 1;
    IncomingStringValue the_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_.Put("old", primary_store_value, &the_value, false, (token));

    primary_store_value.version = "newversion";
    RequestContext request_context;
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Delete(0, "old", "wrongversion", true,
                                   false, request_context, (token)));

    NullableOutgoingValue value;
    EXPECT_EQ(StoreOperationStatus_NOT_FOUND,
        this->skinny_waist_.Get(0, "old", &primary_store_value, request_context, &value, NULL));
}

TYPED_TEST(SkinnyWaistTest, DeleteForceForNotFoundSucceeds) {
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
        this->skinny_waist_.Delete(0, "old", "wrongversion", true,
                                   false, request_context, (token)));
}


class SkinnyWaistUnitTest : public testing::Test {
    // This test fixture is for pure unit tests that satisfy dependencies with
    // mocks exclusively.
    protected:
    SkinnyWaistUnitTest():
        authorizer_(),
        user_store_(),
        primary_store_(),
        launch_monitor_(),
        cluster_version_store_(),
        skinny_waist_("primary.db",
                FLAGS_store_partition,
                FLAGS_store_mountpoint,
                FLAGS_metadata_partition,
                FLAGS_metadata_mountpoint,
                authorizer_,
                user_store_,
                primary_store_,
                profiler_,
                cluster_version_store_,
                launch_monitor_) {}

    virtual void SetUp() {
        // For testing purposes, let's authorize user 0 to do anything.
        EXPECT_CALL(authorizer_, AuthorizeKey(0, _, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(true));
        EXPECT_CALL(authorizer_, AuthorizeGlobal(0, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(true));
    }

    MockAuthorizer authorizer_;
    MockUserStore user_store_;
    MockPrimaryStore primary_store_;
    Profiler profiler_;
    LaunchMonitorPassthrough launch_monitor_;
    MockClusterVersionStore cluster_version_store_;
    SkinnyWaist skinny_waist_;
};

TEST_F(SkinnyWaistUnitTest, GetReturnsInternalErrorWhenAppropriate) {
    PrimaryStoreValue primary_store_value;
    EXPECT_CALL(this->primary_store_, Get("key", &primary_store_value, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));

    NullableOutgoingValue value;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.Get(0, "key", &primary_store_value, request_context, &value, NULL));
}

TEST_F(SkinnyWaistUnitTest, GetVersionReturnsInternalErrorWhenAppropriate) {
    std::string version;
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.GetVersion(0, "key", &version, request_context));
}

TEST_F(SkinnyWaistUnitTest, PutReturnsInternalErrorOnFailureToCheckExistenceOfKey) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    IncomingStringValue value("");
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.Put(0, "key", "version", primary_store_value, &value,
            false, false, request_context, token));
}

TEST_F(SkinnyWaistUnitTest, PutReturnsInternalErrorOnFailureToStoreValue) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
        .WillOnce(Return(StoreOperationStatus_SUCCESS));
    EXPECT_CALL(this->primary_store_, Put("key", _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue string_value("");
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.Put(0, "key", "", primary_store_value,
                &string_value, false, false, request_context, token));
}

TEST_F(SkinnyWaistUnitTest, PutReturnsDriveFullWhenAppropriate) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
        .WillOnce(Return(StoreOperationStatus_NOT_FOUND));
    EXPECT_CALL(this->primary_store_, Put("key", _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_NO_SPACE));
    IncomingStringValue string_value("");
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_NO_SPACE, this->skinny_waist_.Put(0,
        "key", "", primary_store_value, &string_value, false, false,
        request_context, token));
}

TEST_F(SkinnyWaistUnitTest, DeleteReturnsInternalErrorOnFailureToCheckExistenceOfKey) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.Delete(0, "key", "version", false,
                                   false, request_context, token));
}

TEST_F(SkinnyWaistUnitTest, DeleteReturnsInternalErrorOnFailureToDeleteValue) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
        .WillOnce(Return(StoreOperationStatus_SUCCESS));
    EXPECT_CALL(this->primary_store_, Delete("key", _, _))
        .WillOnce(Return(StoreOperationStatus_AUTHORIZATION_FAILURE));
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.Delete(0, "key", "", false,
                                   false, request_context, token));
}

TEST_F(SkinnyWaistUnitTest, InstantSecureEraseReturnsInternalErrorWhenClearFails) {
    EXPECT_CALL(this->primary_store_, Clear(""))
        .WillOnce(Return(StoreOperationStatus_INTERNAL_ERROR));
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
            this->skinny_waist_.InstantSecureErase(""));
}

TEST_F(SkinnyWaistUnitTest, InstantSecureEraseReturnsInternalErrorWhenClusterVersionFails) {
    EXPECT_CALL(this->primary_store_, Clear(""))
        .WillOnce(Return(StoreOperationStatus_SUCCESS));
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
              this->skinny_waist_.InstantSecureErase(""));
}

TEST_F(SkinnyWaistUnitTest, InstantSecureEraseReturnsInternalErrorWhenUserStoreFails) {
    EXPECT_CALL(this->primary_store_, Clear(""))
        .WillOnce(Return(StoreOperationStatus_SUCCESS));
    EXPECT_CALL(this->user_store_, Clear())
        .WillOnce(Return(false));
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
                this->skinny_waist_.InstantSecureErase(""));
}

TEST_F(SkinnyWaistUnitTest, InstantSecureEraseReturnsSuccessWhenAllGoesWell) {
    EXPECT_CALL(this->primary_store_, Clear(""))
        .WillOnce(Return(StoreOperationStatus_SUCCESS));
    EXPECT_CALL(this->user_store_, Clear()).WillOnce(Return(true));
    EXPECT_CALL(this->user_store_, CreateDemoUser()).WillOnce(Return(true));
    EXPECT_EQ(StoreOperationStatus_SUCCESS,
              this->skinny_waist_.InstantSecureErase(""));
}

TEST_F(SkinnyWaistUnitTest, SecurityReturnsInternalErrorWhenAppropriate) {
    User user;
    std::list<User> users;
    users.push_back(user);
    EXPECT_CALL(this->user_store_, Put(_, _))
        .WillOnce(Return(UserStoreInterface::Status::FAIL_TO_STORE));
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        this->skinny_waist_.Security(0, users, request_context));
}

TEST_F(SkinnyWaistUnitTest, GetReturnsCorruptionIfUnderlyingStoreCorrupt) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
    .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT,
                this->skinny_waist_.Get(0, "key", NULL, request_context, NULL));
}

TEST_F(SkinnyWaistUnitTest, GetVersionReturnsCorruptionIfUnderlyingStoreCorrupt) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
            .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT,
                this->skinny_waist_.GetVersion(0, "key", NULL, request_context));
}

TEST_F(SkinnyWaistUnitTest, GetNextReturnsCorruptionIfUnderlyingStoreCorrupt) {
    MockPrimaryStoreIterator* iterator = new MockPrimaryStoreIterator();
    EXPECT_CALL(*iterator, Init()).WillOnce(Return(IteratorStatus_STORE_CORRUPT));

    EXPECT_CALL(this->primary_store_, Find("key")).WillOnce(Return(iterator));

    string actual_key;
    PrimaryStoreValue primary_store_value;
    NullableOutgoingValue value;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT, this->skinny_waist_.GetNext(
            0,
            "key",
            &actual_key,
            &primary_store_value,
            &value,
            request_context));
}

TEST_F(SkinnyWaistUnitTest, GetPreviousReturnsCorruptionIfUnderlyingStoreCorrupt) {
    MockPrimaryStoreIterator* iterator = new MockPrimaryStoreIterator();
    EXPECT_CALL(*iterator, Init()).WillOnce(Return(IteratorStatus_STORE_CORRUPT));

    EXPECT_CALL(this->primary_store_, Find("key")).WillOnce(Return(iterator));

    string actual_key;
    PrimaryStoreValue primary_store_value;
    NullableOutgoingValue value;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT, this->skinny_waist_.GetPrevious(
            0,
            "key",
            &actual_key,
            &primary_store_value,
            &value,
            request_context));
}

TEST_F(SkinnyWaistUnitTest, GetKeyRangeReturnsCorruptionIfUnderlyingStoreCorrupt) {
    MockPrimaryStoreIterator* iterator = new MockPrimaryStoreIterator();
    EXPECT_CALL(*iterator, Init()).WillOnce(Return(IteratorStatus_STORE_CORRUPT));

    EXPECT_CALL(this->primary_store_, Find("start")).WillOnce(Return(iterator));

    std::vector<string> result;
    RequestContext request_context;
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT, this->skinny_waist_.GetKeyRange(
            0,
            "start",
            false,
            "end",
            false,
            3,
            false,
            &result,
            request_context));
}

TEST_F(SkinnyWaistUnitTest, DeleteReturnsCorruptionIfUnderlyingStoreCorrupt) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
            .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT,
                this->skinny_waist_.Delete(0, "key", "", false,
                                           false, request_context, token));
}

TEST_F(SkinnyWaistUnitTest, DeleteForceReturnsCorruptionIfUnderlyingStoreCorrupt) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
            .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT,
        this->skinny_waist_.Delete(0, "key", "", false,
                                   false, request_context, token));
}

TEST_F(SkinnyWaistUnitTest, PutReturnsCorruptionIfUnderlyingStoreCorrupt) {
    EXPECT_CALL(this->primary_store_, Get("key", _, _, _))
            .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));
    PrimaryStoreValue primary_store_value;
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT, this->skinny_waist_.Put(
            0,
            "key",
            "",
            primary_store_value,
            NULL,
            false,
            false,
            request_context,
            token));
}

TEST_F(SkinnyWaistUnitTest, PutForceForceReturnsCorruptionIfUnderlyingStoreCorrupt) {
    EXPECT_CALL(this->primary_store_, Put("key", _, _, _, _))
            .WillOnce(Return(StoreOperationStatus_STORE_CORRUPT));
    PrimaryStoreValue primary_store_value;
    RequestContext request_context;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    EXPECT_EQ(StoreOperationStatus_STORE_CORRUPT, this->skinny_waist_.Put(
            0,
            "key",
            "",
            primary_store_value,
            NULL,
            true,
            false,
            request_context,
            token));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
