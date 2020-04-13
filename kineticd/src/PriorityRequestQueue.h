/*
 * PriorityRequestQueue.h
 *
 *  Created on: Jan 17, 2018
 *      Author: tri
 */

#ifndef PRIORITY_REQUEST_QUEUE_H_ // NOLINT
#define PRIORITY_REQUEST_QUEUE_H_ // NOLINT

#include <unordered_map>
#include <queue>

#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "RequestQueue.h"

using namespace std; // NOLINT

using namespace leveldb::port;  // NOLINT
using namespace com::seagate::kinetic; // NOLINT
using namespace com::seagate::kinetic::proto; // NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class PriorityRequestQueue {
  public:
    PriorityRequestQueue(): cv_(&mu_) {
        for (int i = Command_Priority::Command_Priority_LOWEST; i <= Command_Priority::Command_Priority_HIGHEST; ++i) {
            RequestQueue* reqQueue = new RequestQueue(i);
            priorityToRequestMap_[i] = reqQueue;
        }
    }

    virtual ~PriorityRequestQueue() {
        unordered_map<int, RequestQueue*>::iterator it = priorityToRequestMap_.begin();
        for (; it != priorityToRequestMap_.end(); ++it) {
            delete it->second;
        }
    }

    void enqueue(ConnectionRequestResponse* req) {
        assert(req); // NOLINT
        MutexLock lock(&mu_);
        RequestQueue* requestQ = priorityToRequestMap_[req->command()->header().priority()];
        while (requestQ->size() >= RequestQueue::MAX_SIZE) {
            cv_.Wait();
        }
        requestQ->enqueue(req);
        cv_.Signal();
    }

    ConnectionRequestResponse* dequeue() {
        MutexLock lock(&mu_);
        while (!hasRequest()) {
           cv_.Wait();
        }
        RequestQueue* reqQueue = top();
        assert(reqQueue->size() > 0); // NOLINT
        ConnectionRequestResponse* req = reqQueue->dequeue();
        cv_.Signal();
        return req;
    }

    ConnectionRequestResponse* peekTop() {
        MutexLock lock(&mu_);
        if (!hasRequest()) {
           return NULL;
        }
        RequestQueue* reqQueue = top();
        if (reqQueue->size() == 0) {
            return NULL;
        }
        return reqQueue->peekTop();
    }

    int numberOfRequests();

    bool hasRequest() {
        unordered_map<int, RequestQueue*>::iterator it = priorityToRequestMap_.begin();
        for (; it != priorityToRequestMap_.end(); ++it) {
            if (it->second->size()) {
               return true;
            }
        }
        return false;
    }

  private:
    RequestQueue* top() {
        RequestQueue* topQueue = NULL;
        do {
            unordered_map<int, RequestQueue*>::iterator it = priorityToRequestMap_.begin();
            topQueue = it->second;

            for (; it != priorityToRequestMap_.end(); ++it) {
                RequestQueue* reqQ = it->second;
                if (*topQueue < *reqQ) {  // topQueue has lower priority than reqQ
                    topQueue = reqQ;
                }
            }
        } while (topQueue == NULL);
        return topQueue;
    }

  private:
    unordered_map<int, RequestQueue*> priorityToRequestMap_;

    Mutex mu_;
    CondVar cv_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif // PRIORITY_REQUEST_QUEUE_H_ // NOLINT
