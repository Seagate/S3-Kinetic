#ifndef KINETIC_PTHREADS_MUTEX_GUARD_H_
#define KINETIC_PTHREADS_MUTEX_GUARD_H_

#include <pthread.h>

#include "kinetic/common.h"

namespace com {
namespace seagate {
namespace kinetic {

class PthreadsMutexGuard {
    public:
    explicit PthreadsMutexGuard(pthread_mutex_t* mutex);
    ~PthreadsMutexGuard();

    private:
    pthread_mutex_t* mutex_;

    DISALLOW_COPY_AND_ASSIGN(PthreadsMutexGuard);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PTHREADS_MUTEX_GUARD_H_
