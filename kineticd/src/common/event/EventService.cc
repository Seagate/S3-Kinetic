/*
 * EventService.cc
 *
 *  Created on: May 12, 2016
 *      Author: tri
 */

#include "EventService.h"

#include <iostream>

#include "Event.h"
#include "Subscription.h"
#include "Subscriber.h"

using namespace std; // NOLINT

namespace com {
namespace seagate {
namespace common {
namespace event {

EventService::~EventService() {
    vector<Subscription*>::iterator it;
    for (it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        delete *it;
    }
}

void EventService::subscribe(Event* event, Subscriber* subscriber) {
    bool found = false;
    vector<Subscription*>::iterator it;
    MutexLock lock(&mu_);
    for (it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        if ((*it)->hasSubscribedForEvent(event, subscriber)) {
            found = true;
            break;
        }
    }
    if (!found) {
        Subscription* subscription = new Subscription(event, subscriber);
        subscriptions_.push_back(subscription);
    } else {
        delete event;
    }
}
void EventService::unsubscribe(Event* event, Subscriber* subscriber) {
    vector<Subscription*>::iterator it;
    MutexLock lock(&mu_);
    for (it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        Subscription* subscription = *it;
        if (subscription->hasSubscribedForEvent(event, subscriber)) {
            subscriptions_.erase(it);
            delete subscription;
            break;
        }
    }
}
void EventService::publish(Event* event) {
    mu_.Lock();
    vector<Subscription*>::iterator it;
    it = subscriptions_.begin();
    while (it != subscriptions_.end()) {
        Subscription* subscription = *it;
        Subscriber* subscriber = subscription->subscriber();
        if (subscription->hasSubscriberForEvent(event)) {
            mu_.Unlock();
            subscriber->inform(event);
            return;
        } else {
            ++it;
        }
    }
    mu_.Unlock();
}

} /* namespace event */
} /* namespace common */
} /* namespace seagate */
} /* namespace com */
