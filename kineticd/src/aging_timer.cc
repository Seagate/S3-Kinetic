#include "aging_timer.h"
#include "gmock/gmock.h"
#include "glog/logging.h"
#include <ctime>
#include <iostream>
using namespace std; //NOLINT
using namespace std::chrono; //NOLINT
using std::chrono::milliseconds; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

static void* scheduled_timer_worker(void *arg) {
    AgingTimer *Aging_Timer = (AgingTimer *) arg;
    Aging_Timer->TimeLoop();
    return NULL;
}

AgingTimer::AgingTimer(TimerHandlerInterface *timer_handler) {
    pthread_mutex_init(&mutex_, NULL);
    timer_handler_ptr_ = timer_handler;
    running_ = true;
    armed_ = false;
    sleep_milisec_ = 20;
    def_timeout_window_ = 30000;
    time_out_ = 0;
    time_finish_ = high_resolution_clock::now();
    thread_start_ = high_resolution_clock::now();
    thread_stop_ = high_resolution_clock::now();
}

AgingTimer::~AgingTimer() {
    pthread_mutex_destroy(&mutex_);
    timer_handler_ptr_ = nullptr;
}

void AgingTimer::UpdateFinishTime(uint64_t value) {
#ifdef KDEBUG
    DLOG(INFO) << "=== AgingTimer:: UpdateFinishTime()";//NO_SPELL
#endif
    high_resolution_clock::time_point tNow = high_resolution_clock::now();
    high_resolution_clock::time_point tfinish_candidate = tNow + milliseconds { value }; //NOLINT

    if (tfinish_candidate < time_finish_ || time_out_ == 0) {
        //DLOG(INFO) << "=== AgingTimer:: constraining time!";//NO_SPELL
        time_finish_ = tfinish_candidate;
        time_out_ = value;
        #ifdef KDEBUG
        milliseconds time_span = duration_cast<milliseconds>(time_finish_ - tNow);
        DLOG(INFO) << "=== time_finish_ window == " << time_span.count();//NO_SPELL
        #endif
    }
}

void AgingTimer::StartTimerThread() {
    TimerMutexGuard guard(&mutex_);
#ifdef KDEBUG
    DLOG(INFO) << "=== AgingTimer::StartTimerThread()";//NO_SPELL
#endif
    thread_start_ = high_resolution_clock::now();
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t STK_SIZE = 1 * 1024 * 1024;
    pthread_attr_setstacksize(&thread_attr, STK_SIZE);
    pthread_create(&thread_, &thread_attr, scheduled_timer_worker, this);
    pthread_attr_destroy(&thread_attr);
}

void AgingTimer::StopTimerThread() {
    pthread_mutex_lock(&mutex_);
    if (running_) {
        LOG(INFO) << "Stopping aging timer...";//NO_SPELL
        running_ = false;
        pthread_mutex_unlock(&mutex_);
        pthread_join(thread_, NULL);
        thread_stop_ = std::chrono::system_clock::now();
        LOG(INFO) << "Aging timer stopped";
    }
}

void AgingTimer::PrintThreadRunTime() {
#ifdef KDEBUG
    std::chrono::seconds time_span = duration_cast<seconds>(thread_stop_ - thread_start_);
    DLOG(INFO) << "=== AgingTimer:: Total Thread Run Time:: " << time_span.count();//NO_SPELL
#endif
}

bool AgingTimer::Running() {
    return running_;
}

void AgingTimer::TimeLoop() {
    while (true) {
        if (!Running()) { return; }
        if (IsArmed()) {
            if (TimeCheck()) {
#ifdef KDEBUG
                DLOG(INFO) << "=== AgingTimer::Timer Expired, Flush and Disarm";//NO_SPELL
#endif
#ifndef NLOG
                //LOG(INFO) << "Aging Timer timed out. Flush log/sst" << endl;
#endif
                DisarmTimer();
                timer_handler_ptr_->ServiceTimer(true);
                continue;
            }
        }
        if (!Running()) { return; }
        struct timespec sleep_time = { 0 , 0 };
        sleep_time.tv_nsec = sleep_milisec_ * 1000000L;
        nanosleep(&sleep_time, NULL);
    }
}

void AgingTimer::ArmTimer(uint64_t time) {
#ifdef KDEBUG
    DLOG(INFO) << "=== AgingTimer:: ArmTimer(" << time << ")";//NO_SPELL
#endif
    TimerMutexGuard guard(&mutex_);
    if (!armed_) {
        armed_ = true;
    }
    return UpdateFinishTime(time);
}

void AgingTimer::DisarmTimer() {
    TimerMutexGuard guard(&mutex_);
    armed_ = false;
    time_out_ = 0;
    return;
}

bool AgingTimer::IsArmed() {
    return armed_;
}

bool AgingTimer::TimeCheck() {
    high_resolution_clock::time_point tNow = high_resolution_clock::now();
    milliseconds time_span = duration_cast<milliseconds>(tNow - time_finish_);

    if (time_span.count() >= 0) {
        #ifdef KDEBUG
        auto duration1 = time_finish_.time_since_epoch();
        auto mili1 = duration_cast<milliseconds>(duration1).count();
        auto duration2 = tNow.time_since_epoch();
        auto mili2 = duration_cast<milliseconds>(duration2).count();
        DLOG(INFO) << "=== AgingTimer:: TimeCheck"//NO_SPELL
            << " Now(" << mili2 << ") - Finish(" << mili1 << ")"//NO_SPELL
            << " == " << time_span.count();
        #endif
        return true;
    } return false;
}

} // namespace kinetic
} // namespace seagate
} // namespace com

