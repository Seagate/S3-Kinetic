#ifndef KINETIC_MOCK_DEVICE_INFORMATION_H_
#define KINETIC_MOCK_DEVICE_INFORMATION_H_

#include <vector>

#include "gmock/gmock.h"

#include "device_information_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockDeviceInformation : public DeviceInformationInterface {
    public:
    MockDeviceInformation() {}

    MOCK_METHOD0(GetNominalCapacityInBytes, uint64_t());
    MOCK_METHOD2(Authorize, bool(int64_t user_id, RequestContext& request_context));
    MOCK_METHOD1(GetPortionFull, bool(float* portionFull));

    MOCK_METHOD2(GetCapacity, bool(uint64_t* total, uint64_t* remaining));

    MOCK_METHOD1(GetHdaUtilization, bool(float* hda_utilization));
    MOCK_METHOD2(GetEnUtilization, bool(const std::string& ethernet_device, float* en_utilization));
    MOCK_METHOD1(GetCpuUtilization, bool(float* cpu_idle_percent));

    MOCK_METHOD4(GetHdaTemp, bool(float* current, float* min, float* max, float* target));
    MOCK_METHOD4(GetCpuTemp, bool(float* current, float* min, float* max, float* target));

    MOCK_METHOD4(GetDriveIdentification, bool(std::string* drive_wwn, std::string* drive_sn,
            std::string* drive_vendor, std::string* drive_model));

    MOCK_METHOD2(GetSMART, bool(std::string* smart_log, int* smartctl_return_code));
    MOCK_METHOD1(GetSMARTAttributes, bool(std::map<string, smart_values>* attributes));
    MOCK_METHOD1(GetF3Version, bool(std::string *version));
    MOCK_METHOD1(GetUbootVersion, bool(std::string *version));
    MOCK_METHOD1(GetNetworkStatistics, bool(std::map<string, NetworkPackets>*));
    MOCK_METHOD0(GetKineticdStartLog, string());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_DEVICE_INFORMATION_H_
