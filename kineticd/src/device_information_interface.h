#ifndef KINETIC_DEVICE_INFORMATION_INTERFACE_H_
#define KINETIC_DEVICE_INFORMATION_INTERFACE_H_

#include <inttypes.h>

#include <string>
#include <vector>

#include "request_context.h"
#include "smartlog_processors.h"
#include "net_processor.h"

namespace com {
namespace seagate {
namespace kinetic {

/**
* Implementations must be threadsafe
*/
class DeviceInformationInterface {
    public:
    virtual ~DeviceInformationInterface() {}

    virtual bool Authorize(int64_t user_id, RequestContext& request_context) = 0;

    virtual bool GetCapacity(uint64_t* total_bytes, uint64_t* used_bytes) = 0;
    virtual uint64_t GetNominalCapacityInBytes() = 0;
    virtual bool GetPortionFull(float* portionFull) = 0;

    virtual bool GetHdaUtilization(float* hda_utilization) = 0;
    virtual bool GetEn0Utilization(float* en0_utilization) = 0;
    virtual bool GetEn1Utilization(float* en1_utilization) = 0;
    virtual bool GetCpuUtilization(float* cpu_idle_percent) = 0;

    virtual bool GetCpuTemp(float* current, float* min, float* max, float* target) = 0;

    virtual bool GetDriveIdentification(std::string* drive_wwn,
            std::string* drive_sn,
            std::string* drive_vendor,
            std::string* drive_model) = 0;

    virtual bool GetSMART(std::string* smart_log, int* smartctl_return_code) = 0;

    virtual bool GetSMARTAttributes(std::map<std::string, smart_values>*) = 0;

    virtual bool GetF3Version(std::string* version) = 0;

    virtual bool GetUbootVersion(std::string* version) = 0;

    virtual bool GetNetworkStatistics(std::map<string, NetworkPackets>*) = 0;

    virtual string GetKineticdStartLog() = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DEVICE_INFORMATION_INTERFACE_H_
