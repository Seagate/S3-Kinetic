#include <sys/time.h>

#include <glog/logging.h>

#include "pthreads_mutex_guard.h"
#include "scheduled_background_task.h"
#include <iostream>
using namespace std; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

static void* scheduled_background_task_worker(void *arg) {
    ScheduledBackgroundTask *scheduled_background_task = (ScheduledBackgroundTask *) arg;
    scheduled_background_task->WorkForever();

    return NULL;
}

ScheduledBackgroundTask::ScheduledBackgroundTask(
        const std::string task_name,
        uint64_t interval_seconds) :
    task_name_(task_name),
    interval_seconds_(interval_seconds),
    running_(false) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&running_mutex_, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    pthread_cond_init(&running_condition_, NULL);
}

ScheduledBackgroundTask::~ScheduledBackgroundTask() {
    CHECK(!pthread_mutex_destroy(&running_mutex_));
    CHECK(!pthread_cond_destroy(&running_condition_));
}


void ScheduledBackgroundTask::Start() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t STK_SIZE = 1 * 1024 * 1024;
    pthread_attr_setstacksize(&attr, STK_SIZE);
    CHECK(!pthread_create(&thread_, &attr, scheduled_background_task_worker, this));
    CHECK(!pthread_attr_destroy(&attr));
}

void ScheduledBackgroundTask::WorkForever() {
    CHECK(!pthread_mutex_lock(&running_mutex_));
    running_ = true;
    CHECK(!pthread_mutex_unlock(&running_mutex_));
    if (interval_seconds_ == 0U) {
        VLOG(1) << "Disabling " << task_name_;
        return;
    }

    VLOG(1) << "Starting " << task_name_;

    SetupWork();

    while (true) {
        // Create nested scope so that the mutex is promptly released even
        // if we don't return
        {
            PthreadsMutexGuard guard(&running_mutex_);
            if (!running_) {
                CleanupWork();

                return;
            }
        }

        DoWork();

        // Someone might have called Stop() while DoWork() was running. If this happens
        // then we won't know until after the timed sleep below and waste interval_seconds_
        // seconds. To prevent this check running_ just before starting the sleep.
        {
            PthreadsMutexGuard guard(&running_mutex_);
            if (!running_) {
                return;
            }
        }

        // Unlike a plain sleep() this will allow for a pause
        // while cleanly allowing interruptions
        // from the Stop() method on all platforms
        CHECK(!pthread_mutex_lock(&running_mutex_));

        struct timeval tv;
        struct timespec ts;
        gettimeofday(&tv, NULL);
        TIMEVAL_TO_TIMESPEC(&tv, &ts);
        ts.tv_sec += interval_seconds_;

        CHECK_NE(EINVAL, pthread_cond_timedwait(&running_condition_, &running_mutex_, &ts));
        CHECK(!pthread_mutex_unlock(&running_mutex_));
    }
}

void ScheduledBackgroundTask::Stop() {
    LOG(INFO) << "Stopping... " << task_name_;
    CHECK(!pthread_mutex_lock(&running_mutex_));
    running_ = false;
    CHECK(!pthread_cond_signal(&running_condition_));
    CHECK(!pthread_mutex_unlock(&running_mutex_));

    CHECK(!pthread_join(thread_, NULL));
    LOG(INFO) << "Stopped " << task_name_;
}

bool ScheduledBackgroundTask::Running() {
    PthreadsMutexGuard guard(&running_mutex_);
    return running_;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
