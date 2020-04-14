/*
 * PriorityRequestQueue.cc
 *
 *  Created on: Jan 17, 2018
 *      Author: tri
 */

#include "PriorityRequestQueue.h"

namespace com {
namespace seagate {
namespace kinetic {

int PriorityRequestQueue::numberOfRequests() {
    MutexLock l(&mu_);
    int nRequests = 0;
    unordered_map<int, RequestQueue*>::iterator it;

    for (it = priorityToRequestMap_.begin(); it != priorityToRequestMap_.end(); ++it) {
        nRequests += it->second->size();
    }
    return nRequests;
}

} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */
