#ifndef KINETIC_DEVICE_INFORMATION_H_
#define KINETIC_DEVICE_INFORMATION_H_

#include "glog/logging.h"

#include <vector>
#include <mutex>
#include <map>

#include "authorizer_interface.h"
#include "device_information_interface.h"
#include "request_context.h"
#include "product_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

class DeviceInformation : public DeviceInformationInterface {
    public:
        static const uint64_t NOMINAL_CAPACITY = PRODUCT_CAPACITY;  // Capacity in bytes

    DeviceInformation(
        AuthorizerInterface &authorizer,
        const std::string &storage_partition_path,
        const std::string &proc_stat_path,
        const std::string &storage_device,
        const std::string &sysfs_temperature_dir,
        const std::string &preused_file_path,
        const std::string &kineticd_start_log);

    virtual bool Authorize(int64_t user_id, RequestContext& request_context);

    virtual bool GetCapacity(uint64_t* total_bytes, uint64_t* used_bytes);
    virtual uint64_t GetNominalCapacityInBytes();
    virtual bool GetPortionFull(float* portionFull);

    virtual bool GetHdaUtilization(float* hda_utilization);
    virtual bool GetEn0Utilization(float* en0_utilization);
    virtual bool GetEn1Utilization(float* en1_utilization);
    virtual bool GetCpuUtilization(float* cpu_idle_percent);

    virtual bool GetCpuTemp(float* current, float* min, float* max, float* target);

    virtual bool GetDriveIdentification(std::string* drive_wwn, std::string* drive_sn, \
            std::string* drive_vendor, std::string* drive_model);

    virtual bool LoadDriveIdentification();

    virtual bool GetSMART(std::string* smart_log, int* smartctl_return_code);

    virtual bool GetSMARTAttributes(std::map<std::string, smart_values>*);

    virtual bool GetF3Version(std::string* version);

    virtual bool GetUbootVersion(std::string* version);

    virtual bool GetNetworkStatistics(std::map<string, NetworkPackets>*);

    virtual string GetKineticdStartLog();

    private:
    // Threadsafe; AuthorizerInterface implementations must be threadsafe
    AuthorizerInterface &authorizer_;

    std::string drive_wwn_;
    std::string drive_sn_;
    std::string drive_vendor_;
    std::string drive_model_;
    std::string drive_fv_;

    std::mutex query_mutex_;

    const std::string storage_partition_path_;
    const std::string proc_stat_path_;
    const std::string storage_device_;
    const std::string sysfs_temperature_dir_;
    const std::string preused_file_path_;
    const std::string kineticd_start_log_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DEVICE_INFORMATION_H_
