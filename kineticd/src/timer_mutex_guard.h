#ifndef KINETIC_TIMER_MUTEX_GUARD_H_
#define KINETIC_TIMER_MUTEX_GUARD_H_
#include <pthread.h>

namespace com {
namespace seagate {
namespace kinetic {
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/// TimerMutexGuard
///     -Used By: @AgingTimer
/// -------------------------------------------
/// @Summary:
/// - Creates locally scoped Mutex that is promptly released
///   if returned before an explicit unlock
///------------------------------------------------------
class TimerMutexGuard {
    public:
    explicit TimerMutexGuard(pthread_mutex_t* mutex);
    ~TimerMutexGuard();

    private:
    pthread_mutex_t* mutex_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_TIMER_MUTEX_GUARD_H_
