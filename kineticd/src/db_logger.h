#ifndef KINETIC_DB_LOGGER_H_
#define KINETIC_DB_LOGGER_H_

#include "leveldb/env.h"
#include "log_handler_interface.h"
#include <string>
namespace com {
namespace seagate {
namespace kinetic {

class DbLogger : public leveldb::Logger {
    virtual void Logv(const char* format, va_list ap);
    virtual void LogLevelv(unsigned int level, const char* format, va_list ap);
    virtual void CompactionEvent();
    virtual void StaleEntryRemovalEvent(int level);
    public:
    void SetLogHandlerInterface(LogHandlerInterface* log_handler) {
        log_handler_ = log_handler;
    }

    private:
    LogHandlerInterface* log_handler_;
    static const std::string kDATABASE_PREFIX;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DB_LOGGER_H_
