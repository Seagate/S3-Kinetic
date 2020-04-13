#ifndef KINETIC_MOCK_LOG_HANDLER_H_
#define KINETIC_MOCK_LOG_HANDLER_H_

#include "gmock/gmock.h"

#include "log_handler_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockLogHandler : public LogHandlerInterface {
    public:
    MockLogHandler() {}
    MOCK_METHOD1(LogLatency, void(unsigned char flag));
    MOCK_METHOD1(LogStaleEntry, void(int level));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_LOG_HANDLER_H_
