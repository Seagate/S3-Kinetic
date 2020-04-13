#include <stdlib.h>
#include <stdio.h>

#include <vector>

#include "glog/logging.h"

#include "device_information.h"
#include "network_interfaces.h"
#include "null_authorizer.h"
#include "smartlog_processors.h"
#include "stack_trace.h"
#include "command_line_flags.h"

using com::seagate::kinetic::DeviceInformation;
using com::seagate::kinetic::NetworkInterfaces;
using com::seagate::kinetic::NullAuthorizer;
using com::seagate::kinetic::DeviceNetworkInterface;
using com::seagate::kinetic::smart_values;
using com::seagate::kinetic::SMARTLogProcessor;

int main(int argc, char *argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;
    FLAGS_stderrthreshold = google::INFO;

    if (argc != 2) {
        printf("Usage: %s <storage partition path>\n", argv[0]);
        return 1;
    }

    NullAuthorizer authorizer;
    DeviceInformation device_information(authorizer, argv[1], "/proc/stat",
        "sda", "/sys/devices/platform/axp-temp.0/", "fsize", FLAGS_kineticd_start_log);
    NetworkInterfaces network_interfaces;

    printf("Reading device information\n");

    uint64_t total_bytes, used_bytes;
    CHECK(device_information.GetCapacity(&total_bytes, &used_bytes));
    printf("Capacity: used bytes %.0f / %.0f\n", (float)used_bytes, (float)total_bytes);

    float cpu_idle_percent;
    CHECK(device_information.GetCpuUtilization(&cpu_idle_percent));
    printf("CPU Idle: %f\n", cpu_idle_percent);

    float en0_usage;
    CHECK(device_information.GetEn0Utilization(&en0_usage));
    printf("EN0 Utilization: %f\n", en0_usage);

    float en1_usage;
    CHECK(device_information.GetEn1Utilization(&en1_usage));
    printf("EN1 Utilization: %f\n", en1_usage);

    float hda_utilization;
    CHECK(device_information.GetHdaUtilization(&hda_utilization));
    printf("HDA Utilization: %f\n", hda_utilization);

    float cpu_current, cpu_min, cpu_max, cpu_target;
    CHECK(device_information.GetCpuTemp(&cpu_current, &cpu_min, &cpu_max, &cpu_target));
    printf("CPU Temp: current=%f, min=%f, max=%f, target=%f\n",
        cpu_current, cpu_min, cpu_max, cpu_target);

    std::string drive_wwn;
    std::string drive_sn;
    std::string drive_vendor;
    std::string drive_model;

    CHECK(device_information.GetDriveIdentification(&drive_wwn,
            &drive_sn, &drive_vendor, &drive_model));
    printf("Drive WWN: %s\n", drive_wwn.c_str());
    printf("Drive SN: %s\n", drive_sn.c_str());
    printf("Drive Vendor: %s\n", drive_vendor.c_str());
    printf("Drive Model: %s\n", drive_model.c_str());

    std::vector<DeviceNetworkInterface> interfaces;
    CHECK(network_interfaces.GetExternallyVisibleNetworkInterfaces(&interfaces));
    for (std::vector<DeviceNetworkInterface>::iterator interface = interfaces.begin();
        interface != interfaces.end();
        interface++) {
        printf("Interface %s\n", interface->name.c_str());
        printf("  %s\n", interface->mac_address.c_str());
        printf("  %s\n", interface->ipv4.c_str());
        printf("  %s\n", interface->ipv6.c_str());
    }

    std::map<std::string, smart_values> attributes;
    CHECK(device_information.GetSMARTAttributes(&attributes));
    printf("Start_Stop_Count: value = %d, worst = %d, threshold = %d\n",
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_START_STOP_COUNT].value,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_START_STOP_COUNT].worst,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_START_STOP_COUNT].threshold);
    printf("Power_On_Hours: value = %d, worst = %d, threshold = %d\n",
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_ON_HOURS].value,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_ON_HOURS].worst,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_ON_HOURS].threshold);
    printf("Power_Cycle_Count: value = %d, worst = %d, threshold = %d\n",
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_CYCLE_COUNT].value,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_CYCLE_COUNT].worst,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_POWER_CYCLE_COUNT].threshold);
    printf("HDA Temp: current = %d, max = %d, target = %d\n",
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE].value,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE].worst,
        attributes[SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE].threshold);

    std::string smart_log;
    int smartctl_return_code;
    CHECK(device_information.GetSMART(&smart_log, &smartctl_return_code));
    printf("smartctl returned %d\n", smartctl_return_code);
    printf("SMART log:\n%s\n", smart_log.c_str());

    return EXIT_SUCCESS;
}
