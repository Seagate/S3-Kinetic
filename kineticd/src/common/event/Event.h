/*
 * Event.h
 *
 *  Created on: May 12, 2016
 *      Author: tri
 */

#ifndef EVENT_H_ // NOLINT
#define EVENT_H_ // NOLOINT

namespace com {
namespace seagate {
namespace common {
namespace event {

class Publisher;

class Event {
 public:
    Event() {
    }
    virtual ~Event() {
    }
    virtual Publisher* publisher() const = 0;
};

class TimeoutEvent: public Event {
 public:
    TimeoutEvent(): publisher_(NULL) {
    }
    TimeoutEvent(const TimeoutEvent& src) {
        publisher_ = src.publisher_;
    }
    TimeoutEvent& operator=(const TimeoutEvent& src) {
        publisher_ = src.publisher_;
        return *this;
    }
    TimeoutEvent(Publisher* publisher): publisher_(publisher) { // NOLINT
    }
    virtual ~TimeoutEvent() {
    }
    Publisher* publisher() const {
        return publisher_;
    }

 private:
    Publisher* publisher_;
};

} // namespace event
} // namespace common
} // namespace seagate
} // namespace com

#endif // EVENT_H_ NOLINT
