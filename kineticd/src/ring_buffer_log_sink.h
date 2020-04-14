#ifndef KINETIC_RING_BUFFER_LOG_SINK_H_
#define KINETIC_RING_BUFFER_LOG_SINK_H_

#include "glog/logging.h"
#include "log_ring_buffer.h"

namespace com {
namespace seagate {
namespace kinetic {

class RingBufferLogSink : public ::google::LogSink {
    public:
    explicit RingBufferLogSink(size_t max_message_len);

    virtual void send(int severity, const char *full_filename, const char *base_filename, int line,
        const struct ::tm *tm_time, const char *message, size_t message_len);

    void copyBufferInto(std::vector<LogRingBufferEntry> &dest);

    private:
    pid_t GetTID();
    size_t max_message_len_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_RING_BUFFER_LOG_SINK_H_
