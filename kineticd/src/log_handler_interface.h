#ifndef KINETIC_LOG_HANDLER_INTERFACE_H_
#define KINETIC_LOG_HANDLER_INTERFACE_H_

#define LATENCY_EVENT_LOG_COMPACTION                  1  // 2^0, bit 0
#define LATENCY_EVENT_LOG_UPDATE                      2  // 2^1, bit 1
#define LATENCY_EVENT_LOG_OUTSTANDINGCOMMAND          4  // 2^2, bit 2

namespace com {
namespace seagate {
namespace kinetic {

class LogHandlerInterface {
    public:
    virtual ~LogHandlerInterface() {}
    virtual void LogLatency(unsigned char flag)=0;
    virtual void LogStaleEntry(int level)=0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_LOG_HANDLER_INTERFACE_H_
