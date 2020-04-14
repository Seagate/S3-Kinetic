/*
 * Listenable.h
 *
 *  Created on: Nov 3, 2015
 *      Author: tri
 */

#ifndef LISTENABLE_H_
#define LISTENABLE_H_

#include <vector>

#include "Listener.h"

using namespace std;

namespace com {
namespace seagate {
namespace common {

class Listenable {
public:
    Listenable() {
    }
    virtual ~Listenable() {
    }
    void subscribe(Listener* listener);
    void unsubscribe(Listener* listener);
    vector<Listener*>* getSubscribers() {
        return &listeners_;
    }
    void subscribe(vector<Listener*>* listeners) {
        vector<Listener*>::iterator it;
        for (it = listeners->begin(); it != listeners->end(); ++it) {
            subscribe(*it);
        }
    }

protected:
    virtual void notify() {
       vector<Listener*>::iterator it;
       for (it = listeners_.begin(); it != listeners_.end(); ++it) {
          (*it)->notify();
       }
    }
protected:
    vector<Listener*> listeners_;
};

} /* namespace common */
} /* namespace seagate */
} /* namespace com */

#endif /* LISTENABLE_H_ */
