/*
 * Subscriber.h
 *
 *  Created on: May 12, 2016
 *      Author: tri
 */

#ifndef SUBSCRIBER_H_ // NOLINT
#define SUBSCRIBER_H_ // NOLINT

namespace com {
namespace seagate {
namespace common {
namespace event {

class Event;

class Subscriber {
  public:
    Subscriber() {
    }
    virtual ~Subscriber() {
    }
    virtual void inform(Event* event) = 0;
};

} // namespace event
} // namespace common
} // namespace seagate
} // namespace com

#endif // SUBSCRIBER_H_ NOLINT
