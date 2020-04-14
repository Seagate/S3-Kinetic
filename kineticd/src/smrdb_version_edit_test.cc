#include "gtest/gtest.h"

#include "db/version_edit.h"

using namespace leveldb;  //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

static void TestEncodeDecode(const VersionEdit& edit) {
    std::string encoded, encoded2;
    edit.EncodeTo(&encoded);
    VersionEdit parsed;
    Status s = parsed.DecodeFrom(encoded);
    EXPECT_TRUE(s.ok()) << s.ToString();
    parsed.EncodeTo(&encoded2);
    EXPECT_EQ(encoded, encoded2);
}

class SmrdbVersionEditTest : public ::testing::Test {};

TEST_F(SmrdbVersionEditTest, EncodeDecode) {
    static const uint64_t kBig = 1ull << 50;

    VersionEdit edit;
    for (int i = 0; i < 4; i++) {
        TestEncodeDecode(edit);
        edit.AddFile(3, kBig + 300 + i, kBig + 400 + i,
                                 InternalKey("foo", kBig + 500 + i, kTypeValue),
                                 InternalKey("zoo", kBig + 600 + i, kTypeDeletion));
        edit.DeleteFile(4, kBig + 700 + i);
        edit.SetCompactPointer(i, InternalKey("x", kBig + 900 + i, kTypeValue));
    }

    edit.SetComparatorName("foo");
    edit.SetLogNumber(kBig + 100);
    edit.SetNextFile(kBig + 200);
    edit.SetLastSequence(kBig + 1000);
    TestEncodeDecode(edit);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
