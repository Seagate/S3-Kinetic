/*
 * SmrEnv.cc
 *
 *  Created on: Apr 19, 2015
 *      Author: tri
 */

#include "SmrEnv.h"

#include <signal.h>
using namespace leveldb;

//namespace leveldb {
namespace smr {

ostream& operator<<(ostream& out, SmrEnv& env) {
    out << *(env.disk_) << endl;
    return out;
}


SmrEnv::SmrEnv() : page_size_(getpagesize()),
        started_bgthread_(false),
        has_bg_item_(false),
        disk_(NULL){
    has_bg_defrag_item_ = false;
    this->started_bgDefragThread_ = false;
    PthreadCall("mutex_init", pthread_mutex_init(&mu_, NULL));
    PthreadCall("mutex_init", pthread_mutex_init(&defragmu_, NULL));
    PthreadCall("cvar_init", pthread_cond_init(&bgsignal_, NULL));
    PthreadCall("cvar_init", pthread_cond_init(&bgDefragSignal_, NULL));
    PthreadCall("mutex_init", pthread_mutex_init(&a_mu_, NULL));
    PthreadCall("cvar_init", pthread_cond_init(&a_signal_, NULL));
    PthreadCall("mutex_init", pthread_mutex_init(&a_c_mu_, NULL));
    PthreadCall("cvar_init", pthread_cond_init(&a_c_signal_, NULL));
}

void SmrEnv::ClearBG() {
    PthreadCall("lock", pthread_mutex_lock(&mu_));
    if(has_bg_item_){
        bg_item_.function = NULL;
        bg_item_.arg = NULL;
        has_bg_item_ = false;
    }
    PthreadCall("unlock", pthread_mutex_unlock(&defragmu_));
    PthreadCall("lock", pthread_mutex_lock(&defragmu_));
    if(has_bg_defrag_item_){
        bg_defrag_item_.function = NULL;
        bg_defrag_item_.arg = NULL;
        has_bg_defrag_item_ = false;
    }
    PthreadCall("unlock", pthread_mutex_unlock(&defragmu_));
}

void SmrEnv::Schedule(void (*function)(void*), void* arg, void (*bg_function)(void*)) {
    PthreadCall("lock", pthread_mutex_lock(&mu_));

    // Start background thread if necessary
    if (!started_bgthread_) {
        started_bgthread_ = true;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        size_t STK_SIZE = 8 * 1024 * 1024;
        pthread_attr_setstacksize(&attr, STK_SIZE);
        PthreadCall(
                "create thread",
                pthread_create(&bgthread_, &attr,  &SmrEnv::BGThreadWrapper, this));
        pthread_attr_destroy(&attr);
    }

    if (bg_function !=NULL){
        has_bg_item_ = true;
        bg_item_.function = bg_function;
        bg_item_.arg = arg;
    }

    // If the queue is currently empty, the background thread may currently be
    // waiting.
    if (queue_.empty()) {
        PthreadCall("signal", pthread_cond_signal(&bgsignal_));
    }

    // Add to priority queue
    queue_.push_back(BGItem());
    queue_.back().function = function;
    queue_.back().arg = arg;
//    cout << "QSIZE " << queue_.size() << endl;
    PthreadCall("unlock", pthread_mutex_unlock(&mu_));
}
void SmrEnv::ScheduleDefrag(void (*function)(void*), void* arg, void (*bg_function)(void*)) {
    PthreadCall("lock", pthread_mutex_lock(&defragmu_));

    // Start background thread if necessary
    if (!started_bgDefragThread_) {
        started_bgDefragThread_ = true;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        size_t STK_SIZE = 8 * 1024 * 1024;
        pthread_attr_setstacksize(&attr, STK_SIZE);
        PthreadCall(
                "create thread",
                pthread_create(&bgDefragThread_, &attr,  &SmrEnv::BGDefragThreadWrapper, this));
        pthread_attr_destroy(&attr);
    }

    if (bg_function !=NULL){
        has_bg_defrag_item_ = true;
        bg_defrag_item_.function = bg_function;
        bg_defrag_item_.arg = arg;
    }

    // If the queue is currently empty, the background thread may currently be
    // waiting.
    if (defrag_queue_.empty()) {
        PthreadCall("signal", pthread_cond_signal(&bgDefragSignal_));
    }

    // Add to priority queue
    defrag_queue_.push_back(BGItem());
    defrag_queue_.back().function = function;
    defrag_queue_.back().arg = arg;
    PthreadCall("unlock", pthread_mutex_unlock(&defragmu_));
}

void SmrEnv::BGThread() {
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timespec cond_wait_time;
    cond_wait_time.tv_sec= now.tv_sec;
    cond_wait_time.tv_nsec = (now.tv_usec+1) * 1000;
#ifndef PRODUCT_X86
    signal(SIGSEGV, segFaultHandler);
#endif
    while (true) {
        // Wait until there is an item that is ready to run
        PthreadCall("lock", pthread_mutex_lock(&mu_));
        while (queue_.empty()) {
            if (has_bg_item_)
                PthreadCall("wait", pthread_cond_timedwait(&bgsignal_, &mu_, &cond_wait_time));
            else
                PthreadCall("wait", pthread_cond_wait(&bgsignal_, &mu_));
            if (has_bg_item_)
                break;
        }
        void (*function)(void*) = NULL;
        void* arg;
        if (!queue_.empty()){
            if (queue_.size() > 1) {
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": BG Q size = " << queue_.size() << endl;
            }
            function = queue_.front().function;
            arg = queue_.front().arg;
            queue_.pop_front();
        }
        else if(has_bg_item_){
            function = bg_item_.function;
            arg = bg_item_.arg;
        }

        PthreadCall("unlock", pthread_mutex_unlock(&mu_));
        if (function!= NULL)
            (*function)(arg);
    }
}
void SmrEnv::BGDefragThread() {
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timespec cond_wait_time;
    cond_wait_time.tv_sec= now.tv_sec;
    cond_wait_time.tv_nsec = (now.tv_usec+1) * 1000;
#ifndef PRODUCT_X86
    signal(SIGSEGV, segFaultHandler);
#endif
    while (true) {
        // Wait until there is an item that is ready to run
        PthreadCall("lock", pthread_mutex_lock(&defragmu_));
        while (this->defrag_queue_.empty()) {
            if (has_bg_defrag_item_)
                PthreadCall("wait", pthread_cond_timedwait(&this->bgDefragSignal_, &defragmu_, &cond_wait_time));
            else
                PthreadCall("wait", pthread_cond_wait(&bgDefragSignal_, &defragmu_));
            if (has_bg_defrag_item_)
                break;
        }
        void (*function)(void*) = NULL;
        void* arg;
        if (!defrag_queue_.empty()){
            if (false) { //defrag_queue_.size() > 1) {
               cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Defrag Q size = " << defrag_queue_.size() << endl;
            }
            function = defrag_queue_.front().function;
            arg = defrag_queue_.front().arg;
            defrag_queue_.pop_front();
        }
        else if(has_bg_defrag_item_){
            function = bg_defrag_item_.function;
            arg = bg_defrag_item_.arg;
        }

        PthreadCall("unlock", pthread_mutex_unlock(&defragmu_));
        if (function!= NULL)
            (*function)(arg);
    }
}

namespace {
struct StartThreadState {
        void (*user_function)(void*);
        void* arg;
};
}
static void* StartThreadWrapper(void* arg) {
    StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
    state->user_function(state->arg);
    delete state;
    return NULL;
}

void SmrEnv::StartThread(void (*function)(void* arg), void* arg) {
    pthread_t t;
    StartThreadState* state = new StartThreadState;
    state->user_function = function;
    state->arg = arg;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t STK_SIZE = 8 * 1024 * 1024;
    pthread_attr_setstacksize(&attr, STK_SIZE);
    PthreadCall("start thread",
            pthread_create(&t, &attr,  &StartThreadWrapper, state));
    pthread_attr_destroy(&attr);
}

}  // namespace smr
//}  // namespace leveldb


