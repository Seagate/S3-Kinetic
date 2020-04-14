#include "gtest/gtest.h"

#include <vector>

#include "leveldb/cache.h"
#include "util/coding.h"

using ::leveldb::DecodeFixed32;
using ::leveldb::PutFixed32;
using ::leveldb::NewLRUCache;
using ::leveldb::Slice;
using ::leveldb::Cache;

namespace com {
namespace seagate {
namespace kinetic {

// Conversions between numeric keys/values and the types expected by Cache.
static std::string EncodeKey(int k) {
    std::string result;
    PutFixed32(&result, k);
    return result;
}
static int DecodeKey(const Slice& k) {
    EXPECT_EQ((unsigned int) 4, k.size());
    return DecodeFixed32(k.data());
}
static void* EncodeValue(uintptr_t v) { return reinterpret_cast<void*>(v); }
static int DecodeValue(void* v) { return reinterpret_cast<uintptr_t>(v); }

class SmrdbCacheTest : public ::testing::Test {
 public:
    static SmrdbCacheTest* current_;

    static void Deleter(const Slice& key, void* v) {
        current_->deleted_keys_.push_back(DecodeKey(key));
        current_->deleted_values_.push_back(DecodeValue(v));
    }

    static const int kCacheSize = 1000;
    std::vector<int> deleted_keys_;
    std::vector<int> deleted_values_;
    Cache* cache_;

    SmrdbCacheTest() : cache_(NewLRUCache(kCacheSize)) {
        current_ = this;
    }

    ~SmrdbCacheTest() {
        delete cache_;
    }

    int Lookup(int key) {
        Cache::Handle* handle = cache_->Lookup(EncodeKey(key));
        const int r = (handle == NULL) ? -1 : DecodeValue(cache_->Value(handle));
        if (handle != NULL) {
            cache_->Release(handle);
        }
        return r;
    }

    void Insert(int key, int value, int charge = 1) {
        cache_->Release(cache_->Insert(EncodeKey(key), EncodeValue(value), charge,
                                                                     &SmrdbCacheTest::Deleter));
    }

    void Erase(int key) {
        cache_->Erase(EncodeKey(key));
    }
};
SmrdbCacheTest* SmrdbCacheTest::current_;

TEST_F(SmrdbCacheTest, HitAndMiss) {
    EXPECT_EQ(-1, Lookup(100));

    Insert(100, 101);
    EXPECT_EQ(101, Lookup(100));
    EXPECT_EQ(-1,  Lookup(200));
    EXPECT_EQ(-1,  Lookup(300));

    Insert(200, 201);
    EXPECT_EQ(101, Lookup(100));
    EXPECT_EQ(201, Lookup(200));
    EXPECT_EQ(-1,  Lookup(300));

    Insert(100, 102);
    EXPECT_EQ(102, Lookup(100));
    EXPECT_EQ(201, Lookup(200));
    EXPECT_EQ(-1,  Lookup(300));

    EXPECT_EQ((unsigned int) 1, deleted_keys_.size());
    EXPECT_EQ(100, deleted_keys_[0]);
    EXPECT_EQ(101, deleted_values_[0]);
}

TEST_F(SmrdbCacheTest, Erase) {
    Erase(200);
    EXPECT_EQ((unsigned int) 0, deleted_keys_.size());

    Insert(100, 101);
    Insert(200, 201);
    Erase(100);
    EXPECT_EQ(-1,  Lookup(100));
    EXPECT_EQ(201, Lookup(200));
    EXPECT_EQ((unsigned int) 1, deleted_keys_.size());
    EXPECT_EQ(100, deleted_keys_[0]);
    EXPECT_EQ(101, deleted_values_[0]);

    Erase(100);
    EXPECT_EQ(-1,  Lookup(100));
    EXPECT_EQ(201, Lookup(200));
    EXPECT_EQ((unsigned int) 1, deleted_keys_.size());
}

TEST_F(SmrdbCacheTest, EntriesArePinned) {
    Insert(100, 101);
    Cache::Handle* h1 = cache_->Lookup(EncodeKey(100));
    EXPECT_EQ(101, DecodeValue(cache_->Value(h1)));

    Insert(100, 102);
    Cache::Handle* h2 = cache_->Lookup(EncodeKey(100));
    EXPECT_EQ(102, DecodeValue(cache_->Value(h2)));
    EXPECT_EQ((unsigned int) 0, deleted_keys_.size());

    cache_->Release(h1);
    EXPECT_EQ((unsigned int) 1, deleted_keys_.size());
    EXPECT_EQ(100, deleted_keys_[0]);
    EXPECT_EQ(101, deleted_values_[0]);

    Erase(100);
    EXPECT_EQ(-1, Lookup(100));
    EXPECT_EQ((unsigned int) 1, deleted_keys_.size());

    cache_->Release(h2);
    EXPECT_EQ((unsigned int) 2, deleted_keys_.size());
    EXPECT_EQ(100, deleted_keys_[1]);
    EXPECT_EQ(102, deleted_values_[1]);
}

TEST_F(SmrdbCacheTest, EvictionPolicy) {
    Insert(100, 101);
    Insert(200, 201);

    // Frequently used entry must be kept around
    for (int i = 0; i < kCacheSize + 100; i++) {
        Insert(1000+i, 2000+i);
        EXPECT_EQ(2000+i, Lookup(1000+i));
        EXPECT_EQ(101, Lookup(100));
    }
    EXPECT_EQ(101, Lookup(100));
    EXPECT_EQ(-1, Lookup(200));
}

TEST_F(SmrdbCacheTest, HeavyEntries) {
    // Add a bunch of light and heavy entries and then count the combined
    // size of items still in the cache, which must be approximately the
    // same as the total capacity.
    const int kLight = 1;
    const int kHeavy = 10;
    int added = 0;
    int index = 0;
    while (added < 2*kCacheSize) {
        const int weight = (index & 1) ? kLight : kHeavy;
        Insert(index, 1000+index, weight);
        added += weight;
        index++;
    }

    int cached_weight = 0;
    for (int i = 0; i < index; i++) {
        const int weight = (i & 1 ? kLight : kHeavy);
        int r = Lookup(i);
        if (r >= 0) {
            cached_weight += weight;
            EXPECT_EQ(1000+i, r);
        }
    }
    EXPECT_LE(cached_weight, kCacheSize + kCacheSize/10);
}

TEST_F(SmrdbCacheTest, NewId) {
    uint64_t a = cache_->NewId();
    uint64_t b = cache_->NewId();
    EXPECT_NE(a, b);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
