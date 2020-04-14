#include <stdlib.h>

#include "uboot_processor.h"
#include "popen_wrapper.h"
#include <utility>
#include <functional>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

UbootProcessor::UbootProcessor(std::string *uboot_version)
    : uboot_version_(uboot_version),
    found_uboot_version_(false) {}

void UbootProcessor::ProcessLine(const string& line) {
    string uboot_sentinel("ubootv=");

    if (line.compare(0, uboot_sentinel.length(), uboot_sentinel) == 0) {
        *uboot_version_ =
             line.substr(uboot_sentinel.length(), line.size() - uboot_sentinel.length() - 1);
        VLOG(2) << "fw_printenv reports ubootv <" << *uboot_version_ << ">"; //NO_SPELL
        found_uboot_version_ = true;
    }
}

void UbootProcessor::ProcessPCloseResult(int pclose_result) {}

bool UbootProcessor::Success() {
    return found_uboot_version_;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
