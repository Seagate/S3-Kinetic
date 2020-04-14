#include "gtest/gtest.h"

#include "log_ring_buffer.h"

namespace com {
namespace seagate {
namespace kinetic {

TEST(LogRingBufferTest, SizeIsZeroWhenEmpty) {
    LogRingBuffer::Instance()->changeCapacity(4);

    EXPECT_EQ((uint32_t) 4, LogRingBuffer::Instance()->capacity());
    EXPECT_EQ((uint32_t) 0, LogRingBuffer::Instance()->size());
}

TEST(LogRingBufferTest, SizeIncreasesWhenPushed) {
    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    tm t;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full", "base", 22, &t, "abcdef", 4, (pid_t) 1);

    EXPECT_EQ((uint32_t) 1, LogRingBuffer::Instance()->size());
}

TEST(LogRingBufferTest, SizeStopsAtCapacity) {
    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    tm t;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full1", "base1", 21, &t, "1abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full2", "base2", 22, &t, "2abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full3", "base3", 23, &t, "3abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full4", "base4", 24, &t, "4abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full5", "base5", 25, &t, "5abcdef", 4, (pid_t) 1);

    EXPECT_EQ((uint32_t) 4, LogRingBuffer::Instance()->size());
}

TEST(LogRingBufferTest, CanCopyIntoVector) {
    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    tm t;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full", "base", 22, &t, "abcdef", 4, (pid_t) 1);

    ::std::vector<LogRingBufferEntry> dest;
    LogRingBuffer::Instance()->copyBuffer(dest);
    EXPECT_EQ((size_t) 1, dest.size());

    EXPECT_EQ("abcd", dest.at(0).message);
}

TEST(LogRingBufferTest, CopyIntoVectorAfterWraparound) {
    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    tm t;
    t.tm_sec = 1;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full1", "base1", 21, &t, "1abcdef", 4, (pid_t) 1);
    t.tm_sec = 2;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full2", "base2", 22, &t, "2abcdef", 4, (pid_t) 1);
    t.tm_sec = 3;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full3", "base3", 23, &t, "3abcdef", 4, (pid_t) 1);
    t.tm_sec = 4;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full4", "base4", 24, &t, "4abcdef", 4, (pid_t) 1);
    t.tm_sec = 5;
    LogRingBuffer::Instance()->push(
        ::google::WARNING, "full5", "base5", 25, &t, "5abcdef", 4, (pid_t) 2);

    ::std::vector<LogRingBufferEntry> dest;
    LogRingBuffer::Instance()->copyBuffer(dest);
    EXPECT_EQ((size_t) 4, dest.size());

    EXPECT_EQ("2abc", dest.at(0).message);
    EXPECT_EQ("3abc", dest.at(1).message);
    EXPECT_EQ("4abc", dest.at(2).message);
    EXPECT_EQ("5abc", dest.at(3).message);

    // ensure everything was overwritten
    EXPECT_EQ(::google::WARNING, dest.at(3).severity);
    EXPECT_EQ("base5", dest.at(3).base_filename);
    EXPECT_EQ(25, dest.at(3).line);
    EXPECT_EQ(5, dest.at(3).tm_time.tm_sec);
    EXPECT_EQ(2, dest.at(3).thread_id);
}

TEST(LogRingBufferTest, WraparoundDoesntAffectDataCopiedIntoVector) {
    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    tm t;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full1", "base1", 21, &t, "1abcdef", 4, (pid_t) 1);

    ::std::vector<LogRingBufferEntry> dest;
    LogRingBuffer::Instance()->copyBuffer(dest);
    EXPECT_EQ((size_t) 1, dest.size());
    EXPECT_EQ("1abc", dest.at(0).message);

    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full2", "base2", 22, &t, "2abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full3", "base3", 23, &t, "3abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full4", "base4", 24, &t, "4abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::WARNING, "full5", "base5", 25, &t, "5abcdef", 4, (pid_t) 1);

    EXPECT_EQ("1abc", dest.at(0).message);
}


TEST(LogRingBufferTest, CopyIntoVectorAfterCompleteWraparound) {
    LogRingBuffer::Instance()->clearBuffer();
    LogRingBuffer::Instance()->changeCapacity(4);

    tm t;
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full1", "base1", 21, &t, "1abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full2", "base2", 22, &t, "2abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full3", "base3", 23, &t, "3abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full4", "base4", 24, &t, "4abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full5", "base5", 25, &t, "5abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full6", "base6", 25, &t, "6abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full7", "base7", 25, &t, "7abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full8", "base8", 25, &t, "8abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full9", "base9", 25, &t, "9abcdef", 4, (pid_t) 1);
    LogRingBuffer::Instance()->push(
        ::google::ERROR, "full10", "base10", 25, &t, "10abcdef", 4, (pid_t) 1);

    ::std::vector<LogRingBufferEntry> dest;
    LogRingBuffer::Instance()->copyBuffer(dest);
    EXPECT_EQ((size_t) 4, dest.size());

    EXPECT_EQ("7abc", dest.at(0).message);
    EXPECT_EQ("8abc", dest.at(1).message);
    EXPECT_EQ("9abc", dest.at(2).message);
    EXPECT_EQ("10ab", dest.at(3).message);
}


} // namespace kinetic
} // namespace seagate
} // namespace com
