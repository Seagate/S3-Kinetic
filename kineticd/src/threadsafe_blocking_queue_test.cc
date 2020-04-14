#include "gtest/gtest.h"
#include <thread>

#include "threadsafe_blocking_queue.h"

namespace com {
namespace seagate {
namespace kinetic {

class ThreadsafeBockingQueueTest : public ::testing::Test {};

TEST_F(ThreadsafeBockingQueueTest, BlockingRemoveWaitsForItemInQueue) {
    ThreadsafeBlockingQueue<int> test_queue(10);

    int actual_result = 0;
    int expected_result = 100;

    std::thread remove_thread([&] { ASSERT_TRUE(test_queue.BlockingRemove(actual_result)); });
    // The remove thread has started, but actual_result should not have been updated
    // because there is nothing added to the queue yet:
    ASSERT_NE(expected_result, actual_result);

    bool add_succeeded = false;
    std::thread add_thread([&] { add_succeeded = test_queue.Add(expected_result); });
    add_thread.join();
    // The add needs to have succeeded
    ASSERT_TRUE(add_succeeded);

    // When the remove thread finishes, we should get the actual result
    remove_thread.join();
    ASSERT_EQ(expected_result, actual_result);
}

TEST_F(ThreadsafeBockingQueueTest, AddReturnsFalseWhenQueueFull) {
    size_t depth = 10;
    ThreadsafeBlockingQueue<int> test_queue(depth);
    for (size_t i = 0; i < depth; i++) {
        ASSERT_TRUE(test_queue.Add(1));
    }
    ASSERT_FALSE(test_queue.Add(1));
}

TEST_F(ThreadsafeBockingQueueTest, ReturnReturnsFalseWhenInterrupted) {
    size_t depth = 10;
    ThreadsafeBlockingQueue<int> test_queue(depth);
    ASSERT_TRUE(test_queue.Add(1));
    ASSERT_TRUE(test_queue.Add(1));

    int result;
    ASSERT_TRUE(test_queue.BlockingRemove(result));

    test_queue.InterruptAll();
    ASSERT_FALSE(test_queue.BlockingRemove(result));
}

// We don't want a long-running exhaustive test for concurrency
// and in fact such a test isn't really "possible". This is
// just a little peace of mind that the queue seems to do the right thing
// in a concurrent situation.
TEST_F(ThreadsafeBockingQueueTest, ConcurrentAccessSeemsToWork) {
    // The test would be overly complicated if the queue depth was
    // smaller than the number of items, because our producer threads
    // would have to do complicated retry/wait logic
    const int total_number_of_items = 100000;
    size_t depth(total_number_of_items);
    ThreadsafeBlockingQueue<int> test_queue(depth);

    bool item_status_array[total_number_of_items] = {false};
    std::mutex status_array_mutex;

    int producer_operations_remaining = total_number_of_items;
    int consumer_operations_remaining = total_number_of_items;

    // Super important we use [&] to capture variables by ref
    auto producer_thread_fn = [&] {
      int my_operation_offset;
      while ((my_operation_offset = __sync_fetch_and_add(&producer_operations_remaining, -1))
              > 0) {
          ASSERT_TRUE(test_queue.Add(total_number_of_items - my_operation_offset));
      }
    };

    auto consumer_thread_fn = [&] {
        while (__sync_fetch_and_add(&consumer_operations_remaining, -1) > 0) {
            int successful_item;
            ASSERT_TRUE(test_queue.BlockingRemove(successful_item));
            // Disallow concurrent access to the status array
            std::lock_guard<std::mutex> status_writer_guard(status_array_mutex);
            item_status_array[successful_item] = true;
        }
    };


    // Spawn some threads to produce and consume
    const int num_consumers = 5;
    const int num_producers = 5;

    std::thread producers[num_producers];
    for (int i = 0; i < num_producers; ++i) {
        producers[i] = std::thread(producer_thread_fn);
    }
    std::thread consumers[num_consumers];
    for (int i = 0; i < num_consumers; ++i) {
        consumers[i] = std::thread(consumer_thread_fn);
    }

    // Wait for all the threads to finish
    for (int i = 0; i < num_producers; ++i) {
        producers[i].join();
    }

    for (int i = 0; i < num_consumers; ++i) {
        consumers[i].join();
    }

    // Make sure that each item we expect made it through the queue
    for (int i = 0; i < total_number_of_items; ++i) {
        ASSERT_TRUE(item_status_array[i]) << "Failed for item <" << i << ">";
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
