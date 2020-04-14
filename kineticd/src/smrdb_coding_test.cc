#include "gtest/gtest.h"

#include "util/coding.h"

using ::leveldb::PutFixed32;
using ::leveldb::PutFixed64;
using ::leveldb::PutVarint32;
using ::leveldb::PutVarint64;
using ::leveldb::DecodeFixed32;
using ::leveldb::DecodeFixed64;
using ::leveldb::GetVarint64Ptr;
using ::leveldb::GetVarint32Ptr;
using ::leveldb::VarintLength;
using ::leveldb::Slice;

namespace com {
namespace seagate {
namespace kinetic {

class SmrdbCodingTest : public ::testing::Test { };

TEST_F(SmrdbCodingTest, Fixed32) {
  std::string s;
  for (uint32_t v = 0; v < 100000; v++) {
    PutFixed32(&s, v);
  }

  const char* p = s.data();
  for (uint32_t v = 0; v < 100000; v++) {
    uint32_t actual = DecodeFixed32(p);
    EXPECT_EQ(v, actual);
    p += sizeof(uint32_t);
  }
}

TEST_F(SmrdbCodingTest, Fixed64) {
  std::string s;
  for (int power = 0; power <= 63; power++) {
    uint64_t v = static_cast<uint64_t>(1) << power;
    PutFixed64(&s, v - 1);
    PutFixed64(&s, v + 0);
    PutFixed64(&s, v + 1);
  }

  const char* p = s.data();
  for (int power = 0; power <= 63; power++) {
    uint64_t v = static_cast<uint64_t>(1) << power;
    uint64_t actual;
    actual = DecodeFixed64(p);
    EXPECT_EQ(v-1, actual);
    p += sizeof(uint64_t);

    actual = DecodeFixed64(p);
    EXPECT_EQ(v+0, actual);
    p += sizeof(uint64_t);

    actual = DecodeFixed64(p);
    EXPECT_EQ(v+1, actual);
    p += sizeof(uint64_t);
  }
}

// Test that encoding routines generate little-endian encodings
TEST_F(SmrdbCodingTest, EncodingOutput) {
  std::string dst;
  PutFixed32(&dst, 0x04030201);
  EXPECT_EQ((unsigned int) 4, dst.size());
  EXPECT_EQ(0x01, static_cast<int>(dst[0]));
  EXPECT_EQ(0x02, static_cast<int>(dst[1]));
  EXPECT_EQ(0x03, static_cast<int>(dst[2]));
  EXPECT_EQ(0x04, static_cast<int>(dst[3]));

  dst.clear();
  PutFixed64(&dst, 0x0807060504030201ull);
  EXPECT_EQ((unsigned int) 8, dst.size());
  EXPECT_EQ(0x01, static_cast<int>(dst[0]));
  EXPECT_EQ(0x02, static_cast<int>(dst[1]));
  EXPECT_EQ(0x03, static_cast<int>(dst[2]));
  EXPECT_EQ(0x04, static_cast<int>(dst[3]));
  EXPECT_EQ(0x05, static_cast<int>(dst[4]));
  EXPECT_EQ(0x06, static_cast<int>(dst[5]));
  EXPECT_EQ(0x07, static_cast<int>(dst[6]));
  EXPECT_EQ(0x08, static_cast<int>(dst[7]));
}

TEST_F(SmrdbCodingTest, Varint32) {
  std::string s;
  for (uint32_t i = 0; i < (32 * 32); i++) {
    uint32_t v = (i / 32) << (i % 32);
    PutVarint32(&s, v);
  }

  const char* p = s.data();
  const char* limit = p + s.size();
  for (uint32_t i = 0; i < (32 * 32); i++) {
    uint32_t expected = (i / 32) << (i % 32);
    uint32_t actual;
    const char* start = p;
    p = GetVarint32Ptr(p, limit, &actual);
    EXPECT_TRUE(p != NULL);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(VarintLength(actual), p - start);
  }
  EXPECT_EQ(p, s.data() + s.size());
}

TEST_F(SmrdbCodingTest, Varint64) {
  // Construct the list of values to check
  std::vector<uint64_t> values;
  // Some special values
  values.push_back(0);
  values.push_back(100);
  values.push_back(~static_cast<uint64_t>(0));
  values.push_back(~static_cast<uint64_t>(0) - 1);
  for (uint32_t k = 0; k < 64; k++) {
    // Test values near powers of two
    const uint64_t power = 1ull << k;
    values.push_back(power);
    values.push_back(power-1);
    values.push_back(power+1);
  }

  std::string s;
  for (size_t i = 0; i < values.size(); i++) {
    PutVarint64(&s, values[i]);
  }

  const char* p = s.data();
  const char* limit = p + s.size();
  for (size_t i = 0; i < values.size(); i++) {
    EXPECT_TRUE(p < limit);
    uint64_t actual;
    const char* start = p;
    p = GetVarint64Ptr(p, limit, &actual);
    EXPECT_TRUE(p != NULL);
    EXPECT_EQ(values[i], actual);
    EXPECT_EQ(VarintLength(actual), p - start);
  }
  EXPECT_EQ(p, limit);
}

TEST_F(SmrdbCodingTest, Varint32Overflow) {
  uint32_t result;
  std::string input("\x81\x82\x83\x84\x85\x11");
  EXPECT_TRUE(GetVarint32Ptr(input.data(), input.data() + input.size(), &result)
              == NULL);
}

TEST_F(SmrdbCodingTest, Varint32Truncation) {
  uint32_t large_value = (1u << 31) + 100;
  std::string s;
  PutVarint32(&s, large_value);
  uint32_t result;
  for (size_t len = 0; len < s.size() - 1; len++) {
    EXPECT_TRUE(GetVarint32Ptr(s.data(), s.data() + len, &result) == NULL);
  }
  EXPECT_TRUE(GetVarint32Ptr(s.data(), s.data() + s.size(), &result) != NULL);
  EXPECT_EQ(large_value, result);
}

TEST_F(SmrdbCodingTest, Varint64Overflow) {
  uint64_t result;
  std::string input("\x81\x82\x83\x84\x85\x81\x82\x83\x84\x85\x11");
  EXPECT_TRUE(GetVarint64Ptr(input.data(), input.data() + input.size(), &result)
              == NULL);
}

TEST_F(SmrdbCodingTest, Varint64Truncation) {
  uint64_t large_value = (1ull << 63) + 100ull;
  std::string s;
  PutVarint64(&s, large_value);
  uint64_t result;
  for (size_t len = 0; len < s.size() - 1; len++) {
    EXPECT_TRUE(GetVarint64Ptr(s.data(), s.data() + len, &result) == NULL);
  }
  EXPECT_TRUE(GetVarint64Ptr(s.data(), s.data() + s.size(), &result) != NULL);
  EXPECT_EQ(large_value, result);
}

TEST_F(SmrdbCodingTest, Strings) {
  std::string s;
  PutLengthPrefixedSlice(&s, Slice(""));
  PutLengthPrefixedSlice(&s, Slice("foo"));
  PutLengthPrefixedSlice(&s, Slice("bar"));
  PutLengthPrefixedSlice(&s, Slice(std::string(200, 'x')));

  Slice input(s);
  Slice v;
  EXPECT_TRUE(GetLengthPrefixedSlice(&input, &v));
  EXPECT_EQ("", v.ToString());
  EXPECT_TRUE(GetLengthPrefixedSlice(&input, &v));
  EXPECT_EQ("foo", v.ToString());
  EXPECT_TRUE(GetLengthPrefixedSlice(&input, &v));
  EXPECT_EQ("bar", v.ToString());
  EXPECT_TRUE(GetLengthPrefixedSlice(&input, &v));
  EXPECT_EQ(std::string(200, 'x'), v.ToString());
  EXPECT_EQ("", input.ToString());
}

} // namespace kinetic
} // namespace seagate
} // namespace com
