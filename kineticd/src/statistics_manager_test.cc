#include "gtest/gtest.h"

#include "kinetic.pb.h"
#include "statistics_manager.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_MessageType;
using proto::Command_MessageType_GET;
using proto::Command_MessageType_IsValid;

TEST(StatisticsManagerTest, InitializeCountsToZero) {
    StatisticsManager counters;

    std::map<std::string, uint64_t> counts = counters.GetCounts();

    for (std::map<std::string, uint64_t>::iterator it = counts.begin(); it != counts.end(); it++) {
        EXPECT_EQ(0U, it->second) << "Wrong count for " << it->first;
    }
}

TEST(StatisticsManagerTest, AllowIncrementingValidOperation) {
    StatisticsManager counters;

    counters.IncrementOperationCount(Command_MessageType_GET);
    counters.IncrementOperationCount(Command_MessageType_GET);
    counters.IncrementOperationCount(Command_MessageType_GET);

    std::map<std::string, uint64_t> counts = counters.GetCounts();

    ASSERT_EQ(3U, counts["GET"]);
}

TEST(StatisticsManagerTest, AllowIncrementingByACustom) {
    StatisticsManager counters;

    counters.IncrementOperationCount(Command_MessageType_GET, 3);
    counters.IncrementOperationCount(Command_MessageType_GET, 4);
    counters.IncrementOperationCount(Command_MessageType_GET, 5);

    std::map<std::string, uint64_t> counts = counters.GetCounts();

    ASSERT_EQ(12U, counts["GET"]);
}

TEST(StatisticsManagerTest, TreatsInvalidOperationTypeAsUnknown) {
    StatisticsManager counters;

    counters.IncrementOperationCount((Command_MessageType)123);
    counters.IncrementOperationCount((Command_MessageType)123);
    counters.IncrementOperationCount((Command_MessageType)123);

    std::map<std::string, uint64_t> counts = counters.GetCounts();

    ASSERT_EQ(3U, counts["UNKNOWN"]);
}

TEST(StatisticsManagerTest, AllowIncrementingAllValues) {
    StatisticsManager counters;

    for (int message_type = Command_MessageType_MessageType_MIN;
        message_type <= Command_MessageType_MessageType_MAX;
        message_type++) {
        for (int i = 0; i < message_type; i++) {
            if (Command_MessageType_IsValid(message_type)) {
                counters.IncrementOperationCount((Command_MessageType)message_type);
            }
        }
    }

    std::map<std::string, uint64_t> counts = counters.GetCounts();

    EXPECT_EQ(2U, counts["GET"]);
    EXPECT_EQ(1U, counts["GET_RESPONSE"]);
    EXPECT_EQ(4U, counts["PUT"]);
    EXPECT_EQ(3U, counts["PUT_RESPONSE"]);
    EXPECT_EQ(6U, counts["DELETE"]);
    EXPECT_EQ(5U, counts["DELETE_RESPONSE"]);
    EXPECT_EQ(8U, counts["GETNEXT"]);
    EXPECT_EQ(7U, counts["GETNEXT_RESPONSE"]);
    EXPECT_EQ(10U, counts["GETPREVIOUS"]);
    EXPECT_EQ(9U, counts["GETPREVIOUS_RESPONSE"]);
    EXPECT_EQ(12U, counts["GETKEYRANGE"]);
    EXPECT_EQ(11U, counts["GETKEYRANGE_RESPONSE"]);
    EXPECT_EQ(16U, counts["GETVERSION"]);
    EXPECT_EQ(15U, counts["GETVERSION_RESPONSE"]);
    EXPECT_EQ(22U, counts["SETUP"]);
    EXPECT_EQ(21U, counts["SETUP_RESPONSE"]);
    EXPECT_EQ(24U, counts["GETLOG"]);
    EXPECT_EQ(23U, counts["GETLOG_RESPONSE"]);
    EXPECT_EQ(26U, counts["SECURITY"]);
    EXPECT_EQ(25U, counts["SECURITY_RESPONSE"]);
    EXPECT_EQ(28U, counts["PEER2PEERPUSH"]);
    EXPECT_EQ(27U, counts["PEER2PEERPUSH_RESPONSE"]);

    EXPECT_EQ(counts.end(), counts.find("STEALER"));
    EXPECT_EQ(counts.end(), counts.find("DONOR"));
    EXPECT_EQ(counts.end(), counts.find("STEALER_RESPONSE"));
    EXPECT_EQ(counts.end(), counts.find("DONOR_RESPONSE"));
}

pthread_mutex_t concurrency_test_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t concurrency_test_cond = PTHREAD_COND_INITIALIZER;
volatile bool ready = false;
const unsigned int operation_count = 100000;
void *IncrementCounters(void* counters_ptr) {
    // Wait until the test indicates that all threads are ready. Otherwise, all the threads
    // might finish before a single join call and there might not be any concurrency tests at all
    pthread_mutex_lock(&concurrency_test_mutex);
    while (!ready) {
        pthread_cond_wait(&concurrency_test_cond, &concurrency_test_mutex);
    }
    pthread_mutex_unlock(&concurrency_test_mutex);
    StatisticsManager* counters = (StatisticsManager*)counters_ptr;

    for (unsigned int i = 0; i < operation_count; i++) {
        counters->IncrementOperationCount((Command_MessageType)123);
    }

    return NULL;
}

TEST(StatisticsManagerTest, IncrementingWorksConcurrently) {
    StatisticsManager counters;

    pthread_mutex_init(&concurrency_test_mutex, NULL);
    pthread_cond_init(&concurrency_test_cond, NULL);

    // Create some threads each of which increments counters many times
    const int thread_count = 100;
    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; i++) {
        ASSERT_EQ(0, pthread_create(&threads[i], NULL, IncrementCounters, (void*)&counters))
            << "Error starting thread #" << i;
    }

    pthread_mutex_lock(&concurrency_test_mutex);
    ready = true;
    pthread_cond_broadcast(&concurrency_test_cond);
    pthread_mutex_unlock(&concurrency_test_mutex);

    for (int i = 0; i < thread_count; i++) {
        ASSERT_EQ(0, pthread_join(threads[i], NULL)) << "Error joining thread #" << i;
    }

    std::map<std::string, uint64_t> counts = counters.GetCounts();

    EXPECT_EQ(operation_count * thread_count, counts["UNKNOWN"]);
}

TEST(StatisticsManagerTest, AllowsGettingCount) {
    StatisticsManager counters;

    EXPECT_EQ(0U, counters.GetOperationCount(Command_MessageType_GET));

    counters.IncrementOperationCount(Command_MessageType_GET, 123);

    EXPECT_EQ(123U, counters.GetOperationCount(Command_MessageType_GET));
}


} // namespace kinetic
} // namespace seagate
} // namespace com
