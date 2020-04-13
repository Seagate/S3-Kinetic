#include <stdio.h>
#include "glog/logging.h"

#include "pthreads_mutex_guard.h"

namespace com {
namespace seagate {
namespace kinetic {


PthreadsMutexGuard::PthreadsMutexGuard(pthread_mutex_t* mutex) : mutex_(mutex) {
    pthread_mutex_lock(mutex_);
}

PthreadsMutexGuard::~PthreadsMutexGuard() {
    pthread_mutex_unlock(mutex_);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
