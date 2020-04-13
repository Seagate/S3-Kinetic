/*
 * Listenable.cc
 *
 *  Created on: Nov 3, 2015
 *      Author: tri
 */

#include "Listenable.h"
#include <iostream>
using namespace std;

namespace com {
namespace seagate {
namespace common {

void Listenable::subscribe(Listener* listener) {
    listeners_.push_back(listener);
}

void Listenable::unsubscribe(Listener* listener) {
    vector<Listener*>::iterator it;
    for (it = listeners_.begin(); it != listeners_.end(); ++it) {
        if (*it == listener) {
            it = listeners_.erase(it);
            break;
        }
    }
}

} /* namespace common */
} /* namespace seagate */
} /* namespace com */
