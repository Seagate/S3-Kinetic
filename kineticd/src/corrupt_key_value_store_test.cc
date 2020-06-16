#include <stdlib.h>
#include <stdio.h>

#include "gtest/gtest.h"

#include "kinetic/incoming_value.h"

#include "file_system_store.h"
#include "internal_value_record.pb.h"
#include "key_value_store_interface.h"
#include "key_value_store.h"
#include "mock_device_information.h"
#include "primary_store.h"
#include "std_map_key_value_store.h"
#include "mock_log_handler.h"
#include "command_line_flags.h"
#include "instant_secure_eraser.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingStringValue;
using proto::InternalValueRecord;
using std::string;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::AtLeast;

class CorruptKeyValueStoreTest : public testing::Test {
    protected:
    virtual void SetUp() {
        InstantSecureEraser::ClearSuperblocks(FLAGS_store_test_partition);
        corrupt_store_ = new KeyValueStore(FLAGS_store_test_partition,
                    FLAGS_table_cache_size,
                    FLAGS_block_size,
                    FLAGS_sst_size);
        corrupt_store_->SetListOwnerReference(&send_pending_status_sender_);
        corrupt_store_->Init(false);
    }

    virtual void TearDown() {
        corrupt_store_->DestroyDataBase();
        delete corrupt_store_;
    }

    KeyValueStore* corrupt_store_;
    MockSendPendingStatusInterface send_pending_status_sender_;
};

TEST_F(CorruptKeyValueStoreTest, PutReturnsErrorForCorruptDb) {
    char value[] = "value";
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_STORE_CORRUPT, corrupt_store_->Put("key", value,
                                                                      false, token));
}

TEST_F(CorruptKeyValueStoreTest, GetReturnsErrorForCorruptDb) {
//    string value;
    char* value = new char[sizeof(value)];
    ASSERT_EQ(StoreOperationStatus_STORE_CORRUPT, corrupt_store_->Get("key", value));
    delete value;
}

TEST_F(CorruptKeyValueStoreTest, DeleteReturnsErrorForCorruptDb) {
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    ASSERT_EQ(StoreOperationStatus_STORE_CORRUPT, corrupt_store_->Delete("key", false,
                                                                         token));
}

TEST_F(CorruptKeyValueStoreTest, FindReturnsErrorForCorruptDb) {
    KeyValueStoreIterator *it = corrupt_store_->Find("foo");
    ASSERT_TRUE(it);

    ASSERT_EQ(IteratorStatus_STORE_CORRUPT, it->Init());
    delete it;
}

// TEST_F(CorruptKeyValueStoreTest, ClearHealsCorruptDB) {
//     char foo[] = "foobarbaz";
//     ASSERT_EQ(StoreOperationStatus_SUCCESS, corrupt_store_->Clear());
//     ASSERT_EQ(StoreOperationStatus_SUCCESS, corrupt_store_->Put("key", foo, false));
//     string value;
//     ASSERT_EQ(StoreOperationStatus_SUCCESS, corrupt_store_->Get("key", &value));
//     ASSERT_EQ("foobarbaz", value);
// }

// TEST_F(CorruptKeyValueStoreTest, ManifestReplayTest) {
//     MockLogHandler log_handler;
//     ASSERT_FALSE(system("rm -rf manifest_replay_test.db"));
//     KeyValueStore* db = new KeyValueStore("manifest_replay_test.db", 100);
//     ASSERT_TRUE(db->Init(true));

//     db->SetLogHandlerInterface(&log_handler);

//    EXPECT_CALL(log_handler, LogLatency(_)).Times(AtLeast(1));
//    EXPECT_CALL(log_handler, LogStaleEntry(_)).Times(AtLeast(1));
//    const uint64_t kKeysToWrite = 100;
//    const size_t kValueSize = 1024*1024;

//     // First, write many things into the database to create a large, complex manifest
//     uint64_t keys[kKeysToWrite];
//     unsigned int seed = 0;
//     for (uint64_t i = 0; i <= kKeysToWrite; i++) {
//         uint64_t key_buf = (uint64_t)rand_r(&seed);
//         keys[i] = key_buf;
//         string key((char*)(&key_buf), sizeof(key_buf));

//         char value_buf[kValueSize] = {0};
//         *(uint64_t*)value_buf = key_buf;
//         // string value(value_buf, sizeof(value_buf));

//         ASSERT_EQ(StoreOperationStatus_SUCCESS, db->Put(key, value_buf, false));
//     }

//     // Now overwrite all the keys with new values
//     for (uint64_t i = 0; i < kKeysToWrite; i++) {
//         uint64_t key_buf = keys[i];
//         string key((char*)(&key_buf), sizeof(key_buf));

//         char value_buf[kValueSize] = {0xF};
//         *(uint64_t*)value_buf = key_buf;
//         // string value(value_buf, sizeof(value_buf));

//         ASSERT_EQ(StoreOperationStatus_SUCCESS, db->Put(key, value_buf, true));
//     }

//     // And close and re-open to force replaying the manifest
//     delete db;
//     db = new KeyValueStore("manifest_replay_test.db", 100);
//     ASSERT_TRUE(db->Init(true));

//     uint64_t count = 0;

//     KeyValueStoreIterator* it = db->Find("");
//     it->Init();
//     while (it->Next() == com::seagate::kinetic::IteratorStatus_SUCCESS) {
//         count++;
//     }
//     delete it;

//     ASSERT_EQ(kKeysToWrite, count) << "DB contains wrong number of keys";

//     // Finally, make sure the database has the right contents
//     for (uint64_t i = 0; i < kKeysToWrite; i++) {
//         uint64_t key_buf = keys[i];
//         string key((char*)(&key_buf), sizeof(key_buf));

//         char expected_value_buf[kValueSize] = {0xF};
//         *(uint64_t*)expected_value_buf = key_buf;
//         string expected_value(expected_value_buf, sizeof(expected_value_buf));

//         string value;
//         ASSERT_EQ(StoreOperationStatus_SUCCESS, db->Get(key, &value));
//         ASSERT_EQ(expected_value, value);
//     }

//     delete db;
// }

} // namespace kinetic
} // namespace seagate
} // namespace com
