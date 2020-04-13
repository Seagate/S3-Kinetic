#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "smrdb_test_helpers.h"
#include "db_logger.h"
#include "send_pending_status_interface.h"
#include "pending_cmd_list_proxy.h"
#include "instant_secure_eraser.h"
#include "command_line_flags.h"
#include "helpers/memenv/memenv.h"
#include "db/db_impl.h"
#include "leveldb/db.h"
#include "leveldb/env.h"

using namespace leveldb;  //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class SmrdbMemEnvTest : public ::testing::Test {
 public:
    Env* env_;

    SmrdbMemEnvTest()
            : env_(NewMemEnv(DriveEnv::getInstance())) {
    }
    ~SmrdbMemEnvTest() {
        delete env_;
    }
};

TEST_F(SmrdbMemEnvTest, Basics) {
    uint64_t file_size;
    WritableFile* writable_file;
    std::vector<std::string> children;

    // Create a file.
    EXPECT_TRUE(env_->NewWritableFile("/dir/f", &writable_file).ok());
    delete writable_file;

    // Check that the file exists.
    EXPECT_TRUE(env_->FileExists("/dir/f"));
    EXPECT_TRUE(env_->GetFileSize("/dir/f", &file_size).ok());
    EXPECT_EQ((unsigned int) 0, file_size);
    EXPECT_TRUE(env_->GetChildren("/dir", &children).ok());
    EXPECT_EQ((unsigned int) 1, children.size());
    EXPECT_EQ("f", children[0]);

    // Write to the file.
    EXPECT_TRUE(env_->NewWritableFile("/dir/f", &writable_file).ok());
    EXPECT_TRUE(writable_file->Append("abc").ok());
    delete writable_file;

    // Check for expected size.
    EXPECT_TRUE(env_->GetFileSize("/dir/f", &file_size).ok());
    EXPECT_EQ((unsigned int) 3, file_size);

    // Check that renaming works.
    EXPECT_TRUE(!env_->RenameFile("/dir/non_existent", "/dir/g").ok());
    EXPECT_TRUE(env_->RenameFile("/dir/f", "/dir/g").ok());
    EXPECT_TRUE(!env_->FileExists("/dir/f"));
    EXPECT_TRUE(env_->FileExists("/dir/g"));
    EXPECT_TRUE(env_->GetFileSize("/dir/g", &file_size).ok());
    EXPECT_EQ((unsigned int) 3, file_size);

    // Check that opening non-existent file fails.
    SequentialFile* seq_file;
    RandomAccessFile* rand_file;
    EXPECT_TRUE(!env_->NewSequentialFile("/dir/non_existent", &seq_file).ok());
    EXPECT_TRUE(!seq_file);
    EXPECT_TRUE(!env_->NewRandomAccessFile("/dir/non_existent", &rand_file).ok());
    EXPECT_TRUE(!rand_file);

    // Check that deleting works.
    EXPECT_TRUE(!env_->DeleteFile("/dir/non_existent").ok());
    EXPECT_TRUE(env_->DeleteFile("/dir/g").ok());
    EXPECT_TRUE(!env_->FileExists("/dir/g"));
    EXPECT_TRUE(env_->GetChildren("/dir", &children).ok());
    EXPECT_EQ((unsigned int) 0, children.size());
    EXPECT_TRUE(env_->DeleteDir("/dir").ok());
}

TEST_F(SmrdbMemEnvTest, ReadWrite) {
    WritableFile* writable_file;
    SequentialFile* seq_file;
    RandomAccessFile* rand_file;
    Slice result;
    char scratch[100];

    EXPECT_TRUE(env_->NewWritableFile("/dir/f", &writable_file).ok());
    EXPECT_TRUE(writable_file->Append("hello ").ok());
    EXPECT_TRUE(writable_file->Append("world").ok());
    delete writable_file;

    // Read sequentially.
    EXPECT_TRUE(env_->NewSequentialFile("/dir/f", &seq_file).ok());
    EXPECT_TRUE(seq_file->Read(5, &result, scratch).ok()); // Read "hello.ok()".
    EXPECT_EQ(0, result.compare("hello"));
    EXPECT_TRUE(seq_file->Skip(1).ok());
    EXPECT_TRUE(seq_file->Read(1000, &result, scratch).ok()); // Read "world.ok()".
    EXPECT_EQ(0, result.compare("world"));
    EXPECT_TRUE(seq_file->Read(1000, &result, scratch).ok()); // Try reading past EO.ok()F.
    EXPECT_EQ((unsigned int) 0, result.size());
    EXPECT_TRUE(seq_file->Skip(100).ok()); // Try to skip past end of fil.ok()e.
    EXPECT_TRUE(seq_file->Read(1000, &result, scratch).ok());
    EXPECT_EQ((unsigned int) 0, result.size());
    delete seq_file;

    // Random reads.
    EXPECT_TRUE(env_->NewRandomAccessFile("/dir/f", &rand_file).ok());
    EXPECT_TRUE(rand_file->Read(6, 5, &result, scratch).ok()); // Read "world.ok()".
    EXPECT_EQ(0, result.compare("world"));
    EXPECT_TRUE(rand_file->Read(0, 5, &result, scratch).ok()); // Read "hello.ok()".
    EXPECT_EQ(0, result.compare("hello"));
    EXPECT_TRUE(rand_file->Read(10, 100, &result, scratch).ok()); // Read "d.ok()".
    EXPECT_EQ(0, result.compare("d"));

    // Too high offset.
    EXPECT_TRUE(!rand_file->Read(1000, 5, &result, scratch).ok());
    delete rand_file;
}

TEST_F(SmrdbMemEnvTest, Locks) {
    FileLock* lock;

    // These are no-ops, but we test they return success.
    EXPECT_TRUE(env_->LockFile("some file", &lock).ok());
    EXPECT_TRUE(env_->UnlockFile(lock).ok());
}

TEST_F(SmrdbMemEnvTest, Misc) {
    std::string test_dir;
    EXPECT_TRUE(env_->GetTestDirectory(&test_dir).ok());
    EXPECT_TRUE(!test_dir.empty());

    WritableFile* writable_file;
    EXPECT_TRUE(env_->NewWritableFile("/a/b", &writable_file).ok());

    // These are no-ops, but we test they return success.
    EXPECT_TRUE(writable_file->Sync().ok());
    EXPECT_TRUE(writable_file->Flush().ok());
    EXPECT_TRUE(writable_file->Close().ok());
    delete writable_file;
}

TEST_F(SmrdbMemEnvTest, LargeWrite) {
    const size_t kWriteSize = 300 * 1024;
    char* scratch = new char[kWriteSize * 2];

    std::string write_data;
    for (size_t i = 0; i < kWriteSize; ++i) {
        write_data.append(1, static_cast<char>(i));
    }

    WritableFile* writable_file;
    EXPECT_TRUE(env_->NewWritableFile("/dir/f", &writable_file).ok());
    EXPECT_TRUE(writable_file->Append("foo").ok());
    EXPECT_TRUE(writable_file->Append(write_data).ok());
    delete writable_file;

    SequentialFile* seq_file;
    Slice result;
    EXPECT_TRUE(env_->NewSequentialFile("/dir/f", &seq_file).ok());
    EXPECT_TRUE(seq_file->Read(3, &result, scratch).ok()); // Read "foo.ok()".
    EXPECT_EQ(0, result.compare("foo"));

    size_t read = 0;
    std::string read_data;
    while (read < kWriteSize) {
        EXPECT_TRUE(seq_file->Read(kWriteSize - read, &result, scratch).ok());
        read_data.append(result.data(), result.size());
        read += result.size();
    }
    EXPECT_TRUE(write_data == read_data);
    delete seq_file;
    delete [] scratch;
}

// TEST_F(SmrdbMemEnvTest, DBTest) {
//     DB* db;
//     DbLogger logger;
//     PendingCmdListProxy cmd_list_proxy;
//     MockSendPendingStatusInterface send_pending_status_sender;
//     Options options;
//     options.create_if_missing = true;
//     options.env = env_;
//     options.compression = leveldb::kNoCompression;
//     options.table_cache_size = FLAGS_table_cache_size;
//     options.info_log = &logger;
//     options.outstanding_status_sender = &cmd_list_proxy;
//     options.block_size = FLAGS_block_size;
//     options.write_buffer_size = FLAGS_sst_size;
//     options.value_size_threshold = FLAGS_file_store_minimum_size;
//     cmd_list_proxy.SetListOwnerReference(&send_pending_status_sender);

//     InstantSecureEraserX86::ClearSuperblocks(FLAGS_store_test_partition);

//     const Slice keys[] = {Slice("aaa"), Slice("bbb"), Slice("ccc")};
//     const std::string vals[] = {"foo", "bar", "baz"};

//     EXPECT_TRUE(DB::Open(options, FLAGS_store_test_partition, &db).ok());
//     for (size_t i = 0; i < 3; ++i) {
//         EXPECT_TRUE(db->Put(WriteOptions(), keys[i], PackValueMem(vals[i])).ok());
//     }

//     for (size_t i = 0; i < 3; ++i) {
//         char* res =  new char[sizeof(res)];
//         EXPECT_TRUE(db->Get(ReadOptions(), keys[i], res, false).ok());
//         EXPECT_TRUE(UnpackValue(res) == vals[i]);
//     }

//     Iterator* iterator = db->NewIterator(ReadOptions());
//     iterator->SeekToFirst();
//     for (size_t i = 0; i < 3; ++i) {
//         EXPECT_TRUE(iterator->Valid());
//         EXPECT_TRUE(keys[i] == iterator->key());
//         EXPECT_TRUE(vals[i] == ExtractValue(iterator->value().data()));
//         iterator->Next();
//     }
//     EXPECT_TRUE(!iterator->Valid());
//     delete iterator;

//     for (size_t i = 0; i < 3; ++i) {
//         char* res;
//         res =  new char[sizeof(res)];
//         EXPECT_TRUE(db->Get(ReadOptions(), keys[i], res, false).ok());
//         EXPECT_TRUE(UnpackValue(res) == vals[i]);
//     }

//     delete db;
// }

} // namespace kinetic
} // namespace seagate
} // namespace com
