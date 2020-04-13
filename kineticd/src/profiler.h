#ifndef KINETIC_PROFILER_H_
#define KINETIC_PROFILER_H_

#include <pthread.h>
#include <time.h>

#include <string>

#include "kinetic/common.h"

namespace com {
namespace seagate {
namespace kinetic {

enum EventType {
    kMessageSerialization,
    kMessageDeserialization,
    kMessageProcessing,
    kPrimaryStoreGet,
    kPrimaryStorePut,
    kAuthentication,
    kMacAssignment,
    kKeyAuthorization,
    kGlobalAuthorization,
    kSkinnyWaistGet,
    kSkinnyWaistPut,
    kEnd  // enum end marker
};

class Profiler;  // forward declaration

class Event {
    public:
    Event();
    ~Event();
    friend class Profiler;

    private:
    static std::string ToString(EventType event_type);
    Profiler *profiler_;
    uint64_t start_ns_;
    EventType event_type_;
    bool scoped_;
    DISALLOW_COPY_AND_ASSIGN(Event);
};

/**
* Threadsafe
*/
class Profiler {
    public:
    Profiler();
    ~Profiler();
    void Begin(EventType event_type, Event *e);
    void BeginAutoScoped(EventType event_type, Event *e);
    void End(const Event &event);
    void LogResults();

    private:
    static const size_t kSampleBufferSize = 32;
    void Begin(EventType event_type, bool scoped, Event *e);
    uint64_t ToRawNanoseconds(const struct timespec *tp);
    void RecordSample(EventType event_type, uint64_t ns);
    bool GetTime(struct timespec *tp);
    pthread_mutex_t mutex_;
    uint64_t *samples_;
    uint64_t *event_counters_;
    bool success_;
    DISALLOW_COPY_AND_ASSIGN(Profiler);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PROFILER_H_
