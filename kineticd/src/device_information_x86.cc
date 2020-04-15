#include "device_information.h"

#include "glog/logging.h"
#include <stdlib.h>
#include "smrdisk/DriveEnv.h"
#include "primary_store.h"

using com::seagate::kinetic::DeviceInformation;

/* This file contains a dummy implementation of the DeviceInformation class for
 * use on development machines. The real production code is in
 * device_information_arm.cc.
 */
//const uint64_t DeviceInformation::NOMINAL_CAPACITY = (uint64_t)8000000000000;

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
        storage_partition_path_(storage_partition_path),
        proc_stat_path_(proc_stat_path),
        storage_device_(storage_device),
        sysfs_temperature_dir_(sysfs_temperature_dir),
        kineticd_start_log_(kineticd_start_log),
        capacity_in_bytes_(capacity_in_bytes) {
    }

bool DeviceInformation::Authorize(int64_t user_id, RequestContext& request_context) {
    return authorizer_.AuthorizeGlobal(user_id, Domain::kGetLog, request_context);
}

string DeviceInformation::GetKineticdStartLog() {
    return kineticd_start_log_;
}

bool DeviceInformation::GetCapacity(uint64_t* total_bytes, uint64_t* used_bytes) {
    smr::DriveEnv::getInstance()->GetCapacity(total_bytes, used_bytes);
    return true;
}

uint64_t DeviceInformation::GetNominalCapacityInBytes() {
    return capacity_in_bytes_;
}

bool DeviceInformation::GetPortionFull(float* portionFull) {
    uint64_t totalBytes;
    uint64_t usedBytes;
    if (!GetCapacity(&totalBytes, &usedBytes)) {
        return false;
    }

    *portionFull = std::min(double(usedBytes)/(totalBytes - PrimaryStore::kMinFreeSpace), 1.0);
    return true;
}


bool DeviceInformation::GetHdaUtilization(float* hda_utilization) {
    VLOG(2) << "Returning canned HDA utilization";//NO_SPELL
    *hda_utilization = 0.4;

    return true;
}

bool DeviceInformation::GetEn0Utilization(float* en0_utilization) {
    VLOG(2) << "Returning canned en0 utilization";//NO_SPELL
    *en0_utilization = 0.3;

    return true;
}

bool DeviceInformation::GetEn1Utilization(float* en1_utilization) {
    VLOG(2) << "Returning canned en1 utilization";//NO_SPELL
    *en1_utilization = 0.35;

    return true;
}


bool DeviceInformation::GetCpuUtilization(float* cpu_idle_percent) {
    VLOG(2) << "Returning canned CPU utilization";
    *cpu_idle_percent = 0.5;

    return true;
}

bool DeviceInformation::GetCpuTemp(
        float* current, float* min, float* max, float* target) {
    VLOG(2) << "Returning canned CPU temperature";
    *current = 30.0;
    *min = 25.0;
    *max = 35.0;
    *target = 22.0;

    return true;
}

bool DeviceInformation::GetDriveIdentification(std::string* drive_wwn,
        std::string* drive_sn,
        std::string* drive_vendor,
        std::string* drive_model) {
    *drive_wwn = "Drive WWN";
    *drive_sn = "Drive SN";
    *drive_vendor = "Seagate";
    *drive_model = "Drive Model";
    return true;
}

bool DeviceInformation::LoadDriveIdentification() {
    return true;
}

bool DeviceInformation::GetSMART(std::string* smart_log, int* smartctl_return_code) {
    *smart_log = "SMART log test string";
    *smartctl_return_code = 1234;
    return true;
}

bool DeviceInformation::GetSMARTAttributes(std::map<string, smart_values>* attributes) {
    struct smart_values attribute_values, hda_temp_values;
    attribute_values.value = 100;
    attribute_values.worst = 100;
    attribute_values.threshold = 100;

    hda_temp_values.value = 30;
    hda_temp_values.worst = 100;
    hda_temp_values.threshold = 25;

    (*attributes)[SMARTLogProcessor::SMART_ATTRIBUTE_START_STOP_COUNT] = attribute_values;
    (*attributes)[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_ON_HOURS] = attribute_values;
    (*attributes)[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_CYCLE_COUNT] = attribute_values;
    (*attributes)[SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE] = hda_temp_values;
    return true;
}

bool DeviceInformation::GetF3Version(std::string* version) {
    *version = "F3 Version";
    return true;
}

bool DeviceInformation::GetUbootVersion(std::string* version) {
    *version = "Uboot Version";
    return true;
}

bool DeviceInformation::GetNetworkStatistics(
        std::map<string, NetworkPackets>* interface_packet_information) {
    struct NetworkPackets eth0_packet_information, eth1_packet_information;
    eth0_packet_information.receive_packets = 100;
    eth0_packet_information.receive_drop = 0;
    eth0_packet_information.transmit_packets = 100;
    eth0_packet_information.transmit_drop = 0;

    eth1_packet_information.receive_packets = 100;
    eth1_packet_information.receive_drop = 0;
    eth1_packet_information.transmit_packets = 100;
    eth1_packet_information.transmit_drop = 0;

    (*interface_packet_information)["eth0"] = eth0_packet_information;
    (*interface_packet_information)["eth1"] = eth1_packet_information;
    return true;
}
