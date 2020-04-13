#include "gtest/gtest.h"

#include "util/crc32c.h"

using ::leveldb::crc32c::Value;
using ::leveldb::crc32c::Mask;
using ::leveldb::crc32c::Unmask;
using ::leveldb::crc32c::Extend;

namespace com {
namespace seagate {
namespace kinetic {

class SmrdbCRCTest : public ::testing::Test { };

TEST_F(SmrdbCRCTest, StandardResults) {
  // From rfc3720 section B.4.
  char buf[32];

  memset(buf, 0, sizeof(buf));
  EXPECT_EQ((unsigned int) 0x8a9136aa, Value(buf, sizeof(buf)));

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ((unsigned int) 0x62a8ab43, Value(buf, sizeof(buf)));

  for (int i = 0; i < 32; i++) {
    buf[i] = i;
  }
  EXPECT_EQ((unsigned int) 0x46dd794e, Value(buf, sizeof(buf)));

  for (int i = 0; i < 32; i++) {
    buf[i] = 31 - i;
  }
  EXPECT_EQ((unsigned int) 0x113fdb5c, Value(buf, sizeof(buf)));

  unsigned char data[48] = {
    0x01, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x18,
    0x28, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_EQ((unsigned int) 0xd9963a56, Value(reinterpret_cast<char*>(data), sizeof(data)));
}

TEST_F(SmrdbCRCTest, Values) {
  EXPECT_NE(Value("a", 1), Value("foo", 3));
}

TEST_F(SmrdbCRCTest, Extend) {
  EXPECT_EQ(Value("hello world", 11),
            Extend(Value("hello ", 6), "world", 5));
}

TEST_F(SmrdbCRCTest, Mask) {
  uint32_t crc = Value("foo", 3);
  EXPECT_NE(crc, Mask(crc));
  EXPECT_NE(crc, Mask(Mask(crc)));
  EXPECT_EQ(crc, Unmask(Mask(crc)));
  EXPECT_EQ(crc, Unmask(Unmask(Mask(Mask(crc)))));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
