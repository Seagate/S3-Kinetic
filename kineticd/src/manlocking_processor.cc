#include <stdlib.h>

#include "manlocking_processor.h"
#include "popen_wrapper.h"
#include <utility>
#include <functional>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
const char ManlockingProcessor::MANLOCKING_STATE_QUERY[] = "Band Number 1 is ";

ManlockingProcessor::ManlockingProcessor(int* task_status, int* pclose_result)
    : task_status_(task_status), pclose_result_(pclose_result) {
    *task_status_ = -1;
}

void ManlockingProcessor::ProcessLine(const string& line) {
    string search_string = MANLOCKING_STATE_QUERY;
    string lock_status_string = "NOT Locked";
    if (line.find(search_string) != string::npos) {
        if (line.find(lock_status_string) != string::npos) {
            // Unlocked
            *task_status_ = 0;
        } else {
            // Locked
            *task_status_ = 1;
        }
    }
}

void ManlockingProcessor::ProcessPCloseResult(int pclose_result) {
    *pclose_result_ = pclose_result;
}

bool ManlockingProcessor::Success() {
    if (*task_status_== -1) {
        return false;
    }
    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
