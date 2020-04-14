#ifndef KINETIC_THREADSAFE_BLOCKING_QUEUE_H_
#define KINETIC_THREADSAFE_BLOCKING_QUEUE_H_

#include <list>
#include <mutex>
#include <condition_variable>

namespace com {
namespace seagate {
namespace kinetic {

using std::list;
using std::unique_lock;
using std::mutex;
using std::condition_variable;

/**
* This is a Threadsafe, Blocking, FIFO Queue with a max depth.
* Adding to the queue responds immediately, if the queue is full it will return
* false.
* Removing from the queue blocks until there is an item in the queue. If the queue
* is non empty, BlockingRemove will return immediately.
*
* This implementation is minimal, supporting only the current needs.
*/
template <typename T> class ThreadsafeBlockingQueue {
    public:
    explicit ThreadsafeBlockingQueue(size_t queue_depth) :
            queue_depth_(queue_depth),
            interrupted_(false) {}

    /**
    * Returns true if added successfully.
    * Returns false when the queue is at max depth, indicating
    * that it was not added to the queue.
    */
    bool Add(T item) {
        {
            unique_lock<mutex> lk(mutex_);
            // Obtain the mutex before checking size
            // and acting on that info
            if (queue_.size() >= queue_depth_) {
                return false;
            }
            queue_.push_back(item);
        }
        condv_.notify_one();
        return true;
    }

    /**
    * Blocks until an item is ready. If the queue is non-empty,
    * this will return immediately. If it is empty, it will wait
    * until a an item is added.
    * A false return value indicates an interrupt occurred.
    */
    bool BlockingRemove(T& item) {
        unique_lock<mutex> lk(mutex_);
        condv_.wait(lk, [&] {return queue_.size() != 0 || interrupted_;} );

        if (interrupted_) {
            return false;
        }

        item = queue_.front();
        queue_.pop_front();
        return true;
    }

    /**
    * Interrupts all callers waiting on blocking methods.
    */
    void InterruptAll() {
        interrupted_ = true;
        condv_.notify_all();
    }

    private:
    list<T> queue_;
    size_t queue_depth_;
    mutex mutex_;
    condition_variable condv_;
    bool interrupted_;
};
} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_THREADSAFE_BLOCKING_QUEUE_H_

