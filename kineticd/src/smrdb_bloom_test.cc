#include "gtest/gtest.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"

using ::leveldb::FilterPolicy;
using ::leveldb::NewBloomFilterPolicy;
using ::leveldb::EncodeFixed32;
using ::leveldb::Slice;

namespace com {
namespace seagate {
namespace kinetic {

static const int kVerbose = 0;

static Slice Key(int i, char* buffer) {
    EncodeFixed32(buffer, i);
    return Slice(buffer, sizeof(uint32_t));
}

class SmrdbBloomTest : public ::testing::Test {
 protected:
    const FilterPolicy* policy_;
    std::string filter_;
    std::vector<std::string> keys_;

    SmrdbBloomTest() : policy_(NewBloomFilterPolicy(10)) { }

    ~SmrdbBloomTest() {
        delete policy_;
    }

    void Reset() {
        keys_.clear();
        filter_.clear();
    }

    void Add(const Slice& s) {
        keys_.push_back(s.ToString());
    }

    void Build() {
        std::vector<Slice> key_slices;
        for (size_t i = 0; i < keys_.size(); i++) {
            key_slices.push_back(Slice(keys_[i]));
        }
        filter_.clear();
        policy_->CreateFilter(&key_slices[0], key_slices.size(), &filter_);
        keys_.clear();
        if (kVerbose >= 2) DumpFilter();
    }

    size_t FilterSize() const {
        return filter_.size();
    }

    void DumpFilter() {
        fprintf(stderr, "F(");
        for (size_t i = 0; i+1 < filter_.size(); i++) {
            const unsigned int c = static_cast<unsigned int>(filter_[i]);
            for (int j = 0; j < 8; j++) {
                fprintf(stderr, "%c", (c & (1 <<j)) ? '1' : '.');
            }
        }
        fprintf(stderr, ")\n");
    }

    bool Matches(const Slice& s) {
        if (!keys_.empty()) {
            Build();
        }
        return policy_->KeyMayMatch(s, filter_);
    }

    double FalsePositiveRate() {
        char buffer[sizeof(int)];  //NOLINT
        int result = 0;
        for (int i = 0; i < 10000; i++) {
            if (Matches(Key(i + 1000000000, buffer))) {
                result++;
            }
        }
        return result / 10000.0;
    }
};

TEST_F(SmrdbBloomTest, EmptyFilter) {
    EXPECT_TRUE(!Matches("hello"));
    EXPECT_TRUE(!Matches("world"));
}

TEST_F(SmrdbBloomTest, Small) {
    Add("hello");
    Add("world");
    EXPECT_TRUE(Matches("hello"));
    EXPECT_TRUE(Matches("world"));
    EXPECT_TRUE(!Matches("x"));
    EXPECT_TRUE(!Matches("foo"));
}

static int NextLength(int length) {
    if (length < 10) {
        length += 1;
    } else if (length < 100) {
        length += 10;
    } else if (length < 1000) {
        length += 100;
    } else {
        length += 1000;
    }
    return length;
}

TEST_F(SmrdbBloomTest, VaryingLengths) {
    char buffer[sizeof(int)];  //NOLINT

    // Count number of filters that significantly exceed the false positive rate
    int mediocre_filters = 0;
    int good_filters = 0;

    for (int length = 1; length <= 10000; length = NextLength(length)) {
        Reset();
        for (int i = 0; i < length; i++) {
            Add(Key(i, buffer));
        }
        Build();

        EXPECT_LE(FilterSize(), static_cast<size_t>((length * 10 / 8) + 40))
                << length;

        // All added keys must match
        for (int i = 0; i < length; i++) {
            EXPECT_TRUE(Matches(Key(i, buffer)))
                    << "Length " << length << "; key " << i;
        }

        // Check false positive rate
        double rate = FalsePositiveRate();
        if (kVerbose >= 1) {
            fprintf(stderr, "False positives: %5.2f%% @ length = %6d ; bytes = %6d\n",
                            rate*100.0, length, static_cast<int>(FilterSize()));
        }
        EXPECT_LE(rate, 0.02);   // Must not be over 2%
        if (rate > 0.0125) {
            mediocre_filters++;  // Allowed, but not too often
        } else {
            good_filters++;
        }
    }
    if (kVerbose >= 1) {
        fprintf(stderr, "Filters: %d good, %d mediocre\n",
                        good_filters, mediocre_filters);
    }
    EXPECT_LE(mediocre_filters, good_filters/5);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
