/*
 * Subscription.h
 *
 *  Created on: May 12, 2016
 *      Author: tri
 */

#ifndef SUBSCRIPTION_H_ // NOLINT
#define SUBSCRIPTION_H_ // NOLINT

namespace com {
namespace seagate {
namespace common {
namespace event {

class Subscription {
  public:
    Subscription(Event* event, Subscriber* subscriber):
        event_(event), subscriber_(subscriber) {
    }
    virtual ~Subscription() {
        delete event_;
    }
    Subscriber* subscriber() const {
        return subscriber_;
    }
    bool hasSubscriberForEvent(Event* event) {
        return (event->publisher() == event_->publisher());
    }
    bool hasSubscribedForEvent(Event* event, Subscriber* subscriber) {
        return (event->publisher() == event_->publisher() && subscriber == subscriber_);
    }

  private:
    Event* event_;
    Subscriber* subscriber_;
};

} // namespace event
} // namespace common
} // namespace seagate
} // namespace com

#endif // SUBSCRIPTION_H_ NOLINT
