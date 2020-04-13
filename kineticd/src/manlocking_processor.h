#ifndef KINETIC_MANLOCKING_PROCESSOR_H_
#define KINETIC_MANLOCKING_PROCESSOR_H_

#include "glog/logging.h"

#include "popen_wrapper.h"

#include <map>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class ManlockingProcessor : public LineProcessor {
    public:
    static const char MANLOCKING_STATE_QUERY[];
    explicit ManlockingProcessor(int* task_status, int* pclose_result);
    virtual void ProcessLine(const string& line);
    virtual void ProcessPCloseResult(int pclose_result);
    virtual bool Success();

    private:
    int* task_status_;
    int* pclose_result_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MANLOCKING_PROCESSOR_H_
