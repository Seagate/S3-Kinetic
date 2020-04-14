/*
 * EventService.h
 *
 *  Created on: May 12, 2016
 *      Author: tri
 */

#ifndef EVENT_SERVICE_H_ // NOLINT
#define EVENT_SERVICE_H_ // NOLINT

#include <vector>
#include "util/mutexlock.h"

using namespace std; // NOLINT
using namespace leveldb; //NOLINT
using namespace leveldb::port; //NOLINT

namespace com {
namespace seagate {
namespace common {
namespace event {

class Event;
class Subscriber;
class Subscription;

class EventService {
  public:
    static EventService* getInstance() {
        static EventService* _instance = new EventService();
        return _instance;
    }
    void subscribe(Event* event, Subscriber* subscriber);
    void unsubscribe(Event* event, Subscriber* subscriber);
    void publish(Event* event);

  private:
    // Methods
    EventService() {
    }
    virtual ~EventService();
    // Attributes
    vector<Subscription*> subscriptions_;
    port::Mutex mu_;
};

} // namespace event
} // namespace common
} // namespace seagate
} // namespace com

#endif // EVENT_SERVICE_H_ // NOLINT
