#include <stdlib.h>
#include <dirent.h>
#include "gtest/gtest.h"
#include <string>
#include <vector>

#include "cautious_file_handler.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::vector;

class CautiousFileHandlerTest : public ::testing::Test {
    protected:
    CautiousFileHandlerTest():
            cfh_("test_cautious_file_handler", "foo")
    {}

    virtual void SetUp() {
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_cautious_file_handler"));
    }

    CautiousFileHandler cfh_;
};

TEST_F(CautiousFileHandlerTest, WritesCanBeRead) {
    string orig_data = "foobar";
    ASSERT_TRUE(cfh_.Write(orig_data));

    string data;
    ASSERT_EQ(FileReadResult::OK, cfh_.Read(data));

    EXPECT_EQ("foobar", data);
}

TEST_F(CautiousFileHandlerTest, WritesDontLeaveBehindStrayTempfile) {
    string orig_data = "foobar";
    ASSERT_TRUE(cfh_.Write(orig_data));

    DIR* dir = opendir("test_cautious_file_handler");
    dirent*  pdir;

    vector<string> entries;
    while ((pdir = readdir(dir)) != NULL) {
        entries.push_back(string(pdir->d_name));
    }

    closedir(dir);

    // remove . and ..
    auto i = entries.begin();
    while (i != entries.end()) {
        if ((*i)[0] == '.') {
            i = entries. erase(i);
        } else {
            ++i;
        }
    }

    EXPECT_EQ((size_t) 1, entries.size());
    EXPECT_EQ("foo", entries[0]);
}


TEST_F(CautiousFileHandlerTest, EmptyDirReadCantOpenFile) {
    string data;
    ASSERT_EQ(FileReadResult::CANT_OPEN_FILE, cfh_.Read(data));
}

TEST_F(CautiousFileHandlerTest, DeleteMakesFileInaccessible) {
    string orig_data = "foobar";
    ASSERT_TRUE(cfh_.Write(orig_data));

    string data;
    ASSERT_EQ(FileReadResult::OK, cfh_.Read(data));

    EXPECT_EQ("foobar", data);

    ASSERT_TRUE(cfh_.Delete());

    ASSERT_EQ(FileReadResult::CANT_OPEN_FILE, cfh_.Read(data));
}

TEST_F(CautiousFileHandlerTest, DeleteLeavesStoreInUsableState) {
    string orig_data = "foobar";
    ASSERT_TRUE(cfh_.Write(orig_data));

    string data;
    ASSERT_EQ(FileReadResult::OK, cfh_.Read(data));
    EXPECT_EQ("foobar", data);

    ASSERT_TRUE(cfh_.Delete());
    ASSERT_EQ(FileReadResult::CANT_OPEN_FILE, cfh_.Read(data));

    ASSERT_TRUE(cfh_.Write(orig_data));
    ASSERT_EQ(FileReadResult::OK, cfh_.Read(data));
    EXPECT_EQ("foobar", data);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
