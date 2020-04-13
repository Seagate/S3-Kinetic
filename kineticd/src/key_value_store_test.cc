#include <stdlib.h>

#include "gtest/gtest.h"

#include "command_line_flags.h"
#include "key_value_store.h"
#include "instant_secure_eraser.h"
#include "smrdb_test_helpers.h"
#include "smrdisk/Disk.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::testing::_;

class KeyValueStoreTest : public testing::Test {
    protected:
    KeyValueStoreTest() {
        smr::Disk::initializeSuperBlockAddr(FLAGS_store_test_partition);
        InstantSecureEraserX86::ClearSuperblocks(FLAGS_store_test_partition);
        key_value_store_ = new KeyValueStore(FLAGS_store_test_partition,
                                             FLAGS_table_cache_size,
                                             FLAGS_block_size,
                                             FLAGS_sst_size);;
    }

    virtual void SetUp() {
        ASSERT_TRUE(key_value_store_->Init(true));
        key_value_store_->SetListOwnerReference(&send_pending_status_sender_);
    }

    virtual ~KeyValueStoreTest() {
        EXPECT_CALL(send_pending_status_sender_, SendAllPending(_, _));
        key_value_store_->Close();
    }

    KeyValueStore* key_value_store_;
    MockSendPendingStatusInterface send_pending_status_sender_;
};

TEST_F(KeyValueStoreTest, GetReturnsValue) {
    char* val = (char*)malloc(6);
    memcpy(val, "value", 6);
    char* value = PackValue(val);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    LOG(ERROR) << "Before put";
    key_value_store_->Put("key", value, false, token);
    LOG(ERROR) << "After put";
    char* packed_value =  new char[sizeof(packed_value)];
    key_value_store_->Get("key", packed_value, false);
    std::string result_value = UnpackValue(packed_value);
    EXPECT_EQ("value", result_value);
    delete[] packed_value;
}

TEST_F(KeyValueStoreTest, GetReturnsNotFoundIfNotPresent) {
    char* result_value =  new char[sizeof(result_value)];

    EXPECT_EQ(StoreOperationStatus_NOT_FOUND,
        key_value_store_->Get("key", result_value, false));
    delete [] result_value;
}

TEST_F(KeyValueStoreTest, GetReturnsSuccessIfPresent) {
    char* val = (char*)malloc(6);
    memcpy(val, "value", 6);
    char* value = PackValue(val);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("key", value, false, token);
    char* result_value =  new char[sizeof(result_value)];

    EXPECT_EQ(StoreOperationStatus_SUCCESS,
        key_value_store_->Get("key", result_value, false));
    delete [] result_value;
}

TEST_F(KeyValueStoreTest, FindReturnsIteratorThatWorksForward) {
    char* val1 = (char*)malloc(8);
    memcpy(val1, "value_1", 8);
    char* value1 = PackValue(val1);
    char* val2 = (char*)malloc(8);
    memcpy(val2, "value_2", 8);
    char* value2 = PackValue(val2);
    char* val3 = (char*)malloc(8);
    memcpy(val3, "value_3", 8);
    char* value3 = PackValue(val3);
    char* val4 = (char*)malloc(8);
    memcpy(val4, "value_4", 8);
    char* value4 = PackValue(val4);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("a", value1, false, token);
    key_value_store_->Put("b", value2, false, token);
    key_value_store_->Put("c", value3, false, token);
    key_value_store_->Put("d", value4, false, token);

    KeyValueStoreIterator *it = (key_value_store_->Find("c"));
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());

    ASSERT_EQ("c", it->Key());
    std::string result_value1 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_3", result_value1);
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Next());

    ASSERT_EQ("d", it->Key());
    std::string result_value2 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_4", result_value2);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Next());
    delete it;
}

TEST_F(KeyValueStoreTest, FindReturnsIteratorThatWorksBackward) {
    char* val1 = (char*)malloc(8);
    memcpy(val1, "value_1", 8);
    char* value1 = PackValue(val1);
    char* val2 = (char*)malloc(8);
    memcpy(val2, "value_2", 8);
    char* value2 = PackValue(val2);
    char* val3 = (char*)malloc(8);
    memcpy(val3, "value_3", 8);
    char* value3 = PackValue(val3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("a", value1, false, token);
    key_value_store_->Put("b", value2, false, token);
    key_value_store_->Put("c", value3, false, token);

    KeyValueStoreIterator* it = key_value_store_->Find("c");
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());

    ASSERT_EQ("c", it->Key());
    std::string result_value1 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_3", result_value1);
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Prev());

    ASSERT_EQ("b", it->Key());
    std::string result_value2 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_2", result_value2);
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Prev());

    ASSERT_EQ("a", it->Key());
    std::string result_value3 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_1", result_value3);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Prev());

    delete it;
}

TEST_F(KeyValueStoreTest, FindReturnsForwardInteratorIfNoExactMatch) {
    char* val1 = (char*)malloc(8);
    memcpy(val1, "value_1", 8);
    char* value1 = PackValue(val1);
    char* val3 = (char*)malloc(8);
    memcpy(val3, "value_3", 8);
    char* value3 = PackValue(val3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("a", value1, false, token);
    key_value_store_->Put("c", value3, false, token);
    KeyValueStoreIterator* it = key_value_store_->Find("b");
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());

    ASSERT_EQ("c", it->Key());
    std::string result_value = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_3", result_value);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Next());

    delete it;
}

TEST_F(KeyValueStoreTest, FindReturnsBackwardIteratorIfNoExactMatch) {
    char* val1 = (char*)malloc(8);
    memcpy(val1, "value_1", 8);
    char* value1 = PackValue(val1);
    char* val3 = (char*)malloc(8);
    memcpy(val3, "value_3", 8);
    char* value3 = PackValue(val3);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("a", value1, false, token);
    key_value_store_->Put("c", value3, false, token);

    KeyValueStoreIterator* it = key_value_store_->Find("b");
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());

    ASSERT_EQ("c", it->Key());
    std::string result_value1 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_3", result_value1);
    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Prev());

    ASSERT_EQ("a", it->Key());
    std::string result_value2 = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value_1", result_value2);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Prev());

    delete it;
}

TEST_F(KeyValueStoreTest, FindReturnsIteratorThatWorksIfEmptyDb) {
    KeyValueStoreIterator* it = key_value_store_->Find("b");
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Init());

    delete it;
}

TEST_F(KeyValueStoreTest, FindReturnsIteratorThatWorksIfAtLastObject) {
    char* val = (char*)malloc(6);
    memcpy(val, "value", 6);
    char* value = PackValue(val);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("key", value, false, token);

    KeyValueStoreIterator* it = key_value_store_->Find("a");

    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Init());

    ASSERT_EQ("key", it->Key());
    std::string result_value = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value", result_value);
    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Next());

    delete it;
}

TEST_F(KeyValueStoreTest, FindReturnsIteratorThatWorksIfCalledWithKeyLargerThanLargestKey) {
    char* val = (char*)malloc(6);
    memcpy(val, "value", 6);
    char* value = PackValue(val);
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    key_value_store_->Put("key", value, false, token);

    KeyValueStoreIterator* it = key_value_store_->Find("zzzzzzzzzzz");

    ASSERT_EQ(IteratorStatus::IteratorStatus_NOT_FOUND, it->Init());

    ASSERT_EQ(IteratorStatus::IteratorStatus_SUCCESS, it->Last());
    ASSERT_EQ("key", it->Key());
    std::string result_value = ExtractValue(it->Value(), key_value_store_->GetName());
    ASSERT_EQ("value", result_value);

    delete it;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
