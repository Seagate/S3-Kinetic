#include "glog/logging.h"

#include "db_logger.h"

namespace com {
namespace seagate {
namespace kinetic {

const std::string DbLogger::kDATABASE_PREFIX = "[DATABASE] ";

void DbLogger::Logv(const char* format, va_list ap) {
    char log_buffer[1024];
    vsnprintf(log_buffer, sizeof(log_buffer), format, ap);
    LOG(INFO) << kDATABASE_PREFIX << log_buffer;
}

void DbLogger::LogLevelv(unsigned int level, const char* format, va_list ap) {
    char log_buffer[1024];
    vsnprintf(log_buffer, sizeof(log_buffer), format, ap);
    switch (level) {
        case(0):
            LOG(INFO) << kDATABASE_PREFIX << log_buffer;
            break;
        case(1):
            LOG(WARNING) << kDATABASE_PREFIX << log_buffer;
            break;
        case(2):
            LOG(ERROR) << kDATABASE_PREFIX << log_buffer;
            break;
        case(3):
            LOG(FATAL) << kDATABASE_PREFIX << log_buffer;
            break;
        default:
            VLOG(level - 4) << kDATABASE_PREFIX << log_buffer;
            break;
    }
}

void DbLogger::CompactionEvent() {
    log_handler_->LogLatency(LATENCY_EVENT_LOG_COMPACTION);
}

void DbLogger::StaleEntryRemovalEvent(int level) {
    log_handler_->LogStaleEntry(level);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
