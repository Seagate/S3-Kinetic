#include "gtest/gtest.h"

#include "db/version_set.h"
#include "util/logging.h"

using namespace leveldb;  //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class SmrdbFindFileTest : public ::testing::Test {
 public:
    std::vector<FileMetaData*> files_;
    bool disjoint_sorted_files_;

    SmrdbFindFileTest() : disjoint_sorted_files_(true) { }

    ~SmrdbFindFileTest() {
        for (unsigned int i = 0; i < files_.size(); i++) {
            delete files_[i];
        }
    }

    void Add(const char* smallest, const char* largest,
                     SequenceNumber smallest_seq = 100,
                     SequenceNumber largest_seq = 100) {
        FileMetaData* f = new FileMetaData;
        f->number = files_.size() + 1;
        f->smallest = InternalKey(smallest, smallest_seq, kTypeValue);
        f->largest = InternalKey(largest, largest_seq, kTypeValue);
        files_.push_back(f);
    }

    int Find(const char* key) {
        InternalKey target(key, 100, kTypeValue);
        InternalKeyComparator cmp(BytewiseComparator());
        return FindFile(cmp, files_, target.Encode());
    }

    bool Overlaps(const char* smallest, const char* largest) {
        InternalKeyComparator cmp(BytewiseComparator());
        Slice s(smallest != NULL ? smallest : "");
        Slice l(largest != NULL ? largest : "");
        return SomeFileOverlapsRange(cmp, disjoint_sorted_files_, files_,
                                                                 (smallest != NULL ? &s : NULL),
                                                                 (largest != NULL ? &l : NULL));
    }
};

TEST_F(SmrdbFindFileTest, Empty) {
    EXPECT_EQ(-1, Find("foo"));
    EXPECT_TRUE(!Overlaps("a", "z"));
    EXPECT_TRUE(!Overlaps(NULL, "z"));
    EXPECT_TRUE(!Overlaps("a", NULL));
    EXPECT_TRUE(!Overlaps(NULL, NULL));
}

TEST_F(SmrdbFindFileTest, Single) {
    Add("p", "q");
    EXPECT_EQ(-1, Find("a"));
    EXPECT_EQ(0, Find("p"));
    EXPECT_EQ(0, Find("p1"));
    EXPECT_EQ(0, Find("q"));
    EXPECT_EQ(0, Find("q1"));
    EXPECT_EQ(0, Find("z"));

    EXPECT_TRUE(!Overlaps("a", "b"));
    EXPECT_TRUE(!Overlaps("z1", "z2"));
    EXPECT_TRUE(Overlaps("a", "p"));
    EXPECT_TRUE(Overlaps("a", "q"));
    EXPECT_TRUE(Overlaps("a", "z"));
    EXPECT_TRUE(Overlaps("p", "p1"));
    EXPECT_TRUE(Overlaps("p", "q"));
    EXPECT_TRUE(Overlaps("p", "z"));
    EXPECT_TRUE(Overlaps("p1", "p2"));
    EXPECT_TRUE(Overlaps("p1", "z"));
    EXPECT_TRUE(Overlaps("q", "q"));
    EXPECT_TRUE(Overlaps("q", "q1"));

    EXPECT_TRUE(!Overlaps(NULL, "j"));
    EXPECT_TRUE(!Overlaps("r", NULL));
    EXPECT_TRUE(Overlaps(NULL, "p"));
    EXPECT_TRUE(Overlaps(NULL, "p1"));
    EXPECT_TRUE(Overlaps("q", NULL));
    EXPECT_TRUE(Overlaps(NULL, NULL));
}


TEST_F(SmrdbFindFileTest, Multiple) {
    Add("150", "200");
    Add("200", "250");
    Add("300", "350");
    Add("400", "450");
    EXPECT_EQ(-1, Find("100"));
    EXPECT_EQ(0, Find("150"));
    EXPECT_EQ(0, Find("151"));
    EXPECT_EQ(0, Find("199"));
    EXPECT_EQ(1, Find("200"));
    EXPECT_EQ(1, Find("201"));
    EXPECT_EQ(1, Find("249"));
    EXPECT_EQ(1, Find("250"));
    EXPECT_EQ(1, Find("251"));
    EXPECT_EQ(1, Find("299"));
    EXPECT_EQ(2, Find("300"));
    EXPECT_EQ(2, Find("349"));
    EXPECT_EQ(2, Find("350"));
    EXPECT_EQ(2, Find("351"));
    EXPECT_EQ(3, Find("400"));
    EXPECT_EQ(3, Find("450"));
    EXPECT_EQ(3, Find("451"));

    EXPECT_TRUE(!Overlaps("100", "149"));
    EXPECT_TRUE(!Overlaps("251", "299"));
    EXPECT_TRUE(!Overlaps("451", "500"));
    EXPECT_TRUE(!Overlaps("351", "399"));

    EXPECT_TRUE(Overlaps("100", "150"));
    EXPECT_TRUE(Overlaps("100", "200"));
    EXPECT_TRUE(Overlaps("100", "300"));
    EXPECT_TRUE(Overlaps("100", "400"));
    EXPECT_TRUE(Overlaps("100", "500"));
    EXPECT_TRUE(Overlaps("375", "400"));
    EXPECT_TRUE(Overlaps("450", "450"));
    EXPECT_TRUE(Overlaps("450", "500"));
}

TEST_F(SmrdbFindFileTest, MultipleNullBoundaries) {
    Add("150", "200");
    Add("200", "250");
    Add("300", "350");
    Add("400", "450");
    EXPECT_TRUE(!Overlaps(NULL, "149"));
    EXPECT_TRUE(!Overlaps("451", NULL));
    EXPECT_TRUE(Overlaps(NULL, NULL));
    EXPECT_TRUE(Overlaps(NULL, "150"));
    EXPECT_TRUE(Overlaps(NULL, "199"));
    EXPECT_TRUE(Overlaps(NULL, "200"));
    EXPECT_TRUE(Overlaps(NULL, "201"));
    EXPECT_TRUE(Overlaps(NULL, "400"));
    EXPECT_TRUE(Overlaps(NULL, "800"));
    EXPECT_TRUE(Overlaps("100", NULL));
    EXPECT_TRUE(Overlaps("200", NULL));
    EXPECT_TRUE(Overlaps("449", NULL));
    EXPECT_TRUE(Overlaps("450", NULL));
}

TEST_F(SmrdbFindFileTest, OverlapSequenceChecks) {
    Add("200", "200", 5000, 3000);
    EXPECT_TRUE(!Overlaps("199", "199"));
    EXPECT_TRUE(!Overlaps("201", "300"));
    EXPECT_TRUE(Overlaps("200", "200"));
    EXPECT_TRUE(Overlaps("190", "200"));
    EXPECT_TRUE(Overlaps("200", "210"));
}

TEST_F(SmrdbFindFileTest, OverlappingFiles) {
    Add("150", "600");
    Add("400", "500");
    disjoint_sorted_files_ = false;
    EXPECT_TRUE(!Overlaps("100", "149"));
    EXPECT_TRUE(!Overlaps("601", "700"));
    EXPECT_TRUE(Overlaps("100", "150"));
    EXPECT_TRUE(Overlaps("100", "200"));
    EXPECT_TRUE(Overlaps("100", "300"));
    EXPECT_TRUE(Overlaps("100", "400"));
    EXPECT_TRUE(Overlaps("100", "500"));
    EXPECT_TRUE(Overlaps("375", "400"));
    EXPECT_TRUE(Overlaps("450", "450"));
    EXPECT_TRUE(Overlaps("450", "500"));
    EXPECT_TRUE(Overlaps("450", "700"));
    EXPECT_TRUE(Overlaps("600", "700"));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
