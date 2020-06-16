#include "device_information.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>

#include <fstream>
#include <vector>

#include "glog/logging.h"

#include "authorizer_interface.h"
#include "domain.h"
#include "smartlog_processors.h"
#include "uboot_processor.h"
// #include "net_processor.h"
#include "primary_store.h"
#include "popen_wrapper.h"
#include "smrdisk/DriveEnv.h"

using com::seagate::kinetic::DeviceInformation;
using std::string;

static const uint64_t BYTES_PER_MBPS = 1000000 / 8;

static const int NORMALIZE_TEMP_CONSTANT = 1000;

DeviceInformation::DeviceInformation(
    AuthorizerInterface &authorizer,
    const std::string &storage_partition_path,
    const std::string &proc_stat_path,
    const std::string &storage_device,
    const std::string &sysfs_temperature_dir,
    const std::string &preused_file_path,
    const std::string &kineticd_start_log,
    uint64_t capacity_in_bytes) :
        authorizer_(authorizer),
        drive_wwn_(""),
        drive_sn_(""),
        drive_vendor_(""),
        drive_model_(""),
        storage_partition_path_(storage_partition_path),
        proc_stat_path_(proc_stat_path),
        storage_device_(storage_device),
        sysfs_temperature_dir_(sysfs_temperature_dir),
        preused_file_path_(preused_file_path),
        kineticd_start_log_(kineticd_start_log),
        capacity_in_bytes_(capacity_in_bytes) {}

bool DeviceInformation::Authorize(int64_t user_id, RequestContext& request_context) {
    return authorizer_.AuthorizeGlobal(user_id, Domain::kGetLog, request_context);
}

uint64_t DeviceInformation::GetNominalCapacityInBytes() {
    return capacity_in_bytes_;
}

string DeviceInformation::GetKineticdStartLog() {
    return kineticd_start_log_;
}

bool DeviceInformation::GetPortionFull(float* portionFull) {
    uint64_t totalBytes;
    uint64_t usedBytes;
    if (!GetCapacity(&totalBytes, &usedBytes)) {
        return false;
    }

    const uint64_t minFunctionalBytes = PrimaryStore::kMinFreeSpace;
    // Linear interpolation of space used relative to space allocated
    // for user data
    // need to subtract out used bytes from primary.db
    *portionFull = std::min(double(usedBytes)/(totalBytes - minFunctionalBytes), 1.0);
    return true;
}

bool DeviceInformation::GetCapacity(uint64_t* total_bytes, uint64_t* used_bytes) {
    smr::DriveEnv::getInstance()->GetCapacity(total_bytes, used_bytes);
    return true;
}


bool DeviceInformation::GetHdaUtilization(float* hda_utilization) {
    VLOG(2) << "Reading hda utilization";//NO_SPELL

    std::string stat_file_path = "/sys/block/" + storage_device_ + "/stat";
    FILE* drive_stat_handle = fopen(stat_file_path.c_str(), "r");
    if (!drive_stat_handle) {
        LOG(WARNING) << "Unable to open " << stat_file_path << " file";
        return false;
    }

    uint64_t io_ticks_start, io_ticks_end;
    if (fscanf(drive_stat_handle,
            "%*u %*u %*u %*u %*u %*u %*u %*u %*u %" PRIu64, &io_ticks_start) != 1) {
        LOG(WARNING) << stat_file_path << " file in unexpected format";
        fclose(drive_stat_handle);
        return false;
    }
    sleep(1);

    if (fseek(drive_stat_handle, 0, SEEK_SET)) {
        LOG(WARNING) << "Unable to rewind " << stat_file_path;
        fclose(drive_stat_handle);
        return false;
    }


    if (fscanf(drive_stat_handle,
            "%*u %*u %*u %*u %*u %*u %*u %*u %*u %" PRIu64, &io_ticks_end) != 1) {
        LOG(WARNING) << stat_file_path << " file in unexpected format";
        fclose(drive_stat_handle);
        return false;
    }

    fclose(drive_stat_handle);

    float utilization = (float)(io_ticks_end - io_ticks_start) / 1000;

    VLOG(2) << "Over 1 sec io_ticks: " << io_ticks_start << " -> " << io_ticks_end//NO_SPELL
            << " utilization=" << utilization;


    *hda_utilization = utilization;
    return true;
}

bool DeviceInformation::GetEnUtilization(const std::string& ethernet_device,
        float* en_utilization) {
    VLOG(2) << "Reading << " << ethernet_device << " utilization";

    std::string rx_bytes_path = "/sys/class/net/" + ethernet_device + "/statistics/rx_bytes";
    FILE* rx_bytes_handle = fopen(rx_bytes_path.c_str(), "r");
    if (!rx_bytes_handle) {
        LOG(WARNING) << "Unable to open rx_bytes stats file";//NO_SPELL
        return false;
    }

    std::string tx_bytes_path = "/sys/class/net/" + ethernet_device + "/statistics/tx_bytes";
    FILE* tx_bytes_handle = fopen(tx_bytes_path.c_str(), "r");
    if (!tx_bytes_handle) {
        fclose(rx_bytes_handle);
        LOG(WARNING) << "Unable to open tx_bytes stats file";//NO_SPELL
        return false;
    }

    std::string link_speed_path = "/sys/class/net/" + ethernet_device + "/speed";
    FILE* link_speed_handle = fopen(link_speed_path.c_str() , "r");
    if (!link_speed_handle) {
        fclose(rx_bytes_handle);
        fclose(tx_bytes_handle);
        LOG(WARNING) << "Unable to open link speed file: " << link_speed_path;
        return false;
    }
    uint64_t link_speed_mbps;
    fscanf(link_speed_handle, "%" PRIu64, &link_speed_mbps);
    fclose(link_speed_handle);



    uint64_t rx_bytes_start, rx_bytes_end, tx_bytes_start, tx_bytes_end;
    fscanf(rx_bytes_handle, "%" PRIu64, &rx_bytes_start);
    fscanf(tx_bytes_handle, "%" PRIu64, &tx_bytes_start);

    sleep(1);
    fseek(rx_bytes_handle, 0, SEEK_SET);
    fseek(tx_bytes_handle, 0, SEEK_SET);

    fscanf(rx_bytes_handle, "%" PRIu64, &rx_bytes_end);
    fscanf(tx_bytes_handle, "%" PRIu64, &tx_bytes_end);

    fclose(tx_bytes_handle);
    fclose(rx_bytes_handle);

    uint64_t bytes_per_second = (tx_bytes_end - tx_bytes_start) + (rx_bytes_end - rx_bytes_start);
    uint64_t ethernet_link_capacity_bps_ = link_speed_mbps * BYTES_PER_MBPS;;
    float utilization = (double)bytes_per_second / ethernet_link_capacity_bps_;

    VLOG(2) << ethernet_device << " utilization: " << utilization
            << " tx " << tx_bytes_start << " -> " << tx_bytes_end//NO_SPELL
            << " rx " << rx_bytes_start << " -> " << rx_bytes_end//NO_SPELL
            << " " << bytes_per_second << " bytes/sec";//NO_SPELL

    *en_utilization = utilization;
    return true;
}

bool ReadProcStatPath(const string& proc_stat_path, uint64_t* total_time, uint64_t* idle_time) {
    VLOG(2) << "Reading CPU stats from " << proc_stat_path;

    FILE* stat_handle = fopen(proc_stat_path.c_str(), "r");
    if (!stat_handle) {
        LOG(WARNING) << "Unable to open " << proc_stat_path;
        return false;
    }

    int user_time = 0;
    int nice_time = 0;
    int cpu_idle_time;
    int system_time = 0;
    int iowait_time = 0;
    int irq_time = 0;
    int softirq_time = 0;

    int items_assigned = fscanf(stat_handle, "cpu %d %d %d %d %d %d %d", &user_time, &nice_time,
        &system_time, &cpu_idle_time, &iowait_time, &irq_time, &softirq_time);

    if (fclose(stat_handle)) {
        LOG(WARNING) << "Unable to close " << proc_stat_path;
    }

    static const int expected_assignments = 7;

    if (items_assigned != expected_assignments) {
        LOG(WARNING) << "Expected to assign " << expected_assignments
                    << " but assigned " << items_assigned;
        return false;
    }

    VLOG(2) << "CPU stats: user_time: " << user_time//NO_SPELL
            << " nice_time: " << nice_time//NO_SPELL
            << " system_time: " << system_time//NO_SPELL
            << " idle_time: " << idle_time//NO_SPELL
            << " iowait_time: " << iowait_time//NO_SPELL
            << " irq_time: " << irq_time
            << " softirq_time: " << softirq_time;//NO_SPELL

    *total_time = user_time + nice_time + system_time + cpu_idle_time + iowait_time +
        irq_time + softirq_time;

    // Time the CPU spent waiting for IO to finish is really idle time because the
    // CPU wasn't doing any work. We don't want situations where IO is the bottleneck
    // to appear CPU-constrained.
    *idle_time = cpu_idle_time + iowait_time;

    if (*total_time <= 0) {
        LOG(WARNING) << "Total system time is 0";
        return false;
    }

    return true;
}

bool DeviceInformation::GetCpuUtilization(float* cpu_idle_percent) {
    VLOG(2) << "Reading CPU utilization";

    uint64_t start_total_time;
    uint64_t start_idle_time;
    uint64_t end_total_time;
    uint64_t end_idle_time;

    if (!ReadProcStatPath(proc_stat_path_, &start_total_time, &start_idle_time)) {
        return false;
    }

    sleep(1);

    if (!ReadProcStatPath(proc_stat_path_, &end_total_time, &end_idle_time)) {
        return false;
    }

    uint64_t total_time = end_total_time - start_total_time;
    uint64_t idle_time = end_idle_time - start_idle_time;

    VLOG(2) << "Measured CPU utilization over " << total_time << " and was idle for " << idle_time;

    *cpu_idle_percent = (float)idle_time / total_time;
    return true;
}

bool DeviceInformation::GetCpuTemp(
    float* current, float* min, float* max, float* target) {

    std::ifstream current_value((sysfs_temperature_dir_ + "temp1_input").c_str());
    if (!current_value.is_open()) {
        *current = -1;
    } else {
        current_value >> *current;
        *current = *current / NORMALIZE_TEMP_CONSTANT;
    }

    std::ifstream min_value((sysfs_temperature_dir_ + "temp1_min").c_str());
    if (!min_value.is_open()) {
        *min = -1;
    } else {
        min_value >> *min;
        *min = *min / NORMALIZE_TEMP_CONSTANT;
    }

    std::ifstream max_value((sysfs_temperature_dir_ + "temp1_max").c_str());
    if (!max_value.is_open()) {
        *max = -1;
    } else {
        max_value >> *max;
        *max = *max / NORMALIZE_TEMP_CONSTANT;
    }

    return true;
}

bool DeviceInformation::GetDriveIdentification(std::string* drive_wwn, std::string* drive_sn,
        std::string* drive_vendor, std::string* drive_model) {
    std::lock_guard<std::mutex> lock(query_mutex_);
    if ((0 == drive_wwn_.length()) || (0 == drive_sn_.length()) ||
            (0 == drive_vendor_.length()) || (0 == drive_model_.length())) {
        DriveIdSMARTLogProcessor processor(&drive_wwn_, &drive_sn_, &drive_model_, &drive_fv_);
        std::string smartctl_command = "/usr/sbin/smartctl -i /dev/" + storage_device_;
        if (!execute_command(smartctl_command, processor)) {
            return false;
        }
        drive_vendor_ = "Seagate";
    }
    *drive_wwn = drive_wwn_;
    *drive_sn = drive_sn_;
    *drive_vendor = drive_vendor_;
    *drive_model = drive_model_;
    return true;
}

bool DeviceInformation::LoadDriveIdentification() {
    if ((0 == drive_wwn_.length()) || (0 == drive_sn_.length()) ||
            (0 == drive_vendor_.length()) || (0 == drive_model_.length())) {
        DriveIdSMARTLogProcessor processor(&drive_wwn_, &drive_sn_, &drive_model_, &drive_fv_);
        std::string smartctl_command = "/usr/sbin/smartctl -i /dev/" + storage_device_;
        if (!execute_command(smartctl_command, processor)) {
            return false;
        }
        drive_vendor_ = "Seagate";
    }
    return true;
}

bool DeviceInformation::GetSMART(std::string* smart_log, int* smartctl_return_code) {
    string smartctl_command = "/usr/sbin/smartctl -a /dev/" + storage_device_;
    RawStringProcessor processor(smart_log, smartctl_return_code);
    return execute_command(smartctl_command, processor);
}

bool DeviceInformation::GetSMARTAttributes(std::map<string, smart_values>* attributes) {
    std::string smartctl_command = "/usr/sbin/smartctl -A /dev/" + storage_device_;
    SMARTLogProcessor processor(attributes);
    return execute_command(smartctl_command, processor);
}

bool DeviceInformation::GetF3Version(std::string* version) {
    std::lock_guard<std::mutex> lock(query_mutex_);
    if (0 == drive_fv_.length()) {
        DriveIdSMARTLogProcessor processor(&drive_wwn_, &drive_sn_, &drive_model_, &drive_fv_);
        std::string smartctl_command = "/usr/sbin/smartctl -i /dev/" + storage_device_;
        if (!execute_command(smartctl_command, processor)) {
            return false;
        }
    }
    *version = drive_fv_;
    return true;
}

bool DeviceInformation::GetUbootVersion(std::string* version) {
    UbootProcessor processor(version);
    std::string dmesg_ubootv = "dmesg | grep -o 'ubootv=[0-9].*'"; // Option B: parse boot args
    bool result_status = false;

    if (execute_command(dmesg_ubootv, processor)) {
        if (!version->empty()) {
            result_status = true;
        }
    }
    return result_status;
}

bool DeviceInformation::GetNetworkStatistics(
        std::map<string, NetworkPackets>* interface_packet_information) {
    NetProcessor processor(interface_packet_information);
    std::string net_command = "cat /proc/net/dev";
    return execute_command(net_command, processor);
}
