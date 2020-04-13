#include "launch_monitor.h"

#include "version_info.h"
#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::make_shared;
using std::pair;
using std::move;
using std::unique_ptr;

#define LAUNCH_THRESHOLD 5


LaunchMonitorPassthrough::LaunchMonitorPassthrough() {
}

LaunchMonitorPassthrough::~LaunchMonitorPassthrough() {
}

bool LaunchMonitorPassthrough::OperationAllowed(LaunchStep requested_operation) {
    return true;
}

void LaunchMonitorPassthrough::OperationCompleted() {
}

void LaunchMonitorPassthrough::LoadMonitor() {
}

void LaunchMonitorPassthrough::SuccessfulLoad() {
}

bool LaunchMonitorPassthrough::DidStop() {
    return false;
}

LaunchMonitor::LaunchMonitor(unique_ptr<CautiousFileHandlerInterface> file_handler)
        : file_handler_(move(file_handler)) {
}

LaunchMonitor::~LaunchMonitor() {
}

void LaunchMonitor::InitClearCount() {
    stop_load = false;
    fail_count = 0;
    stop_point = LaunchStep::NO_STOP;
}

void LaunchMonitor::SuccessfulLoad() {
    InitClearCount();
    WriteState(LaunchStep::NO_STOP, true);
}

bool LaunchMonitor::DidStop() {
    return stop_load;
}

void LaunchMonitor::WriteState(LaunchStep current_operation, bool bSuccess) {
    // Write current_operation, fail_count, and CURRENT_SEMANTIC_VERSION
    std::ostringstream stringStream;
    std::string output;
    stringStream << (int)current_operation << " ";
    // Only increment count if we are potentially at a stop point
    if (current_operation == LaunchStep::NO_STOP || bSuccess) {
        stringStream << fail_count << " ";
    } else {
        stringStream << fail_count + 1 << " ";
    }
    stringStream << CURRENT_SEMANTIC_VERSION;
    output = stringStream.str();
    file_handler_->Write(output);
}

// Called when there is no pending risk operation, but the load isn't complete
//   For example, when the drive powers up in Lock state
void LaunchMonitor::OperationCompleted() {
    WriteState(LaunchStep::NO_STOP, true);
}

// Returns true if operation is allowed
bool LaunchMonitor::OperationAllowed(LaunchStep requested_operation) {
    if (stop_load) {
        // Don't update disc state anymore
        if (requested_operation == stop_point) {
            LOG(ERROR) << "Launch monitor halted load: " << int(stop_point);
            return false;
        } else {
            return true;
        }
    }
    return true;
}

void LaunchMonitor::LoadMonitor() {
    string fw_revision;
    int convert;
    string data;
    stop_load = false;
    FileReadResult readResult = file_handler_->Read(data);
    if ((readResult == FileReadResult::IO_ERROR) ||
            (readResult == FileReadResult::CANT_OPEN_FILE)) {
        LOG(WARNING) << "Could not read read prior launch status";
        InitClearCount();
    } else {
        std::stringstream stringStream(data);
        stringStream >> convert;
        stringStream >> fail_count;
        stringStream >> fw_revision;
        stop_point = (LaunchStep)convert;
        // Parse data into FW, FailCount, and Tag
        if (fw_revision.compare(CURRENT_SEMANTIC_VERSION) != 0) {
            // new firmware revision - reset counts to allow load attempt
            InitClearCount();
        }
    }
    if (fail_count > LAUNCH_THRESHOLD) {
        stop_load = true;
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
