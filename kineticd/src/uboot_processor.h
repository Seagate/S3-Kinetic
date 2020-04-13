#ifndef KINETIC_UBOOT_PROCESSOR_H_
#define KINETIC_UBOOT_PROCESSOR_H_

#include "glog/logging.h"

#include "popen_wrapper.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class UbootProcessor : public LineProcessor {
    public:
    explicit UbootProcessor(std::string *uboot_version);
    virtual void ProcessLine(const string& line);
    virtual void ProcessPCloseResult(int pclose_result);
    virtual bool Success();

    private:
    std::string *uboot_version_;
    bool found_uboot_version_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_UBOOT_PROCESSOR_H_
