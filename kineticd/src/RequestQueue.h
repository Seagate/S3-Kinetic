/*
 * RequestQueue.h
 *
 *  Created on: Jan 9, 2018
 *      Author: tri
 */

#ifndef REQUEST_QUEUE_H_ // NOLINT
#define REQUEST_QUEUE_H_ // NOLINT

#include <queue>
#include <assert.h> // NOLINT

#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "connection.h"


using namespace std; // NOLINT

using namespace leveldb::port;  // NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class RequestQueue {
  public:
    static const int MAX_SIZE = 210;
    static constexpr uint64_t REMAINED_TIMEOUT_THRESHOLD = 100*1000*1000;  // nanoseconds

  public:
    explicit RequestQueue(int priority): cv_(&mu_) {
        priority_ = priority;
    }

    virtual ~RequestQueue() {
    }

    void enqueue(ConnectionRequestResponse* req) {
        MutexLock lock(&mu_);
        assert(req->getPriority() == priority_);  // NOLINT
        req->setExpiredTime();
        requests_.push(req);
    }

    ConnectionRequestResponse* dequeue() {
        MutexLock lock(&mu_);
        assert(requests_.size() > 0);  // NOLINT
        ConnectionRequestResponse* req = requests_.top();
        requests_.pop();
        return req;
    }

    ConnectionRequestResponse* peekTop() {
        MutexLock lock(&mu_);
        if (requests_.size() == 0) {
            return NULL;
        }
        return requests_.top();
    }

    bool operator <(const RequestQueue& rhs) const {
        // Queue with command has higher priority than empty queue
        if (rhs.size() == 0 || (size() == 0 && rhs.size() == 0)) {
            return false;
        } else if (size() == 0) {
            return true;
        }

        // Queue with top command with remained timeout smaller a threshold
        // has higher priority.  Times are converted to milliseconds.
        struct timespec timeNow;
        clock_gettime(CLOCK_MONOTONIC, &timeNow);
        uint64_t dTimeNow = timeNow.tv_sec*1000*1000*1000 + timeNow.tv_nsec;
        uint64_t thisReqRemainedTimeout = requests_.top()->getExpiredTime() - dTimeNow;
        uint64_t rhsReqRemainedTimeout = rhs.requests_.top()->getExpiredTime() - dTimeNow;
        if (thisReqRemainedTimeout < REMAINED_TIMEOUT_THRESHOLD &&
            rhsReqRemainedTimeout < REMAINED_TIMEOUT_THRESHOLD) {
            if (thisReqRemainedTimeout > rhsReqRemainedTimeout) {
               return true;  // this has lower priority than rhs
            } else if (thisReqRemainedTimeout < rhsReqRemainedTimeout) {
               return false; // this has higher priority
            }
            return (priority() < rhs.priority());
        } else if (thisReqRemainedTimeout < REMAINED_TIMEOUT_THRESHOLD) {
            return false;  // this has higher priority
        } else if (rhsReqRemainedTimeout < REMAINED_TIMEOUT_THRESHOLD) {
            return true;   // this has lower priority than rhs
        }
        return (priority() < rhs.priority());
    }

    int priority() const {
        return priority_;
    }

    int size() const {
        return requests_.size();
    }

  private:
    priority_queue<ConnectionRequestResponse, vector<ConnectionRequestResponse*>,
                   ConnectionRequestResponse::Comparator> requests_;
    int priority_;
    Mutex mu_;
    CondVar cv_;
};


} // namespace kinetic
} // namespace seagate
} // namespace com

#endif // REQUEST_QUEUE_H_ // NOLINT
