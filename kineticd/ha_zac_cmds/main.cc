#include "includes/zac_mediator.h"
#include <ios>
#include <iomanip>
#include <utility>
#include <stdlib.h>
#include <syslog.h>
#include <sstream>
#include <string.h>
#include <memory>
#include <fstream>
#include <fcntl.h>
#include <algorithm>

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using ::zac_ha_cmd::ZacZone;
static const unsigned int kLAMARR_TOTAL_CONV_ZONES = 64;
static const unsigned int kLAMARR_TOTAL_ZONES = 29809;
static const unsigned int ZD_PAGE_SIZE = 512;
static const unsigned int ZD_PAGE_HEADER_SZ = 64;
static const unsigned int ZAC_ZONE_DESC_LENGTH = 64;
static const unsigned int ZAC_ZONE_DESC_OFFSET = 64;
const std::string kSCSI_DEV_QUERY = "lsscsi | grep -o '/dev/sd[A-Za-z]'";

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
/// Host Aware Command Set Standalone Tests
/// -------------------------------------------------------------------------------
/// @Summary:
/// Building Zac_local For ARM
///    1. `source` uboot-linux/envsetup.sh
///    2. `arm_crosscompile`
///    3. `make ARM=yes`
///    4. Produces executable `zac_local`
///    5. To Run; scp executable to Host Aware LamarrKV
///        * `./zac_local /dev/sda`
///
///
/// Building Zac_local for X86
///    * `make`
///    * Produces executable `zac_local`
///    * To Run; need a host aware drive on your x86 workstation,
///      pass the drive handle to the executable
///        * `sudo ./zac_local /dev/sdb`
/// -------------------------------------------------------------------------------
/// Lamarr Host Aware File Heirarchy
///        kineticd/
///             └── ha_zac_cmds/
///             │   ├── includes/
///             │   │    │
///             │   │    ├── ata_cmd_handler.h
///             │   │    ├── zac_mediator.h
///             │   │    └── zoned_ata_standards.h
///             │   ├── ata_cmd_handler.cc
///             │   ├── zac_mediator.cc
///             │   ├── main.cc  --  CURRENT FILE - "Standalone `zac_local` library functions"
///             │   └── makefile
///             │
///             ├── src/
///             │    └── zac_ha_exercise_drive.cc  --  "X86 Unit Tests"
///             │
///             └── CMakeLists.txt  --  "Compilation Rules for linking to SMRDB"
/// -------------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////////
/// CalcReturnPage()  -- Helper / Utility Function
/// Utility Function to Verify Return Page Size & num of zones in return page count
uint64_t CalcReturnPage(unsigned int num_zones) {
    unsigned int res = (((num_zones * ZAC_ZONE_DESC_LENGTH) + ZD_PAGE_HEADER_SZ) + 511) & ~511;
    unsigned int page_count = res / ZD_PAGE_SIZE;
    unsigned int num_zones_reported = page_count * 7;
    unsigned int num_zones_reportedb = (res - ZD_PAGE_HEADER_SZ) / ZAC_ZONE_DESC_LENGTH;

    std::cout << "Zac_Local Test::CalcReturnPage\n "
              << "Buffersize for (numzones: "
              << std::dec << num_zones << ") = "
              << res << " Page Count: "
              << page_count << " #Zones:" << num_zones_reported
              << " #Zones(alt):" <<num_zones_reportedb
              << ";" << std::endl;
    return res;
}

///////////////////////////////////////////////////////////////////////////////////
/// GetZoneDescList()
/// -------------------------------------------------------------------------------
/// Sequentially populate @num_zones zaczone structs with condition of @roption
void GetZoneDescList(ZacMediator* zac_kin,
                     unsigned int start_zone,
                     unsigned int roption,
                     unsigned int num_zones) {
    unsigned int starting_zone_id = start_zone;
    unsigned int report_option = roption;
    unsigned int num_zones_requested = num_zones;
    ZacZone *zone_list = NULL;

    zone_list = (ZacZone *) malloc(sizeof(ZacZone) * num_zones_requested);
    if (!zone_list) {
        std::cerr << "Zac_Local Test::GetZoneDescList malloc ZacZone array fail" << std::endl;
        return;
    }

    bool res = zac_kin->GetZoneDescriptorList(zone_list,
                                              starting_zone_id,
                                              report_option,
                                              num_zones_requested);

    std::cout << "Zac_Local Test::GetZoneDescList Returned: "
              << std::boolalpha << res << "\n-------------\n" << std::endl;
    if (zone_list) {
        free(zone_list);
    }
}

///////////////////////////////////////////////////////////////////////////////////
/// RetreiveZonePair()
/// -------------------------------------------------------------------------------
/// Report Zones & populate two zac zone structs (can be disjoint or neigboring / sequential zones)
bool RetreiveZonePair(ZacMediator* zac_kin, uint64_t starting_zone_id, uint64_t second_zone_id) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::RetreiveZonePair(starting_zone_id:"
              << starting_zone_id << ", second_zone_id:"
              << second_zone_id << ")" << std::endl;
    bool result = true;
    ZacZone *zone_list = NULL;
    zone_list = (ZacZone *) malloc(sizeof(ZacZone) * 2);
    if (!zone_list) {
        std::cerr << "Zac_Local Test::RetreiveZonePair() \"malloc(sizeof(ZacZone) * 2\" [FAILED]" << std::endl;
        return false;
    }

    memset(zone_list, 0, sizeof(ZacZone) * 2);
    result = zac_kin->GetPairZoneDescriptors(zone_list, starting_zone_id, second_zone_id);

    if (result) zac_kin->PrintZoneDescriptors(zone_list, 2);
    if (zone_list) {
        free(zone_list);
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////////
/// DriveZoneReport()
/// -------------------------------------------------------------------------------
/// Snap shot showing # of zones for each zone condition
bool DriveZoneReport(ZacMediator* zac_kin) {
    std::set<unsigned int> snapshot_conditions {kZAC_RO_EMPTY, kZAC_RO_IMP_OPEN,
                                               kZAC_RO_EXP_OPEN, kZAC_RO_FULL, kZAC_RO_NON_SEQ};
    std::cout << "Zac_Local Test::DriveZoneReport \n";
    std::string report_snapshot = zac_kin->GetZoneConditionSnapShot();
    std::cout << "DriveZoneReport::Result Report Snapshot \n"
              << report_snapshot << std::endl;
    std::string report_whitelist = zac_kin->GetZoneConditionSnapShot(snapshot_conditions);
    std::cout << "DriveZoneReport::Result Report(whitelist) \n"
              << report_whitelist << std::endl;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////
/// TestResetWriteReadZone()
/// -------------------------------------------------------------------------------
/// Reset zoneid 300, write data to zone, read from zone
void TestResetWriteReadZone(ZacMediator* zac_kin) {
    std::cout << "Zac_Local Test::TestResetWriteReadZone" << std::endl;
    int ret = 0;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    unsigned int zone_id = 300;
    uint64_t wp_lba = 0;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        std::cerr << "TestResetWriteReadZone()::FAILED posix memalign" << std::endl;
        return;
    }
    memset(iobuf, 0x41, 4096);

    if (zac_kin->AllocateZone(zone_id, &wp_lba) != 0) {
        std::cerr << "TestResetWriteReadZone()::FAIL "
                  << "zac_kin->AllocateZone(" << zone_id << ")" << std::endl;
        return;
    }

    zac_kin->GetZoneInfo(&zone, zone_id);
    std::cout << "\n======= Get Zone: 65 Info #1 (POST ResetWP #1) ======== \n"
              << "zone->start_lba: " << zone.start_lba << "\n"
              << "zone->write_pointer: " << zone.write_pointer << "\n"
              << "zone->zone_length: " << zone.zone_length << "\n"
              << "------------------------------------------" << std::endl;

    uint64_t expected_lba = zone_id << 19;
    if (zone.write_pointer != expected_lba) {
        std::cerr << "TestResetWriteReadZone()::ERROR -> zone.write_pointer("
                  << zone.write_pointer << ") != expectedlba("
                  << expected_lba << ")" << std::endl;
        return;
    }

    if (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize) {
        std::cerr << "TestResetWriteReadZone::FAILED zac_kin->WriteZone("
                  << wp_lba << ", " << iosize << ")" << std::endl;
        if (iobuf) {
            free(iobuf);
        }
        return;
    }

    zac_kin->ReadZone(zone_id);

    if (zac_kin->AllocateZone(zone_id, &wp_lba) != 0) {
        std::cout << "TestResetWriteReadZone()::FAIL "
                  << "zac_kin->AllocateZone(" << zone_id << ")" << std::endl;
        return;
    }

    zac_kin->GetZoneInfo(&zone, zone_id);
    std::cout << "\n======= Get Zone: 65 Info #2 (POST ResetWP #2) ======== \n"
              << "zone->start_lba: " << std::dec << zone.start_lba << "\n"
              << "zone->zone_condition: " << zone.zone_condition << "\n"
              << "zone->write_pointer: " << zone.write_pointer << "\n"
              << "zone->zone_length: " << zone.zone_length << "\n"
              << "zone->zone_type: " << zone.zone_type << std::endl;

    if (iobuf) {
        free(iobuf);
    }
}

///////////////////////////////////////////////////////////////////////////////////
/// WriteAndRetreiveZone()
/// -------------------------------------------------------------------------------
/// Write 4Kib of '0x41' to zone @zone_id
/// Retreive zone information (wp position, state, length etc) before & after write
bool WriteAndRetreiveZone(ZacMediator* zac_kin, unsigned int zone_id) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::WriteAndRetreiveZone(zone_id: "
              << std::dec << zone_id << ")" << std::endl;
    int ret = 0;
    bool result = true;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba = 0;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        std::cerr << "    ---->> Zac_Local Test::WriteAndRetreiveZone()"
                  << " [FAILED]: posix memalign" << std::endl;
        return false;
    }
    memset(iobuf, 0x41, 4096);

    zac_kin->GetZoneInfo(&zone, zone_id);
    std::cout << "\nZac_Local Test::WriteAndRetreiveZone::Get Zone: \"" << zone_id
              << "\" Info (PRE WRITE)\n"
              << "zone->start_lba: " << zone.start_lba << "\n"
              << "zone->write_pointer: " << zone.write_pointer << "\n"
              << "zone->zone_length: " << zone.zone_length << std::endl;

    uint64_t expected_lba = zone_id << 19;
    if (zone.write_pointer != expected_lba) {
        std::cerr << "    ---->> Zac_Local Test::WriteAndRetreiveZone()"
                  << " [FAILED]: zone.write_pointer("
                  << zone.write_pointer << ") != expectedlba("
                  << expected_lba << ")" << std::endl;
        result = false;
    }

    if (result && (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize)) {
        std::cerr << "    ---->> Zac_Local Test::WriteAndRetreiveZone() [FAILED]:"
                  << "zac_kin->WriteZone("
                  << wp_lba << ", " << iosize << ")" << std::endl;
        result = false;
    }

    zac_kin->GetZoneInfo(&zone, zone_id);
    std::cout << "\nZac_Local Test::WriteAndRetreiveZone()::Get Zone: \"" << zone_id
              << "\" Info (POST WRITE)\n"
              << "zone->start_lba: " << std::dec << zone.start_lba << "\n"
              << "zone->zone_condition: " << zone.zone_condition << "\n"
              << "zone->write_pointer: " << zone.write_pointer << "\n"
              << "zone->zone_length: " << zone.zone_length << "\n"
              << "zone->zone_type: " << zone.zone_type << std::endl;

    if (iobuf) {
        free(iobuf);
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////////
/// ReportOpenZones()
/// -------------------------------------------------------------------------------
/// Report All Open Zones
bool ReportOpenZones(ZacMediator* zac_kin, unsigned int start_lba,
                     unsigned int num_req_zones, unsigned int *zone_count) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::ReportOpenZones()" << std::endl;
    int zone_count_result = 0;
    zone_count_result = zac_kin->ReportOpenZones(start_lba, num_req_zones);
    if (zone_count_result >= 0) {
        *zone_count = zone_count_result;
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// ReportAllZones()
/// -------------------------------------------------------------------------------
/// Report All zones on Device
bool ReportAllZones(ZacMediator* zac_kin, unsigned int start_zone_id,
                    unsigned int num_req_zones, unsigned int *zone_count) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::ReportAllZones(start_zone_id:"
              << start_zone_id << ", num_req_zones:"
              << num_req_zones << ")" << std::endl;

    unsigned int start_lba = start_zone_id << 19;
    int zone_count_result = 0;
    zone_count_result = zac_kin->ReportAllZones(start_lba, num_req_zones);
    if (zone_count_result >= 0) {
        *zone_count = zone_count_result;
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// AllocateZone()
/// -------------------------------------------------------------------------------
/// ResetWP for Zoneid @zid
bool AllocateZone(ZacMediator* zac_kin, unsigned int zid) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::AllocateZone(id: " << std::dec << zid << ")" << std::endl;
    unsigned int zone_id = zid;
    uint64_t lba = 0;
    if (zac_kin->AllocateZone(zone_id, &lba) != 0) {
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////
/// AllocateZone()
/// -------------------------------------------------------------------------------
/// ResetWP for Zoneid @zid
bool FinishZone(ZacMediator* zac_kin, unsigned int zone_id) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::FinishZone(zone_id:"
              << std::dec << zone_id << ")" << std::endl;
    unsigned int start_lba = zone_id << 19;
    if (zac_kin->FinishZone(start_lba) != 0) {
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////
/// OpenZone()
/// -------------------------------------------------------------------------------
/// Open zone id associated with lba 35651584
bool OpenZone(ZacMediator* zac_kin, const unsigned int zone_id) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::OpenZone(zone_id: " << std::dec << zone_id << ")" << std::endl;
    unsigned int start_lba = zone_id << 19;
    if (zac_kin->OpenZone(start_lba) == 0) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// OpenNumZones()
/// -------------------------------------------------------------------------------
/// Sequentially Open @number_of_zones starting from @start_zone_id
bool OpenNumZones(ZacMediator *zac_kin, int number_of_zones,
                  int64_t start_zone_id, unsigned int *num_open_result) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::OpenNumZones(count:"
              << number_of_zones << ", startzone:"
              << start_zone_id << ")" << std::endl;
    bool result = true;
    unsigned int success_count = 0;
    uint64_t start_lba = start_zone_id << 19;
    for (int i = 0; i < number_of_zones; i++) {
        if (zac_kin->OpenZone(start_lba) == 0) {
            success_count++;
        } else {
            result = false;
            break;
        }
        start_lba += 524288;
    }
    *num_open_result = success_count;
    return result;
}

///////////////////////////////////////////////////////////////////////////////////
/// OpenAllZones()
/// -------------------------------------------------------------------------------
/// Open all WP zones on Device
bool OpenAllZones(ZacMediator* zac_kin) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::OpenAllZones()" << std::endl;
    if (zac_kin->OpenAllZones() == 0) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// ResetAllZones()
/// -------------------------------------------------------------------------------
/// Reset Write Pointer for all WP sones on Device
bool ResetAllZones(ZacMediator* zac_kin) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::ResetAllZones()" << std::endl;
    if (zac_kin->ResetAllZones() == 0) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// CloseZone()
/// -------------------------------------------------------------------------------
/// Close zone associated with lba 35651584
bool CloseZone(ZacMediator* zac_kin, const unsigned int zone_id) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::CloseZone(zone_id: " << std::dec << zone_id << ")" << std::endl;
    unsigned int start_lba = zone_id << 19;
    if (zac_kin->CloseZone(start_lba) == 0) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// CloseNumZones()
/// -------------------------------------------------------------------------------
/// Sequentially Close @number_of_zones starting from @start_zone_id
bool CloseNumZones(ZacMediator *zac_kin, int number_of_zones, uint64_t start_zone_id, unsigned int *success_ctr) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::CloseNumZones(count:"
              << number_of_zones << ", startzone:"
              << start_zone_id << ")" << std::endl;
    bool result = true;
    unsigned int success_count = 0;
    uint64_t start_lba = start_zone_id << 19;
    for (int i = 0; i < number_of_zones; i++) {
        if (zac_kin->CloseZone(start_lba) == 0) {
            success_count++;
        } else {
            result = false;
            break;
        }
        start_lba += 524288;
    }
    *success_ctr = success_count;
    return result;
}

///////////////////////////////////////////////////////////////////////////////////
/// CloseAllZones()
/// -------------------------------------------------------------------------------
/// Close all WP zones on device
bool CloseAllZones(ZacMediator* zac_kin) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::CloseAllZones()" << std::endl;
    if (zac_kin->CloseAllZones() == 0) {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////
/// TestGetZoneDescriptors()
/// -------------------------------------------------------------------------------
/// Test ZacMediators @GetZoneDescriptorList() Function with valid & invalid parameter combinations
void TestGetZoneDescriptors(ZacMediator* zac_kin) {
    std::cout << "\n--------------------------------------------\n"
              << "Zac_Local Test::TestGetZoneDescriptors()" << std::endl;
    uint64_t gt_max_zoneid = 29809;
    uint64_t zone_id = 80;
    unsigned int num_requested_desc = 20;
    unsigned int reporting_option = kZAC_RO_EXP_OPEN;
    unsigned int invalid_report_option = kZAC_RO_OFFLINE;
    unsigned int first_wp_zoneid = 64;

    /// Request Report starting from Invalid Zone ID (ID is greater than number of zones available)
    /// Expected Result:Should Fail (zoneid too large)
    GetZoneDescList(zac_kin, gt_max_zoneid, reporting_option, num_requested_desc);

    /// Request all Explicitly-Open Zones from zoneid 80 onwards. Populate at most 1 Descriptor
    /// Expected Result: Should Succeed (if there are enough open zones)
    num_requested_desc = 1;
    GetZoneDescList(zac_kin, zone_id, reporting_option, num_requested_desc);

    /// Request all Explicitly-Open Zones from zoneid 80 onwards. Populate at most 0 Descriptors
    /// Expected Result: Should Fail(0 zones requested)
    num_requested_desc = 0;
    GetZoneDescList(zac_kin, zone_id, reporting_option, num_requested_desc);

    /// Request Invalid Zone Report Option
    /// Expected Result: Should Fail (Lamarr does not support offline condition)
    num_requested_desc = 1;
    GetZoneDescList(zac_kin, zone_id, invalid_report_option, num_requested_desc);

    /// Request Undefined(invalid) Report Option
    /// Expected Result: Should Fail
    invalid_report_option = 0x08; // assign undefined reporting option (random value)
    zone_id = 0;
    GetZoneDescList(zac_kin, zone_id, invalid_report_option, num_requested_desc);

    /// Request for all Explicitly-Open zones from zoneid 64 onwards. Populate at most 7000 zones
    /// Expected Result: Should Succeed (if there are enough open zones)
    num_requested_desc = 7000;
    GetZoneDescList(zac_kin, first_wp_zoneid, reporting_option, num_requested_desc);

    /// Request to Populate More Descriptors than available (Only 29808 Zones, requesting 50 Descriptors from 29800)
    /// Expected Result: Should Succeed with Extra Zone Descriptors containing dummy data
    zone_id = 29800;
    num_requested_desc = 50;
    GetZoneDescList(zac_kin, zone_id, reporting_option, num_requested_desc);

    /// Standard Request for 100 Explicitly Open Zone Descriptors starting from Zone 80
    /// Expected Result: Should Succeed (if there are enough open zones)
    zone_id = 80;
    num_requested_desc = 100;
    GetZoneDescList(zac_kin, zone_id, reporting_option, num_requested_desc);
}

///////////////////////////////////////////////////////////////////////////////////
/// GetDeviceHandles()
/// -------------------------------------------------------------------------------
/// Retreive and return names of system scsi devices
/// Any names matching @device_blacklist elements will not be returned
bool GetDeviceHandles(std::vector<std::string> *device_list, const std::set<std::string> &device_blacklist = std::set<std::string>()) {
    FILE *res = popen(kSCSI_DEV_QUERY.c_str(), "r");
    char buff[512];

    if (!res) {
        std::cout << "    ---->> Zac_Local Test::GetDeviceHandles() [FAILURE]:"
                  << " System Command: \"" << kSCSI_DEV_QUERY << "\" returned error status" << std::endl;
        return false;
    }

    std::stringstream ss;
    while (fgets(buff, sizeof(buff), res) != NULL) {
        ss << buff;
    }
    pclose(res);

    std::string device_result = "";
    while (ss >> device_result) {
        if (!device_blacklist.empty()) {
            auto candidate_device = device_blacklist.find(device_result);
            if (candidate_device == device_blacklist.end()) {
                device_list->emplace_back(device_result);
            } else {
                std::cout << "Zac_Local Test::GetDeviceHandles(): Device \""
                          << device_result << "\" has been blacklisted. (skipping)" << std::endl;
            }
        } else {
        device_list->emplace_back(device_result);
    }
    }
    return (!device_list->empty());
}

///////////////////////////////////////////////////////////////////////////////////
/// TestDevice()
/// -------------------------------------------------------------------------------
/// Default Tests to run against @device_handle
/// Exercises basic Host Aware functionality
bool TestDevice(std::unique_ptr<ZacMediator> &zac_med, const std::string &device_handle) {
    if (zac_med->OpenDevice(device_handle) < 0) {
        std::cout << "Zac_Local Test::TestDevice [FAILURE]: Failed to open device: \""
                  << device_handle << "\"" << std::endl;
        return false;
    }

    bool all_tests_passing = true;
    unsigned int start_zone_id = 0;
    unsigned int zone_report_count = 0;
    const unsigned int num_req_zones = 0;
    const unsigned int allocate_zone_id = 300;
    const unsigned int finish_zone_id = 400;
    uint64_t start_lba = start_zone_id << 19;
    const unsigned int open_zone_id = 200;
    const unsigned int open_from_zoneid = 65;
    const unsigned int zones_to_open = 10;
    const unsigned int close_zone_id = 150;
    const unsigned int close_from_zoneid = 75;
    const unsigned int zones_to_close = 5;
    unsigned int open_close_count = 0;

    std::set<unsigned int> snapshot_conditions {kZAC_RO_EMPTY, kZAC_RO_IMP_OPEN,
                                                kZAC_RO_EXP_OPEN, kZAC_RO_CLOSED, kZAC_RO_RESET};

    if (!ResetAllZones(zac_med.get())) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: ResetAllZones() Test" << std::endl;
    }

    if (!ReportAllZones(zac_med.get(), start_zone_id, num_req_zones, &zone_report_count)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: ReportAllZones() Test" << std::endl;
    } else {
        std::cout << "    ---->> Zac_Local Test::TestDevice() ReportAllZones Result: "
                  << std::dec << "\"" << zone_report_count << "\"" << std::endl;
    }

    if (!AllocateZone(zac_med.get(), allocate_zone_id)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: AllocateZone() Test" << std::endl;
    }

    if (!OpenZone(zac_med.get(), open_zone_id)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: OpenZone() Test" << std::endl;
    }

    if (!CloseZone(zac_med.get(), close_zone_id)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: CloseZone() Test" << std::endl;
    }

    if (!OpenNumZones(zac_med.get(), zones_to_open, open_from_zoneid, &open_close_count)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: OpenNumZones() Test" << std::endl;
    }

    if (!CloseNumZones(zac_med.get(), zones_to_close, close_from_zoneid, &open_close_count)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: CloseNumZones() Test" << std::endl;
    }

    if (!FinishZone(zac_med.get(), finish_zone_id)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: FinishZone() Test" << std::endl;
    }

    if (!ReportOpenZones(zac_med.get(), start_lba, num_req_zones, &open_close_count)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: ReportOpenZones() Test" << std::endl;
    } else {
        std::cout << "    ---->> Zac_Local Test::TestDevice() ReportOpenZones Result: "
                  << std::dec << "\"" << open_close_count << "\"" << std::endl;
    }

    if (!WriteAndRetreiveZone(zac_med.get(), allocate_zone_id)) {
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: WriteAndRetreiveZone() Test" << std::endl;
    }
    uint64_t retreive_pair_start_id = 29800;
    if (!RetreiveZonePair(zac_med.get(), retreive_pair_start_id, (retreive_pair_start_id + 1))) {
        std::cout << "    ---->> Zac_Local Test::TestDevice() \""
                  << device_handle << "\" [FAILED]: RetreiveZonePair() Test" << std::endl;
    }

    std::string report = zac_med->GetZoneConditionSnapShot(snapshot_conditions);
    std::cout << "    ---->> Zac_Local Test::TestDevice() ReportZones Result: \n" << report;

    if (!(zac_med->CloseDevice() >= 0)) {
        all_tests_passing = false;
        std::cout << "    ---->> Zac_Local Test::TestDevice [FAILURE]: Failed to Close Device: \""
                  << device_handle << "\"" << std::endl;
    }
    return all_tests_passing;
}

///////////////////////////////////////////////////////////////////////////////////
/// MultipleDeviceTest()
/// -------------------------------------------------------------------------------
/// Run functionality test @TestDevice() for each available scsi device
/// Devices are discovered via @GetDeviceHandles() & @kSCSI_DEV_QUERY
bool MultipleDeviceTest(std::unique_ptr<ZacMediator> &zac_med,
                        const std::set<std::string> &device_blacklist = std::set<std::string>()) {
    std::cout << "Zac_Local Test::MultipleDeviceTest" << std::endl;
    std::vector<std::string> device_list;
    std::string test_result_detail = "";
    if (!GetDeviceHandles(&device_list, device_blacklist)) {
        std::cout << "Zac_Local Test::MultipleDeviceTest(): - [FAILURE]:"
                  << "Cannot locate device handles" << std::endl;
        return false;
    }

    std::cout << "\nTHE FOLLOWING DEVICES WILL BE EVALUATED:\n";
    for (auto &device_name : device_list) {
        std::cout << "  ->" << device_name << "\n";
    } std::cout << std::endl;

    for (auto &device : device_list) {
        std::cout << "\n============================================"
                  << "\n------> TESTING DEVICE: \"" << device << "\" <--------\n" << std::endl;
        test_result_detail = (TestDevice(zac_med, device)) ? "All Tests Passed?: TRUE" : "All Tests Passed?: FALSE";
        std::cout << "\n-----------------------------------------------------"
                  << "\n---> FINISHED TESTING DEVICE: \"" << device << "\""
                  << "\n---> " << test_result_detail
                  << "\n===================================================" << std::endl;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////
/// CustomTesting() - Write Your Own Tests Here
/// -------------------------------------------------------------------------------
/// Function for Developers to write custom workloads
void CustomTesting(std::unique_ptr<ZacMediator> &zac_med, const std::string &device_handle) {
    std::cout << "\n============================================"
              << "\n------> Custom Testing: \"" << device_handle << "\" <--------\n" << std::endl;
    /// Open Device Handle / FD specified by @device_path (e.g. open /dev/sdb)
    if (zac_med->OpenDevice(device_handle) < 0) {
        std::cout << "Zac_Local Test::CustomTesting [FAILURE]: Failed to open device: \""
                  << device_handle << "\"" << std::endl;
        return;
    }
    /// Write Your Own Tests Here ////
    const unsigned int close_zone_id = 150;
    if (!CloseZone(zac_med.get(), close_zone_id)) {
        std::cout << "    ---->> Zac_Local Test::CustomTesting() \""
                  << device_handle << "\" [FAILED]: CloseZone() Test" << std::endl;
    }

    if (!OpenZone(zac_med.get(), close_zone_id)) {
        std::cout << "    ---->> Zac_Local Test::CustomTesting() \""
                  << device_handle << "\" [FAILED]: CustomTesting() Test" << std::endl;
    }
    if (!(zac_med->CloseDevice() >= 0)) {
        std::cout << "    ---->> Zac_Local Test::CustomTesting() [FAILURE]: Failed to Close Device: \""
                  << device_handle << "\"" << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
/// All following method calls are meant to provide usage examples for the HA Command Set
/// Make Changes to this file any way you like WITHIN the FUNCTION: @CustomTesting()
/// (DON'T COMMIT CHANGES unless associated with APPROVED Task Work)
/// -------------------------------------------------------------------------------
/// Mem Leaks Checks(for X86 only)
///   sudo valgrind --leak-check=full --show-leak-kinds=all ./zac_local <device-handle>
/// -------------------------------------------------------------------------------
/// NOTE: <device-handle> should be replaced with target legacy SMR device handle. e.g) /dev/sdb
/// TODO(jdevore): add arg parsing for multiple drives & blacklist specification
int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cerr << "Must Provide a Device Handle!\n" << std::endl;
        printf("USAGE: %s <device> <optional-test-name>\n"
               "Options:\n"
               "     <test-name>: Specify which test function is called\n"
               "                 -> \"custom\"   -  CustomTesting() function is used\n"
               "                 -> \"standard\" -  TestDevice() function is used\n"
               "                 Defaults to \"standard\"\n"
               "EXAMPLES:\n"
               "    -Standard Test w/ sdb:     \"sudo ./zac_local /dev/sdb standard\"\n"
               "    -CustomTesting() w/ sdb:   \"sudo ./zac_local /dev/sdb custom\"\n"
               "    -Valgrind:                 \"sudo valgrind --leak-check=full --show-leak-kinds=all ./zac_local <device-handle>\"\n",
               argv[0]);

        exit(EXIT_FAILURE);
    }

    std::cout << "======= Zac_Local::Non-KV Test HA Command Set =======" << std::endl;
    std::string device_path = argv[1];
    std::string test_target = argc == 3 ? argv[2] : "";  // "custom" -> @CustomTesting()
    std::shared_ptr<AtaCmdHandler> zac_ata(new AtaCmdHandler());
    std::unique_ptr<ZacMediator> zac_kin(new ZacMediator(zac_ata.get()));
    if (!test_target.empty()) {
        std::transform(test_target.begin(), test_target.end(), test_target.begin(), ::tolower);
    }

    if (test_target.compare("custom") == 0) {
        CustomTesting(zac_kin, device_path);
    } else {
        std::cout << "\n============================================"
                  << "\n------> TESTING DEVICE: \"" << device_path << "\" <--------\n" << std::endl;
        std::string result_info = (TestDevice(zac_kin, device_path)) ? "All Tests Passed?: TRUE" : "All Tests Passed?: FALSE";
        std::cout << "\n-----------------------------------------------------"
                  << "\n---> FINISHED TESTING DEVICE: \"" << device_path << "\""
                  << "\n---> " << result_info
                  << "\n===================================================" << std::endl;
    }
    return 0;
}
