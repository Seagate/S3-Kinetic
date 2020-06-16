#include <stdlib.h>

#include "gtest/gtest.h"

#include "command_line_flags.h"
#include "kinetic/incoming_value.h"

#include "file_system_store.h"
#include "internal_value_record.pb.h"
#include "key_value_store_interface.h"
#include "key_value_store.h"
#include "mock_device_information.h"
#include "mock_cluster_version_store.h"
#include "primary_store.h"
#include "profiler.h"
#include "std_map_key_value_store.h"
#include "typed_test_helpers.h"
#include "instant_secure_eraser.h"
#include "smrdisk/Disk.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingStringValue;
using ::kinetic::IncomingBuffValue;
using com::seagate::kinetic::StoreOperationStatus;
using proto::InternalValueRecord;
using std::string;
using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::NiceMock;

template <class T>
class PrimaryStoreTest : public testing::Test {
    protected:
    PrimaryStoreTest() :
        profiler_(),
        file_system_store_(),
        mock_cluster_version_store_(),
        device_information_(),
        instant_secure_eraser_() {
        key_value_store_ = CreateKeyValueStore<T>();
        primary_store_ = new PrimaryStore(file_system_store_,
                *key_value_store_,
                mock_cluster_version_store_,
                device_information_,
                profiler_,
                32 * 1024,
                instant_secure_eraser_,
                "fsize");
    }

    virtual ~PrimaryStoreTest() {
        delete primary_store_;
        delete key_value_store_;
    }

    virtual void SetUp() {
        smr::Disk::initializeSuperBlockAddr(FLAGS_store_test_partition);
        InstantSecureEraser::ClearSuperblocks(FLAGS_store_test_partition);
        ASSERT_TRUE(key_value_store_->Init(true));
        key_value_store_->SetListOwnerReference(&send_pending_status_sender_);

        // Simulate a file system with plenty of free space
        EXPECT_CALL(device_information_, GetCapacity(_, _)).WillRepeatedly(DoAll(
            SetArgPointee<0>(4000000000000L),
            SetArgPointee<1>(1000000000000L),
            Return(true)));
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_file_system_store"));
        ASSERT_NE(-1, system("rm -rf fsize"));
        EXPECT_CALL(send_pending_status_sender_, SendAllPending(_, _));
        key_value_store_->Close();
        primary_store_->Close();
    }

    virtual void AssertEqual(const std::string &s, const NullableOutgoingValue &value) {
        std::string value_string;
        int err;
        ASSERT_TRUE(value.ToString(&value_string, &err));
        ASSERT_EQ(s, value_string);
    }

    Profiler profiler_;
    MockFileSystemStore file_system_store_;
    KeyValueStoreInterface* key_value_store_;
    NiceMock<MockClusterVersionStore> mock_cluster_version_store_;
    MockDeviceInformation device_information_;
    MockInstantSecureEraser instant_secure_eraser_;
    PrimaryStore* primary_store_;
    MockSendPendingStatusInterface send_pending_status_sender_;
};

using testing::Types;

typedef Types<KeyValueStore> Implementations;

TYPED_TEST_CASE(PrimaryStoreTest, Implementations);

TYPED_TEST(PrimaryStoreTest, StoresVersion) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "theversion";
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(8);
    memcpy(the_value, "thevalue", 8);
    IncomingBuffValue buff_value(the_value, 8);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &buff_value, false, (token));

    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_value, &value, NULL));
    EXPECT_EQ("theversion", actual_value.version);
    this->AssertEqual("thevalue", value);
}

TYPED_TEST(PrimaryStoreTest, StoresEmptyStringVersion) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "";
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(8);
    memcpy(the_value, "thevalue", 8);
    IncomingBuffValue value(the_value, 8);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &value, false, (token));

    PrimaryStoreValue actual_primary_store_value;
    NullableOutgoingValue actual_value;

    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_primary_store_value, &actual_value, NULL));
    EXPECT_EQ("", actual_primary_store_value.version);
    this->AssertEqual("thevalue", actual_value);
}

TYPED_TEST(PrimaryStoreTest, ToleratesVersionWithEmbeddedNulls) {
    char version_bytes[4] = {'H', 'i', 0, '!'};

    std::string version(version_bytes, 4);

    PrimaryStoreValue primary_store_value;
    primary_store_value.version = version;
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(9);
    memcpy(the_value, "the value", 9);
    IncomingBuffValue buff_value(the_value, 9);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &buff_value, false, (token));

    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;

    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_value, &value, NULL));
    ASSERT_EQ(version, actual_value.version);
    this->AssertEqual("the value", value);
}

TYPED_TEST(PrimaryStoreTest, ToleratesValueWithEmbeddedNulls) {
    char value_bytes[4] = {'H', 'i', 0, '!'};

    string version("version");
    string value(value_bytes, 4);
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = version;
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(4);
    memcpy(the_value, value_bytes, 4);
    IncomingBuffValue buff_value(the_value, 4);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &buff_value, false, (token));

    PrimaryStoreValue actual_primary_store_value;
    NullableOutgoingValue actual_value;
    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_primary_store_value, &actual_value, NULL));
    ASSERT_EQ(version, actual_primary_store_value.version);
    this->AssertEqual(value, actual_value);
}

TYPED_TEST(PrimaryStoreTest, ToleratesKeyWithEmbeddedNulls) {
    char key_bytes[4] = {'H', 'i', 0, '!'};

    string key(key_bytes, 4);

    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "version";
    primary_store_value.algorithm = 1;
    char *the_value;
    the_value = (char*) smr::DynamicMemory::getInstance()->allocate(5);
    memcpy(the_value, "value", 5);
    IncomingBuffValue buff_value(the_value, 5);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put(key, primary_store_value, &buff_value, false, (token));

    PrimaryStoreValue actual_primary_store_value;
    NullableOutgoingValue actual_value;
    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get(key, &actual_primary_store_value, &actual_value, NULL));
    ASSERT_EQ("version", actual_primary_store_value.version);
    this->AssertEqual("value", actual_value);
}

TYPED_TEST(PrimaryStoreTest, StoresTag) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.tag = "tag";
    primary_store_value.algorithm = 1;
    IncomingStringValue the_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &the_value, false, (token));

    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;
    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_value, &value, NULL));
    ASSERT_EQ("tag", actual_value.tag);
}

TYPED_TEST(PrimaryStoreTest, ToleratesEmptyStringTag) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.tag = "";
    primary_store_value.algorithm = 1;
    IncomingStringValue the_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &the_value, false, (token));

    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;
    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_value, &value, NULL));
    ASSERT_EQ("", actual_value.tag);
}


TYPED_TEST(PrimaryStoreTest, ToleratesTagWithEmbeddedNulls) {
    char tag_bytes[] = {'n', 'u', 0, 'l', 'l'};
    string tag(tag_bytes, sizeof(tag_bytes));

    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    primary_store_value.tag = tag;
    IncomingStringValue the_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &the_value, false, (token));

    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;
    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
        this->primary_store_->Get("key", &actual_value, &value, NULL));
    ASSERT_EQ(tag, actual_value.tag);
}

TYPED_TEST(PrimaryStoreTest, StoresAlgorithm) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1234;

    IncomingStringValue the_value("");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("key", primary_store_value, &the_value, false, (token));
    PrimaryStoreValue actual_value;
    NullableOutgoingValue value;
    ASSERT_EQ(StoreOperationStatus::StoreOperationStatus_SUCCESS,
    this->primary_store_->Get("key", &actual_value, &value, NULL));
    ASSERT_EQ(1234, actual_value.algorithm);
}


TYPED_TEST(PrimaryStoreTest, AllowsIteratingForward) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "ver_a";
    primary_store_value.tag = "tag_a";
    primary_store_value.algorithm = 1;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_a, "value_a", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("a", primary_store_value, &value_a, false, (token));

    primary_store_value.version = "ver_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = 2;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_b, "value_b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    this->primary_store_->Put("b", primary_store_value, &value_b, false, (token));

    primary_store_value.version = "ver_c";
    primary_store_value.tag = "tag_c";
    primary_store_value.algorithm = 3;
    char *the_value_c;
    the_value_c = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_c, "value_c", 7);
    IncomingBuffValue value_c(the_value_c, 7);
    this->primary_store_->Put("c", primary_store_value, &value_c, false, (token));

    PrimaryStoreIterator* it = this->primary_store_->Find("b");
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());
    NullableOutgoingValue value1, value2;
    std::string version, tag;
    int32_t algorithm;

    ASSERT_EQ("b", it->Key());
    ASSERT_TRUE(it->Version(&version));
    ASSERT_EQ("ver_b", version);
    ASSERT_EQ(StoreOperationStatus_SUCCESS, it->Value(&value1));
    this->AssertEqual("value_b", value1);
    ASSERT_TRUE(it->Tag(&tag));
    ASSERT_EQ("tag_b", tag);
    ASSERT_TRUE(it->Algorithm(&algorithm));
    ASSERT_EQ(2, algorithm);
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Next());

    ASSERT_EQ("c", it->Key());
    ASSERT_TRUE(it->Version(&version));
    ASSERT_EQ("ver_c", version);
    ASSERT_EQ(StoreOperationStatus_SUCCESS, it->Value(&value2));
    this->AssertEqual("value_c", value2);
    ASSERT_TRUE(it->Tag(&tag));
    ASSERT_EQ("tag_c", tag);
    ASSERT_TRUE(it->Algorithm(&algorithm));
    ASSERT_EQ(3, algorithm);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Next());
    delete it;
}

TYPED_TEST(PrimaryStoreTest, AllowsIteratingBackward) {
    PrimaryStoreValue primary_store_value;
    primary_store_value.version = "ver_a";
    primary_store_value.tag = "tag_a";
    primary_store_value.algorithm = 1;
    char *the_value_a;
    the_value_a = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_a, "value_a", 7);
    IncomingBuffValue value_a(the_value_a, 7);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    this->primary_store_->Put("a", primary_store_value, &value_a, false, (token));

    primary_store_value.version = "ver_b";
    primary_store_value.tag = "tag_b";
    primary_store_value.algorithm = 2;
    char *the_value_b;
    the_value_b = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_b, "value_b", 7);
    IncomingBuffValue value_b(the_value_b, 7);
    this->primary_store_->Put("b", primary_store_value, &value_b, false, (token));

    primary_store_value.version = "ver_c";
    primary_store_value.tag = "tag_c";
    primary_store_value.algorithm = 3;
    char *the_value_c;
    the_value_c = (char*) smr::DynamicMemory::getInstance()->allocate(7);
    memcpy(the_value_c, "value_c", 7);
    IncomingBuffValue value_c(the_value_c, 7);
    this->primary_store_->Put("c", primary_store_value, &value_c, false, (token));

    PrimaryStoreIterator* it = this->primary_store_->Find("b");
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());
    NullableOutgoingValue value1, value2;
    std::string version, tag;
    int32_t algorithm;

    ASSERT_EQ("b", it->Key());
    ASSERT_TRUE(it->Version(&version));
    ASSERT_EQ("ver_b", version);
    ASSERT_EQ(StoreOperationStatus_SUCCESS, it->Value(&value1));
    this->AssertEqual("value_b", value1);
    ASSERT_TRUE(it->Tag(&tag));
    ASSERT_EQ("tag_b", tag);
    ASSERT_TRUE(it->Algorithm(&algorithm));
    ASSERT_EQ(2, algorithm);
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Prev());

    ASSERT_EQ("a", it->Key());
    ASSERT_TRUE(it->Version(&version));
    ASSERT_EQ("ver_a", version);
    ASSERT_EQ(StoreOperationStatus_SUCCESS, it->Value(&value2));
    this->AssertEqual("value_a", value2);
    ASSERT_TRUE(it->Tag(&tag));
    ASSERT_EQ("tag_a", tag);
    ASSERT_TRUE(it->Algorithm(&algorithm));
    ASSERT_EQ(1, algorithm);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Prev());
    delete it;
}

class PrimaryStoreUnitTest : public testing::Test {
    protected:
    PrimaryStoreUnitTest() :
        profiler_(),
        file_system_store_(),
        mock_key_value_store_(),
        mock_cluster_version_store_(),
        device_information_(),
        instant_secure_eraser_(),
        primary_store_(file_system_store_,
                mock_key_value_store_,
                mock_cluster_version_store_,
                device_information_,
                profiler_,
                32 * 1024,
                instant_secure_eraser_,
                "fsize") {}

    void SetUp() {
        // Simulate a file system with plenty of free space
        EXPECT_CALL(device_information_, GetCapacity(_, _)).WillRepeatedly(DoAll(
            SetArgPointee<0>(4000000000000L),
            SetArgPointee<1>(1000000000000L),
            Return(true)));
    }

    void TearDown() {
        primary_store_.Close();
    }


    // Expect a Get on mock_key_value_store_ and return an embedded (small) value
    void ExpectSmallValue(const std::string &key) {
        InternalValueRecord internal_value_record;
        internal_value_record.set_version("version");
        internal_value_record.set_value("value");
        internal_value_record.set_tag("tag");
        std::string packed_value;
        ASSERT_TRUE(internal_value_record.SerializeToString(&packed_value));
        char value;
        EXPECT_CALL(mock_key_value_store_, Get(key, &value, false, true, NULL)).
            WillOnce(DoAll(SetArrayArgument<1>(&(packed_value[0]),
                           &(packed_value[0])+packed_value.length()),
                           Return(StoreOperationStatus_SUCCESS)));
    }

    // Expect a Get on mock_key_value_store_ and return an indication that the value
    // is stored in the file system
    void ExpectLargeValue(const std::string &key) {
        InternalValueRecord internal_value_record;
        internal_value_record.set_version("version");
        internal_value_record.set_tag("tag");
        std::string packed_value;
        ASSERT_TRUE(internal_value_record.SerializeToString(&packed_value));
        char value;

        EXPECT_CALL(mock_key_value_store_, Get(key, &value, false, true, _)).
            WillOnce(DoAll(SetArrayArgument<1>(&(packed_value[0]),
                           &(packed_value[0])+packed_value.length()),
                           Return(StoreOperationStatus_SUCCESS)));
    }

    void ExpectLargeValueError(const std::string &key) {
        InternalValueRecord internal_value_record;
        internal_value_record.set_version("version");
        internal_value_record.set_tag("tag");
        std::string packed_value;
        ASSERT_TRUE(internal_value_record.SerializeToString(&packed_value));
        char value;

        EXPECT_CALL(mock_key_value_store_, Get(key, &value, false, false, _)).
            WillOnce(DoAll(SetArrayArgument<1>(&(packed_value[0]),
                           &(packed_value[0])+packed_value.length()),
                           Return(StoreOperationStatus_SUCCESS)));
    }


    Profiler profiler_;
    MockFileSystemStore file_system_store_;
    MockKeyValueStore mock_key_value_store_;
    NiceMock<MockClusterVersionStore> mock_cluster_version_store_;
    MockDeviceInformation device_information_;
    MockInstantSecureEraser instant_secure_eraser_;
    PrimaryStore primary_store_;
};


TEST_F(PrimaryStoreUnitTest, PutSmallValue) {
    std::string key("key");
    EXPECT_CALL(mock_key_value_store_, Put(key, _, _, _))
        .WillOnce(DoAll(DeleteArg<1>(), Return(StoreOperationStatus_SUCCESS)));
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue value("small");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
    primary_store_.Put(key, primary_store_value, &value, false, token));
}


TEST_F(PrimaryStoreUnitTest, PutLargeValue) {
    std::string key("key");
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    std::string long_string;

    // Use a large value so that it lands in the file system
    long_string.resize(32 * 1024, 'v');
    IncomingStringValue value(long_string);

    EXPECT_CALL(mock_key_value_store_, Put(key, _, _, _))
        .WillOnce(DoAll(DeleteArg<1>(), Return(StoreOperationStatus_SUCCESS)));

    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
    primary_store_.Put(key, primary_store_value, &value, false, token));
}

TEST_F(PrimaryStoreUnitTest, ReplaceLargeValueWithSmallValue) {
    std::string key("key");
    EXPECT_CALL(mock_key_value_store_, Put(key, _, _, _))
        .WillOnce(DoAll(DeleteArg<1>(), Return(StoreOperationStatus_SUCCESS)));

    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    IncomingStringValue value("value");
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS,
    primary_store_.Put(key, primary_store_value, &value, false, token));
}

TEST_F(PrimaryStoreUnitTest, ClearEmptiesKeyValueAndFileSystemStore) {
    std::string pin("");
    EXPECT_CALL(instant_secure_eraser_, Erase(pin))
        .WillOnce(Return(PinStatus::PIN_SUCCESS));
    EXPECT_CALL(instant_secure_eraser_, MountCreateFileSystem(_))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_key_value_store_, Init(false))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_key_value_store_, Init(true))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_cluster_version_store_, Erase())
        .WillOnce(Return(true));
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Clear(pin, false));
}

TEST_F(PrimaryStoreUnitTest, WriteCorrectNumberOfPreusedBytes) {
    // Write preused bytes
    primary_store_.SetPreUsedBytes();

    // Read from fsize file and make sure that the correct number of bytes was written
    uint64_t total_bytes;
    uint64_t used_bytes;
    device_information_.GetCapacity(&total_bytes, &used_bytes);

    uint64_t preused_bytes;
    std::ifstream infile("fsize", std::ifstream::binary);
    if (infile.is_open()) {
        string preused;
        getline(infile, preused);
        infile.close();
        preused_bytes = strtol(preused.c_str(), NULL, 0);
    } else {
        preused_bytes = 0;
    }
    EXPECT_EQ(used_bytes, preused_bytes);
}


// Error handling tests

TEST_F(PrimaryStoreUnitTest, GetReturnsInternalErrorIfKeyValueStoreFails) {
    std::string key("key");
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    NullableOutgoingValue value;

    EXPECT_CALL(mock_key_value_store_, Get(key, _, _, _, _))
        .WillOnce(Return(StoreOperationStatus_INTERNAL_ERROR));
    ASSERT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        primary_store_.Get(key, &primary_store_value, &value));
}

TEST_F(PrimaryStoreUnitTest, PutDeletesLargeValueIfKvPutFails) {
    std::string key("key");

    EXPECT_CALL(mock_key_value_store_, Put(key, _, _, _))
            .WillOnce(Return(StoreOperationStatus_INTERNAL_ERROR));

    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    std::string long_string;

    // Use a large value so that it lands in the file system
    long_string.resize(32 * 1024, 'v');
    IncomingStringValue value(long_string);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_INTERNAL_ERROR,
        primary_store_.Put(key, primary_store_value, &value, false, token));
}


TEST_F(PrimaryStoreUnitTest, DeleteReturnsInternalErrorIfKeyValueStoreFails) {
    std::string key("key");

    EXPECT_CALL(mock_key_value_store_, Delete(key, false, _))
        .WillOnce(Return(StoreOperationStatus_INTERNAL_ERROR));
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_INTERNAL_ERROR, primary_store_.Delete(key,
        false, token));
}


// TEST_F(PrimaryStoreUnitTest, DeleteReturnsInternalErrorIfFileSystemStoreFails) {
//     std::string key("key");
//     ExpectLargeValue(key);
//     EXPECT_CALL(file_system_store_, Delete(key))
//         .WillOnce(Return(false));
//     ASSERT_EQ(StoreOperationStatus_INTERNAL_ERROR, primary_store_.Delete(key, false));
// }

TEST_F(PrimaryStoreUnitTest, ClearReturnsInternalErrorIfKeyValueStoreFails) {
    std::string pin("");
    EXPECT_CALL(instant_secure_eraser_, Erase(pin))
        .WillOnce(Return(PinStatus::INTERNAL_ERROR));
    ASSERT_EQ(StoreOperationStatus_INTERNAL_ERROR, primary_store_.Clear(pin, false));
}

TEST_F(PrimaryStoreUnitTest, PutReturnsNoSpaceWhenAppropriate) {
    EXPECT_CALL(device_information_, GetCapacity(_, _)).WillOnce(DoAll(
        SetArgPointee<0>(4000000000000L),
        SetArgPointee<1>(3995000000000L),
        Return(true)));

    EXPECT_CALL(mock_key_value_store_, Put("key", _, _, _))
            .WillOnce(Return(StoreOperationStatus_SUCCESS));

    IncomingStringValue value("value");
    PrimaryStoreValue primary_store_value;
    primary_store_value.algorithm = 1;
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_SUCCESS, primary_store_.Put("key",
        primary_store_value, &value, false, token));
    ASSERT_EQ(smr::Disk::DiskStatus::NO_SPACE, smr::Disk::_status);
}
} // namespace kinetic
} // namespace seagate
} // namespace com
