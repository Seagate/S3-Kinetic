#ifndef KINETIC_SCHEDULED_BACKGROUND_TASK_H_
#define KINETIC_SCHEDULED_BACKGROUND_TASK_H_

#include <string>

#include "kinetic/common.h"

namespace com {
namespace seagate {
namespace kinetic {

class ScheduledBackgroundTask {
    public:
    ScheduledBackgroundTask(const std::string task_name, uint64_t interval_seconds);
    virtual void SetupWork() {}
    virtual void CleanupWork() {}
    virtual void DoWork() = 0;
    void WorkForever();
    void Start();
    void Stop();
    bool Running();
    virtual ~ScheduledBackgroundTask();

    private:
    const std::string task_name_;
    uint64_t interval_seconds_;
    bool running_;
    pthread_t thread_;
    pthread_mutex_t running_mutex_;
    pthread_cond_t running_condition_;
    DISALLOW_COPY_AND_ASSIGN(ScheduledBackgroundTask);
};


} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SCHEDULED_BACKGROUND_TASK_H_
