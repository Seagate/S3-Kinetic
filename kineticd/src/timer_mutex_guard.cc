#include "timer_mutex_guard.h"
#include <stdio.h>

namespace com {
namespace seagate {
namespace kinetic {

TimerMutexGuard::TimerMutexGuard(pthread_mutex_t* mutex) : mutex_(mutex) {
    pthread_mutex_lock(mutex_);
}

TimerMutexGuard::~TimerMutexGuard() {
    pthread_mutex_unlock(mutex_);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
