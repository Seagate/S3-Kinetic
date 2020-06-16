#include "gtest/gtest.h"

#include <string>
#include <vector>

#include "instant_secure_eraser.h"
#include "smrdb_test_helpers.h"
#include "command_line_flags.h"

#include "leveldb/db.h"
#include "db/db_impl.h"
#include "leveldb/env.h"

using namespace leveldb;  // NOLINT

namespace com {
namespace seagate {
namespace kinetic {

const int kPreusedZones = 11;

class SmrdbSmrEnvTest : public ::testing::Test {
 public:
    Env* env_;
    std::string db_path_;

    SmrdbSmrEnvTest()
        : env_(NULL) {
        env_ = DriveEnv::getInstance();
        db_path_ = FLAGS_store_test_partition;
        smr::Disk::initializeSuperBlockAddr(db_path_);
        DestroyDB(db_path_, Options());
        InstantSecureEraser::ClearSuperblocks(db_path_);
    }

    ~SmrdbSmrEnvTest() {
        env_->clearDisk();
        InstantSecureEraser::ClearSuperblocks(db_path_);
    }
};

TEST_F(SmrdbSmrEnvTest, DefragmentZonesAllErased) {
    int zones_to_write = 3;
    const size_t kWriteSize = 299 * 1024;
    std::string write_data;

    for (size_t i = 0; i < kWriteSize; ++i) {
        write_data.append(1, static_cast<char>(i));
    }

    uint64_t fsize = kWriteSize*10;
    int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

    EXPECT_TRUE(env_->CreateDir(db_path_, true).ok());
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones);
    stringstream ss;

    WritableFile* writable_file;
    string fileNumPrefix("000000");

    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        EXPECT_TRUE(env_->NewWritableFile(db_path_ + "/" + fname, &writable_file).ok());
        for (int j = 0; j < 10; ++j) {
            writable_file->Append(write_data);
        }
        delete writable_file;
        ss.str("");
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);
    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);


    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(db_path_ + "/" + fname);
        ss.str("");
    }

    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones);
}

TEST_F(SmrdbSmrEnvTest, DefragmentZonesPartiallyErased) {
    int zones_to_write = 3;
    const size_t kWriteSize = 299 * 1024;
    std::string write_data;

    for (size_t i = 0; i < kWriteSize; ++i) {
        write_data.append(1, static_cast<char>(i));
    }

    uint64_t fsize = kWriteSize*10;
    int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

    EXPECT_TRUE(env_->CreateDir(db_path_, true).ok());
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones);
    stringstream ss;

    WritableFile* writable_file;
    string fileNumPrefix("000000");

    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        EXPECT_TRUE(env_->NewWritableFile(db_path_ + "/" + fname, &writable_file).ok());
        for (int j = 0; j < 10; ++j) {
                writable_file->Append(write_data);
        }
        delete writable_file;
        ss.str("");
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);
    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);


    // delete first half of files on first zone
    for (int i = 4; i < numFilesPerZone/2; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(db_path_ + "/" + fname);
        ss.str("");
    }

    // delete first quarter of files on second zone
    for (int i = numFilesPerZone; i < numFilesPerZone + (2*numFilesPerZone)/4; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(db_path_ + "/" + fname);
        ss.str("");
    }

    // delete first quarter of files on third zone
    for (int i = numFilesPerZone*2; i < numFilesPerZone + (3*numFilesPerZone)/4; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        env_->DeleteFile(db_path_ + "/" + fname);
        ss.str("");
    }

    for (int i = 0; i < zones_to_write; i++) {
        env_->Defragment(0);
    }

    EXPECT_EQ(env_->GetZonesUsed(), (kPreusedZones+zones_to_write)-1);
}

TEST_F(SmrdbSmrEnvTest, DefragmentZonesErasedAlternate) {
    int zones_to_write = 2;
    const size_t kWriteSize = 299 * 1024;
    std::string write_data;

    for (size_t i = 0; i < kWriteSize; ++i) {
        write_data.append(1, static_cast<char>(i));
    }

    uint64_t fsize = kWriteSize*10;
    int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

    EXPECT_TRUE(env_->CreateDir(db_path_, true).ok());
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones);
    stringstream ss;

    WritableFile* writable_file;
    string fileNumPrefix("000000");

    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        EXPECT_TRUE(env_->NewWritableFile(db_path_ + "/" + fname, &writable_file).ok());
        for (int j = 0; j < 10; ++j) {
                writable_file->Append(write_data);
        }
        delete writable_file;
        ss.str("");
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);
    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);

    // delete half of the files on first zone, alternating files
    for (int i = 4; i < numFilesPerZone; ++i) {
        if (i % 2 == 0) {
            ss << fileNumPrefix << i;
            string fname = ss.str();
            fname = fname.substr(fname.size() - 6);
            fname += ".sst";
            env_->DeleteFile(db_path_ + "/" + fname);
            ss.str("");
        }
    }

    // delete half of the files on second zone, alternating files
    for (int i = numFilesPerZone; i < 2*numFilesPerZone; ++i) {
        if (i % 2 != 0) {
            ss << fileNumPrefix << i;
            string fname = ss.str();
            fname = fname.substr(fname.size() - 6);
            fname += ".sst";
            env_->DeleteFile(db_path_ + "/" + fname);
            ss.str("");
        }
    }

    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), (kPreusedZones+zones_to_write) - 1);
}

TEST_F(SmrdbSmrEnvTest, DefragmentZonesHalfErasedAlternate) {
    int zones_to_write = 20;
    const size_t kWriteSize = 299 * 1024;
    std::string write_data;

    for (size_t i = 0; i < kWriteSize; ++i) {
        write_data.append(1, static_cast<char>(i));
    }

    uint64_t fsize = kWriteSize*10;
    int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

    EXPECT_TRUE(env_->CreateDir(db_path_, true).ok());
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones);
    stringstream ss;

    WritableFile* writable_file;
    string fileNumPrefix("000000");

    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        EXPECT_TRUE(env_->NewWritableFile(db_path_ + "/" + fname, &writable_file).ok());
        for (int j = 0; j < 10; ++j) {
                writable_file->Append(write_data);
        }
        delete writable_file;
        ss.str("");
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);
    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);

    // delete third of the files, alternating files
    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        if (i % 2 == 0) {
            ss << fileNumPrefix << i;
            string fname = ss.str();
            fname = fname.substr(fname.size() - 6);
            fname += ".sst";
            env_->DeleteFile(db_path_ + "/" + fname);
            ss.str("");
        }
    }

    for (int i = 0; i < zones_to_write; ++i) {
        env_->Defragment(0);
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+(zones_to_write/2));
}

TEST_F(SmrdbSmrEnvTest, DefragmentZonesThirdErased) {
    int zones_to_write = 6;
    const size_t kWriteSize = 299 * 1024;
    std::string write_data;

    for (size_t i = 0; i < kWriteSize; ++i) {
        write_data.append(1, static_cast<char>(i));
    }

    uint64_t fsize = kWriteSize*10;
    int numFilesPerZone = (256*1024*1024)/fsize; //kWriteSize/4; //TRUNCATE(kWriteSize + 4096, 4096);

    EXPECT_TRUE(env_->CreateDir(db_path_, true).ok());
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones);
    stringstream ss;

    WritableFile* writable_file;
    string fileNumPrefix("000000");

    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        ss << fileNumPrefix << i;
        string fname = ss.str();
        fname = fname.substr(fname.size() - 6);
        fname += ".sst";
        EXPECT_TRUE(env_->NewWritableFile(db_path_ + "/" + fname, &writable_file).ok());
        for (int j = 0; j < 10; ++j) {
                writable_file->Append(write_data);
        }
        delete writable_file;
        ss.str("");
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);
    env_->Defragment(0);
    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+zones_to_write);

    // delete third of the files, alternating files
    for (int i = 4; i < zones_to_write*numFilesPerZone; ++i) {
        if (i % 3 == 0) {
            ss << fileNumPrefix << i;
            string fname = ss.str();
            fname = fname.substr(fname.size() - 6);
            fname += ".sst";
            env_->DeleteFile(db_path_ + "/" + fname);
            ss.str("");
        }
    }

    for (int i = 0; i < zones_to_write; ++i) {
        env_->Defragment(0);
    }

    EXPECT_EQ(env_->GetZonesUsed(), kPreusedZones+(zones_to_write-2));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
