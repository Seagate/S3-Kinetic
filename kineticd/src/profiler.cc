#include "profiler.h"

#include <string.h>

#include <algorithm>
#include <vector>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "glog/logging.h"

#include "pthreads_mutex_guard.h"

namespace com {
namespace seagate {
namespace kinetic {

Event::Event()
    : profiler_(NULL), scoped_(false) {}

Event::~Event() {
    if (scoped_) {
        profiler_->End(*this);
    }
}

std::string Event::ToString(EventType event_type) {
    switch (event_type) {
        case kMessageSerialization:
            return "Message Serialization";
        case kMessageDeserialization:
            return "Message Deserialization";
        case kMessageProcessing:
            return "Message Processing";
        case kPrimaryStoreGet:
            return "Primary Store Get";
        case kPrimaryStorePut:
            return "Primary Store Put";
        case kAuthentication:
            return "Authentication";
        case kMacAssignment:
            return "MAC Assignment";
        case kKeyAuthorization:
            return "Key-scoped Authorization";
        case kGlobalAuthorization:
            return "Global Authorization";
        case kSkinnyWaistGet:
            return "Skinny Waist Get";
        case kSkinnyWaistPut:
            return "Skinny Waist Put";
        default:
            return "Error";
    }
}

Profiler::Profiler()
        : samples_(new uint64_t[kEnd * Profiler::kSampleBufferSize]),
        event_counters_(new uint64_t[kEnd]),
        success_(true) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&mutex_, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    memset(samples_, 0, kEnd * Profiler::kSampleBufferSize * sizeof(uint64_t));
    memset(event_counters_, 0, kEnd * sizeof(uint64_t));
}

Profiler::~Profiler() {
    CHECK(!pthread_mutex_destroy(&mutex_));
    delete[] samples_;
    delete[] event_counters_;
}

void Profiler::Begin(EventType event_type, Event *e) {
    Begin(event_type, false, e);
}

void Profiler::BeginAutoScoped(EventType event_type, Event *e) {
    Begin(event_type, true, e);
}

void Profiler::End(const Event &event) {
    if (!success_) {
        return;
    }
    PthreadsMutexGuard guard(&mutex_);
    struct timespec tp;
    if (!GetTime(&tp)) {
        PLOG(ERROR) << "Failed to get time from POSIX clock";//NO_SPELL
        success_ = false;
        return;
    }
    RecordSample(event.event_type_, ToRawNanoseconds(&tp) - event.start_ns_);
}

void Profiler::LogResults() {
    if (!success_) {
        LOG(ERROR) << "Refusing to log results because an irrecoverable error previously occurred";
        return;
    }

    PthreadsMutexGuard guard(&mutex_);
    for (size_t i = 0; i < kEnd; ++i) {
        LOG(INFO) << Event::ToString(static_cast<EventType>(i)) << ": "
                    << event_counters_[i] << " events";
        std::vector<uint64_t> v(samples_ + i * Profiler::kSampleBufferSize,
            samples_ + (i + 1) * Profiler::kSampleBufferSize);
        std::sort(v.begin(), v.end());
        for (auto it = v.begin(); it != v.end(); ++it) {
            LOG(INFO) << *it;
        }
    }
}

void Profiler::Begin(EventType event_type, bool scoped, Event *e) {
    if (!success_) {
        return;
    }
    struct timespec tp;
    if (!GetTime(&tp)) {
        PLOG(ERROR) << "Failed to get time from POSIX clock";//NO_SPELL
        success_ = false;
        return;
    }
    e->profiler_ = this;
    e->event_type_ = event_type;
    e->start_ns_ = ToRawNanoseconds(&tp);
    e->scoped_ = scoped;
}

uint64_t Profiler::ToRawNanoseconds(const struct timespec *tp) {
    return tp->tv_sec * 1000000000 + tp->tv_nsec;
}

void Profiler::RecordSample(EventType event_type, uint64_t ns) {
    size_t ring_position = event_counters_[event_type] % Profiler::kSampleBufferSize;
    size_t index = event_type * Profiler::kSampleBufferSize + ring_position;
    samples_[index] = ns;
    event_counters_[event_type]++;
}

bool Profiler::GetTime(struct timespec* tp) {
    #ifndef __MACH__
        return clock_gettime(CLOCK_MONOTONIC, tp) != -1;
    #else
        // OSX rudely omits clock_gettime but does provide a somewhat more complex
        // way of getting a monotonically increasing timer value
        clock_serv_t cclock;
        mach_timespec_t mts;

        if (host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock) != KERN_SUCCESS) {
            return false;
        }

        bool got_clock_time = clock_get_time(cclock, &mts) == KERN_SUCCESS;

        mach_port_deallocate(mach_task_self(), cclock);

        if (got_clock_time) {
            tp->tv_sec = mts.tv_sec;
            tp->tv_nsec = mts.tv_nsec;
        }

        return got_clock_time;
    #endif
}

} // namespace kinetic
} // namespace seagate
} // namespace com
