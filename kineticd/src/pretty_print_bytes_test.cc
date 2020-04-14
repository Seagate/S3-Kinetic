#include "gtest/gtest.h"
#include "pretty_print_bytes.h"

using com::seagate::kinetic::PrettyPrintBytes;

TEST(PrettyPrintBytesTest, HandlesEmptyString) {
    EXPECT_EQ("<>", PrettyPrintBytes(""));
}

TEST(PrettyPrintBytesTest, HandlesAsciiString) {
    EXPECT_EQ("<asdf>", PrettyPrintBytes("asdf"));
}

TEST(PrettyPrintBytesTest, HandlesBinaryString) {
    char bytes[] = {0x01, 0x02, 0x03, (char)0xFF};
    EXPECT_EQ("<\\x01\\x02\\x03\\xFF>", PrettyPrintBytes(std::string(bytes, 4)));
}

TEST(PrettyPrintBytesTest, HandlesMixedAsciiBinaryString) {
    char bytes[] = {'D', 'e', (char)0xAD};
    EXPECT_EQ("<De\\xAD>", PrettyPrintBytes(std::string(bytes, 3)));
}

TEST(PrettyPrintBytesTest, HandlesAsciiWithNullsString) {
    char bytes[] = {'Z', 0, 'e', 'r', 'o'};
    EXPECT_EQ("<Z\\x00ero>", PrettyPrintBytes(std::string(bytes, 5)));
}

TEST(PrettyPrintBytesTest, TruncatesLongInput) {
    std::string long_string;
    for (int i = 0; i < 1024; i++) {
        long_string.append("X");
    }
    EXPECT_EQ("<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX>",
        PrettyPrintBytes(long_string));
}
