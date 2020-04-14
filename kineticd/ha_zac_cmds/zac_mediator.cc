#include "includes/zac_mediator.h"
#include <algorithm>
#include <array>
#include <syslog.h>
#include <unistd.h>
#include <iomanip>
#include <pthread.h>
#include <ctime>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace zac_ha_cmd {
pthread_mutex_t ZacMediator::allocate_zone_mutex_ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ZacMediator::error_log_mutex_ = PTHREAD_MUTEX_INITIALIZER;

ZacMediator::ZacMediator(AtaCmdHandler* sg_ptr)
                                        :log_open_(false),
                                        device_fd_(0),
                                        partial_bit_(0),
                                        sg_cmd_(sg_ptr) {}

ZacMediator::~ZacMediator() {
    sg_cmd_ = nullptr;
    CloseLog();
}

int ZacMediator::OpenDevice(std::string device_path) {
    int ret;
    struct stat st;

    device_fd_ = open(device_path.c_str(), O_RDONLY);
    if (device_fd_ < 0) {
        printf("Open device file %s failed %d (%s)\n", device_path.c_str(), errno, strerror(errno));
        ret = -errno;
        close(device_fd_);
        return ret;
    }

    if (fstat(device_fd_, &st) != 0) {
        printf("Stat device %s failed %d (%s)\n",
        device_path.c_str(),
        errno,
        strerror(errno));

        ret = -errno;
        close(device_fd_);
        return ret;
    }

    if ((!S_ISCHR(st.st_mode)) // is char device
        && (!S_ISBLK(st.st_mode))) { // is char block
        ret = -ENXIO;
        close(device_fd_);
        return ret;
    }
    return device_fd_;
}

///should be done on destruction
int ZacMediator::CloseDevice() {
    if (close(device_fd_)) {
        return( -errno );
    }
    return 0;
}

int ZacMediator::ResetAllZones() {
#ifdef KDEBUG
    std::cout << "======= RESET_ALL_ZONES =======" << std::endl;
#endif
    int status = 0;
    unsigned int start_lba = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (0x01 << kBYTE_THREE_OFFSET); //reset all
    SetPartialOff();
    status = ZacResetWpExt(start_lba, cmd_field_opts);
    cmd_field_opts = 0;
    return status;
}

//see header file for behavior overview
int ZacMediator::AllocateZone(unsigned int zone_id, uint64_t *lba) {
    pthread_mutex_lock(&allocate_zone_mutex_);
#ifdef KDEBUG
    std::cout << "======= ALLOCATE_ZONE(zoneid: "
              << std::dec << zone_id << ", *lba) =======" << std::endl;
#endif
    int attempt_count = 0;
    int fail_count = 0;
    int status = 1; //1 in-case invalid zoneid
    uint32_t cmd_field_opts = 0;
    *lba = 0;

    if (IsValidZoneId(zone_id)) {
        status = 0;
        uint64_t lowLba = CalcLowestLba(zone_id);
        if (zone_id > kLAST_CONVENTIONAL_ZONE_ID) {
            cmd_field_opts |= (0x00 << kBYTE_THREE_OFFSET);
            SetPartialOff();
            while (attempt_count <= 1) {
                attempt_count++;
                status = ZacResetWpExt(lowLba, cmd_field_opts);
                if (status == 0 || fail_count > 1) { break; }
                fail_count++;
                struct timespec sleep_time = { 0 , 0 };
                sleep_time.tv_nsec = 2 * 1000000L;
                nanosleep(&sleep_time, NULL);
            }

            if (fail_count > 0) {
                log_stream_ << "-ResetWP(Z:"
                            << zone_id
                            << ")-Attempts:"
                            << attempt_count
                            << "-Fails:"
                            << fail_count;
                ZacLogError();
            }
        }
        *lba = lowLba;
    }
    pthread_mutex_unlock(&allocate_zone_mutex_);
    return status;
}

// This Version of AllocateZone is not utilized
int ZacMediator::AllocateZone(unsigned int zone_id) {
    pthread_mutex_lock(&allocate_zone_mutex_);
#ifdef KDEBUG
    std::cout << "======= ALLOCATE_ZONE =======" << std::endl;
#endif
    int status = 1;
    uint32_t cmd_field_opts = 0;
    if (IsValidZoneId(zone_id)) {
        uint64_t lowLba = CalcLowestLba(zone_id);
        if (zone_id > kLAST_CONVENTIONAL_ZONE_ID) {
            cmd_field_opts |= (0x00 << kBYTE_THREE_OFFSET); // only reset lba
            SetPartialOff();
            status = ZacResetWpExt(lowLba, cmd_field_opts);
        } else {
            status = 0;
        }
    }
    //unlock here
    pthread_mutex_unlock(&allocate_zone_mutex_);
    return status;
}

int ZacMediator::OpenAllZones() {
#ifdef KDEBUG
    std::cout << "======= OPEN_ALL_ZONES =======" << std::endl;
#endif
    int status = 0;
    unsigned int start_lba = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (0x01 << kBYTE_THREE_OFFSET); // open all
    SetPartialOff();
    status = ZacOpenZoneExt(start_lba, cmd_field_opts);
    return status;
}

int ZacMediator::OpenZone(uint64_t start_lba) {
#ifdef KDEBUG
    std::cout << "======= OPEN_ZONE =======" << std::endl;
#endif
    int status = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (0x00 << kBYTE_THREE_OFFSET); // only open lba
    SetPartialOff();
    status = ZacOpenZoneExt(start_lba, cmd_field_opts);
    return status;
}

int ZacMediator::CloseZone(uint64_t start_lba) {
#ifdef KDEBUG
    std::cout << "======= CLOSE_ZONE =======" << std::endl;
#endif
    int status = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (0x00 << kBYTE_THREE_OFFSET); // only close lba
    SetPartialOff();
    status = ZacCloseZoneExt(start_lba, cmd_field_opts);
    return status;
}

int ZacMediator::CloseAllZones() {
#ifdef KDEBUG
    std::cout << "\n======= CLOSE_ALL_ZONES =======" << std::endl;
#endif
    int status = 0;
    unsigned int start_lba = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (0x01 << kBYTE_THREE_OFFSET); // close all
    SetPartialOff();
    status = ZacCloseZoneExt(start_lba, cmd_field_opts);
    return status;
}

int ZacMediator::FinishZone(uint64_t start_lba) {
#ifdef KDEBUG
    std::cout << "\n======= FINISH_ZONE =======" << std::endl;
#endif
    int status = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts  |= (0x00 << kBYTE_THREE_OFFSET); // only finish lba
    SetPartialOff();
    status = ZacFinishZoneExt(start_lba, cmd_field_opts);
    return status;
}

int ZacMediator::FinishAllZones() {
#ifdef KDEBUG
    std::cout << "======= FINISH_ALL_ZONES =======" << std::endl;
#endif
    int status = 0;
    unsigned int start_lba = 0;
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (0x01 << kBYTE_THREE_OFFSET); // finish all
    SetPartialOff();
    status = ZacFinishZoneExt(start_lba, cmd_field_opts);
    return status;
}

int ZacMediator::ReportOpenZones(uint64_t start_lba, unsigned int num_zone_desc) {
#ifdef KDEBUG
    std::cout << "======= REPORT_OPEN_ZONES =======" << std::endl;
#endif
    int zones = 0;
    unsigned int rp_buff_size = CalcRetPageBuff(num_zone_desc);
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (kZAC_RO_EXP_OPEN << kBYTE_THREE_OFFSET); // rep option explicit open
    SetPartialOff();
    zones = ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
    return zones;
}

int ZacMediator::ReportClosedZones(uint64_t start_lba, unsigned int num_zone_desc) {
#ifdef KDEBUG
    std::cout << "======= REPORT_CLOSE_ZONES =======" << std::endl;
#endif
    int zones = 0;
    unsigned int rp_buff_size = CalcRetPageBuff(num_zone_desc);
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (kZAC_RO_CLOSED << kBYTE_THREE_OFFSET);
    SetPartialOff();
    zones = ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
    return zones;
}

int ZacMediator::ReportEmptyZones(uint64_t start_lba, unsigned int num_zone_desc) {
#ifdef KDEBUG
    std::cout << "======= REPORT_EMPTY_ZONES =======" << std::endl;
#endif
    int zones = 0;
    unsigned int rp_buff_size = CalcRetPageBuff(num_zone_desc);
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (kZAC_RO_EMPTY << kBYTE_THREE_OFFSET);
    SetPartialOff();
    zones = ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
    return zones;
}

int ZacMediator::ReportFullZones(uint64_t start_lba, unsigned int num_zone_desc) {
#ifdef KDEBUG
    std::cout << "======= REPORT_FULL_ZONES =======" << std::endl;
#endif
    int zones = 0;
    unsigned int rp_buff_size = CalcRetPageBuff(num_zone_desc);
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (kZAC_RO_FULL << kBYTE_THREE_OFFSET);
    SetPartialOff();
    zones = ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
    return zones;
}

int ZacMediator::ReportAllZones(uint64_t start_lba, unsigned int num_zone_desc) {
#ifdef KDEBUG
    std::cout << "======= REPORT_ALL_ZONES (start_lba: " << std::dec << start_lba << ") =======" << std::endl;
#endif
    int zones = 0;
    unsigned int rp_buff_size = CalcRetPageBuff(num_zone_desc);
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (kZAC_RO_ALL << kBYTE_THREE_OFFSET);
    SetPartialOff();
    zones = ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
    return zones;
}

int ZacMediator::ReportZonesCustom(uint64_t start_lba,
                                   unsigned int num_zone_desc,
                                   unsigned int report_option) {
#ifdef KDEBUG
    std::cout << "======= REPORTZONES("
              << ReportingOptionString(report_option)
              << ") =======" << std::endl;
#endif
    int zones_result = -1;
    if (IsValidRO(report_option)) {
        unsigned int rp_buff_size = CalcRetPageBuff(num_zone_desc);
        uint32_t cmd_field_opts = 0;
        cmd_field_opts |= (report_option << kBYTE_THREE_OFFSET);
        SetPartialOff();
        zones_result = ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
        return zones_result;
    }
    std::cerr << "======= zac_mediator::ReportZonesCustom "
              << "Invalid Reporting Option Provided" << std::endl;
    return zones_result;
}

int ZacMediator::WriteZone(uint64_t wp_lba, void *data, size_t size) {
    int total_bytes_written = 0;
    int status = 0;
    uint32_t cmd_field_opts = 0;
    SetPartialOff();

    if (size > kSIZE_LIMIT) {
        int half_mib_chunks = size / kSIZE_LIMIT;

        for (int i = 0; i < half_mib_chunks; i++) {
            status = ZacWriteExt(wp_lba, cmd_field_opts, data, kSIZE_LIMIT);

            if (status < 0) {
                printf("Write failed\n");
                return status;
            }

            total_bytes_written += status;
            data = ((unsigned char*)data) + status;
            wp_lba = wp_lba + ((uint64_t)(status/kLAMARR_LBA_SIZE));
            cmd_field_opts = 0;
        }
    } else {
        status = ZacWriteExt(wp_lba, cmd_field_opts, data, size);

        if (status < 0) {
            printf("Write Failed\n");
            return status;
        }
        total_bytes_written = status;
    }

    return total_bytes_written;
}

// NOTE: this command is incomplete
int ZacMediator::ReadZone(unsigned int zone_id) {
    if (!IsValidZoneId(zone_id)) {
        std::cout << "ZacMediator::ReadZone(" << zone_id <<") invalid zone id provided";
        return 1;
    }
    int status = 0;
    uint32_t cmd_field_opts = 0;
    uint64_t lowLba = CalcLowestLba(zone_id);
    SetPartialOff();
    status = ZacReadZoneExt(lowLba, 4096, cmd_field_opts);
    return status;
}


std::string ZacMediator::GetZoneConditionSnapShot() {
    std::stringstream ss(std::ios_base::out | std::ios_base::ate);
    std::vector<unsigned int> rep_options = {kZAC_RO_ALL, kZAC_RO_FULL, kZAC_RO_EMPTY,
                                             kZAC_RO_IMP_OPEN, kZAC_RO_EXP_OPEN, kZAC_RO_CLOSED,
                                             kZAC_RO_RESET, kZAC_RO_NON_SEQ, kZAC_RO_NOT_WP};
    int res = 0;
    for (const unsigned int &ro : rep_options) {
        res = ReportZonesCustom(0, 0, ro);
        if (res >= 0) {
            ss << " -" << ReportingOptionString(ro) << " : " << std::dec << res << "\n";
        } else {
            ss << " -" << ReportingOptionString(ro) << " : -1\n";
        }
        res = 0;
    }
    return ss.str();
}

std::string ZacMediator::GetZoneConditionSnapShot(std::set<unsigned int> &target_report_options) {
    if (target_report_options.empty()) {
        std::cout << "ZacMediator::GetZoneConditionSnapShot() ERROR"
                  << " - Must Provide Target Reporting Options" << std::endl;
        return "";
    }
    std::stringstream ss(std::ios_base::out | std::ios_base::ate);
    std::vector<unsigned int> rep_options = {kZAC_RO_ALL, kZAC_RO_FULL, kZAC_RO_EMPTY,
                                             kZAC_RO_IMP_OPEN, kZAC_RO_EXP_OPEN, kZAC_RO_CLOSED,
                                             kZAC_RO_RESET, kZAC_RO_NON_SEQ, kZAC_RO_NOT_WP};
    int res = 0;
    for (const unsigned int &ro : rep_options) {
        auto request_rep_option = target_report_options.find(ro);
        if (request_rep_option != target_report_options.end()) {
            res = ReportZonesCustom(0, 0, ro);
            if (res >= 0) {
                ss << " -" << ReportingOptionString(ro) << " : " << std::dec << res << "\n";
            } else {
                ss << " -" << ReportingOptionString(ro) << " : -1\n";
            }
            res = 0;
        }
    }
    return ss.str();
}

bool ZacMediator::GetZoneDescriptorList(ZacZone* zone_list_ptr,
                                        uint64_t start_zone_id,
                                        unsigned int report_option,
                                        unsigned int num_zones_requested) {
#ifdef KDEBUG
    std::cout << "======= ZacMediator::GetZoneDescriptorList -> "
              << "Condition:\""
              << ReportingOptionString(report_option) << "\""
              << std::dec << " -Requested# of Zones to Print:"
              << num_zones_requested << " -StartingZone:" << start_zone_id << std::endl;
#endif

    if ((!IsValidZoneId(start_zone_id)) || (!IsValidRO(report_option)) || (zone_list_ptr == nullptr)) { //NOLINT
        std::cerr << "--->> zac_mediator::GetZoneDescriptorList() "
                  << "invalid argument provided. Returning" << std::endl;
        return false;
    }

    if (num_zones_requested <= 0) {
        std::cerr << "--->> zac_mediator::GetZoneDescriptorList(): "
                  << " ZERO zones Requested. No work to be done.. exiting" << std::endl;
        return false;
    }
    num_zones_requested = ((start_zone_id + num_zones_requested) > kLAMARR_HIGHESTWP_ZONE) ?
                           (kLAMARR_HIGHESTWP_ZONE - start_zone_id) : (num_zones_requested);

    unsigned int num_populated_zones = 0; // number of structs populated
    uint64_t rp_buff_size = 0; // size of buffer to populate
    unsigned int num_zones_reported = 0; // how many zones returned by report zones ata cmd
    uint32_t cmd_field_opts = 0; // byte field used to populate command
    unsigned int remaining_quota = 0; // # zones not yet populated (requested - num populated)
    unsigned int num_zones_inbuff = 0;
    std::vector<uint8_t> *buff_ptr;
    uint64_t zone_descriptor_start_position = kZAC_ZONE_DESCRIPTOR_OFFSET;
    uint64_t curr_start_lba = CalcLowestLba(start_zone_id); //Report Zones starting lba

    while (num_populated_zones < num_zones_requested) {
        remaining_quota = num_zones_requested - num_populated_zones;
        rp_buff_size = (remaining_quota <= kMAX_ZONE_DESC_RET) ? CalcRetPageBuff(remaining_quota) :
                                                                CalcRetPageBuff(kMAX_ZONE_DESC_RET);
        SetPartialOff();
        cmd_field_opts |= (report_option << kBYTE_THREE_OFFSET); // reporting option (e.g. open)
        num_zones_reported = ZacReportZonesExt(curr_start_lba, rp_buff_size, cmd_field_opts);
        if (num_zones_reported <= 0) {
            std::cerr << "--->> ZacMediator::GetZoneDescriptorList 0 zones reported" << std::endl;
            return false;
        }

        buff_ptr = sg_cmd_->GetDataBufp();
        num_zones_inbuff = (buff_ptr->size() - kZD_PAGE_HDR_SIZE)  / kZAC_ZONE_DESC_LENGTH;
        if (num_zones_inbuff > remaining_quota) {
            num_zones_inbuff = remaining_quota;
        }

        for (int i = 0; i < (int)num_zones_inbuff; i++) {
            zone_list_ptr[num_populated_zones].zone_type = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position]) & 0x0f;//NOLINT
            zone_list_ptr[num_populated_zones].zone_condition = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position + 1] >> 4) & 0x0f; //NOLINT
            zone_list_ptr[num_populated_zones].zone_length = ParseQword(zone_descriptor_start_position + 8);//NOLINT
            zone_list_ptr[num_populated_zones].start_lba = ParseQword(zone_descriptor_start_position + 16);//NOLINT
            zone_list_ptr[num_populated_zones].write_pointer = ParseQword(zone_descriptor_start_position + 24);//NOLINT
            zone_list_ptr[num_populated_zones].non_seq = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position + 1] >> 1) & 1;//NOLINT
            zone_list_ptr[num_populated_zones].zone_id = CalcZoneId(zone_list_ptr[num_populated_zones].start_lba);//NOLINT
            zone_descriptor_start_position += kZAC_ZONE_DESC_LENGTH;
            num_populated_zones++;
        }

        sg_cmd_->ClearDataBuf();
        zone_descriptor_start_position = kZAC_ZONE_DESCRIPTOR_OFFSET;
        cmd_field_opts = 0;
        rp_buff_size = 0;
        buff_ptr = 0;
        curr_start_lba = zone_list_ptr[num_populated_zones-1].start_lba + kLAMARR_ZONE_LBA_COUNT;
    }
#ifdef KDEBUG
    PrintZoneDescriptors(zone_list_ptr, num_zones_requested);
#endif
    return true;
}

bool ZacMediator::GetPairZoneDescriptors(ZacZone* zone_list_ptr,
                                  uint64_t start_zone_id,
                                  uint64_t second_zone_id) {
    if ((!IsWpZoneId(start_zone_id)) || (!IsWpZoneId(second_zone_id)) || (start_zone_id == second_zone_id)) { //NOLINT
        std::cout << "--->>--->> zac_mediator::GetPairZoneDescriptors() invalid zone ID's ("
                  << start_zone_id << "," << second_zone_id << ")."
                  << "(ID Rules: valid WP zones, different, and <= max zone) " << std::endl;
        return false;
    }

    if (zone_list_ptr == nullptr) {
        std::cerr << "--->> zac_mediator::GetPairZoneDescriptors() "
                  << "Error: zone_list_ptr = null" << std::endl;
        return false;
    }
#ifdef KDEBUG
    std::cout << "======= ZacMediator::GetPairZoneDescriptors(Report-Opt: "
              << std::dec << " [start_zone_id: " << start_zone_id
              << " ,second_zone_id: " << second_zone_id << "]) =======" << std::endl;
#endif

    uint64_t rp_buff_size = 0; // size of buffer to populate
    int num_calls_required = 1; // # of report zones calls required
    unsigned int num_zones_reported = 0; // how many zones returned by report zones ata cmd
    uint32_t cmd_field_opts = 0; // byte field to populate command
    uint64_t zone_descriptor_start_position = kZAC_ZONE_DESCRIPTOR_OFFSET;
    std::vector<uint8_t> *buff_ptr; // pointer to return buffer
    // std::array<uint64_t, 2> lba_list = {0, 0}; // both zone's starting lba's
    std::vector<uint64_t> lba_list(2, 0);
    lba_list.at(0) = CalcLowestLba(start_zone_id);
    lba_list.at(1) = CalcLowestLba(second_zone_id);
    std::sort(lba_list.begin(), lba_list.end()); // sort small -> largest lba

    // Distance between zone id's.If distance > @kMAX_ZONE_DESC_RET, multiple calls to report zones
    uint64_t abs_zid_dist = (start_zone_id > second_zone_id) ? (start_zone_id - second_zone_id) :
                                                               (second_zone_id - start_zone_id);
    if (abs_zid_dist > kMAX_ZONE_DESC_RET) {
        num_calls_required = 2;
    }

    int call_count = 0; // # of Report Zone Calls made
    uint64_t target_lba = 0;
    int zones_populated = 0;
    while (call_count < num_calls_required) {
        SetPartialOff();
        cmd_field_opts |= (kZAC_RO_ALL << kBYTE_THREE_OFFSET); // Report All zones
        // BufferSize set to abs_zid_dist if only one call; size of 2 descriptros if 2 calls
        rp_buff_size = (num_calls_required > 1) ? (CalcRetPageBuff(2)) :
                                                  (CalcRetPageBuff(abs_zid_dist+1));

        target_lba = lba_list[call_count];
        num_zones_reported = ZacReportZonesExt(target_lba, rp_buff_size, cmd_field_opts);
        if (num_zones_reported <= 0) {
            std::cerr << "-->> ZacMediator::GetPairZoneDescriptors 0 zones reported" << std::endl;
            return false;
        }
        buff_ptr = sg_cmd_->GetDataBufp();

        // Iterate over return pages from Report Zones, populate at most two zones
        // To avoid checking each descriptor,move @zone_descriptor_start_position by
        // @abs_zid_dist * descriptor length each iteration
        uint64_t current_startlba = 0;
        for (int i = 0; (i < 2 && zones_populated < 2); i++) {
            current_startlba = ParseQword(zone_descriptor_start_position + 16);
            if (current_startlba == lba_list[zones_populated]) {
                zone_list_ptr[zones_populated].zone_type = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position]) & 0x0f; //NOLINT
                zone_list_ptr[zones_populated].zone_condition = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position + 1] >> 4) & 0x0f; //NOLINT
                zone_list_ptr[zones_populated].zone_length = ParseQword(zone_descriptor_start_position + 8); //NOLINT
                zone_list_ptr[zones_populated].start_lba = current_startlba;
                zone_list_ptr[zones_populated].write_pointer = ParseQword(zone_descriptor_start_position + 24); //NOLINT
                zone_list_ptr[zones_populated].non_seq = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position + 1] >> 1) & 1; //NOLINT
                zone_list_ptr[zones_populated].zone_id = CalcZoneId(zone_list_ptr[zones_populated].start_lba); //NOLINT
                zone_descriptor_start_position += (kZAC_ZONE_DESC_LENGTH * abs_zid_dist);
                zones_populated++;
            }
            if (num_calls_required > 1) break; // if another call required, avoid parsing qword
        }
        // reset variables for next report zones call
        sg_cmd_->ClearDataBuf();
        zone_descriptor_start_position = kZAC_ZONE_DESCRIPTOR_OFFSET;
        cmd_field_opts = 0;
        rp_buff_size = 0;
        buff_ptr = 0;
        call_count++;
    } // end while()
#ifdef KDEBUG
    PrintZoneDescriptors(zone_list_ptr, 2);
#endif
    return true;
}

void ZacMediator::GetZoneInfo(ZacZone *zone_ptr, unsigned int zone_id) {
    uint64_t start_lba = ((uint64_t)zone_id) * 524288;
    unsigned int rp_buff_size = CalcRetPageBuff(1);
    uint32_t cmd_field_opts = 0;
    cmd_field_opts |= (kZAC_RO_ALL << kBYTE_THREE_OFFSET);
    SetPartialOff();
    ZacReportZonesExt(start_lba, rp_buff_size, cmd_field_opts);
    PopulateZone(zone_ptr, 0);
}

////////////////////////////////////////////////////////////////////////////////////
/// BEGIN ATA Extended Command Section
/// -------------------------------------------------------------------------------
int ZacMediator::ZacReportZonesExt(uint64_t start_lba,
                                   unsigned int buff_size,
                                   uint32_t cmd_field_opts) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->AllocateDataBuf(buff_size);
    unsigned int ret_page_ct = buff_size / kZD_PAGE_SIZE;
    sg_cmd_->ConstructIoHeader(SG_DXFER_FROM_DEV, &io_hdr);
    cmd_field_opts |= (kZAC_ATA_REPORT_ZONES_EXT_AF << kACT_FIELD_OFFSET);
    cmd_field_opts |= (ret_page_ct << kRETPAGE_SECTOR_OFFSET);
    cmd_field_opts |= (kZAC_ATA_REPORT_ZONES_EXT_CMD << kZAC_CMD_TMP_OFFSET);
    cmd_field_opts |= (kATA_PROTO_DMA << kTXFER_PROTO_OFFSET);
    PopulateCdb(start_lba, kCDB_RZACBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(ZacReportZonesExt)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    // Check return status on execution of command
    if (ret != 0) {
        // For now only get sense_key
        log_stream_ << "ZacReportZonesExt lba:"
            << start_lba
            << " Failed with sense_key: "
            << std::hex << std::setfill('0') << std::setw(2)
            << (sg_cmd_->GetSenseBufp()[1] & 0x0F);
        ZacLogError();
        return ret;
    }

    return ParseDword();
}

int ZacMediator::ZacResetWpExt(uint64_t start_lba, uint32_t cmd_field_opts) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->ClearDataBuf();
    sg_cmd_->ConstructIoHeader(SG_DXFER_NONE, &io_hdr);
    cmd_field_opts |= (kATA_PROTO_NON << kTXFER_PROTO_OFFSET);
    cmd_field_opts |= (kZAC_ATA_RESET_WRITE_POINTER_EXT_AF << kACT_FIELD_OFFSET);
    cmd_field_opts |= (kZAC_ATA_RESET_WRITE_POINTER_EXT_CMD << kZAC_CMD_TMP_OFFSET);
    PopulateCdb(start_lba, kCDB_DEFZACBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(ZacResetWpExt)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);
    return ret;
}

int ZacMediator::ZacOpenZoneExt(uint64_t start_lba, uint32_t cmd_field_opts) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->ClearDataBuf();
    sg_cmd_->ConstructIoHeader(SG_DXFER_NONE, &io_hdr);
    cmd_field_opts |= (kZAC_ATA_OPEN_ZONE_EXT_AF << kACT_FIELD_OFFSET);
    cmd_field_opts |= (kZAC_ATA_OPEN_ZONE_EXT_CMD << kZAC_CMD_TMP_OFFSET);
    cmd_field_opts |= (kATA_PROTO_NON << kTXFER_PROTO_OFFSET);
    PopulateCdb(start_lba, kCDB_DEFZACBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(ZacOpenZoneExt)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    // Check return status on execution of command
    if (ret != 0) {
        // For now only get sense_key
        long unsigned int sense_key = (sg_cmd_->GetSenseBufp()[1] & 0x0F); //NOLINT
        long unsigned int additional_sense_key = sg_cmd_->GetSenseBufp()[2]; //NOLINT
        printf("Command Failed with sense_key: 0x%02lx\n", sense_key);
        printf("Addtional sense_key: 0x%02lx\n", additional_sense_key);
    }
    return ret;
}

int ZacMediator::ZacCloseZoneExt(uint64_t start_lba, uint32_t cmd_field_opts) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->ClearDataBuf();
    sg_cmd_->ConstructIoHeader(SG_DXFER_NONE, &io_hdr);
    cmd_field_opts |= (kZAC_ATA_CLOSE_ZONE_EXT_AF << kACT_FIELD_OFFSET);
    cmd_field_opts |= (kZAC_ATA_CLOSE_ZONE_EXT_CMD << kZAC_CMD_TMP_OFFSET);
    cmd_field_opts |= (kATA_PROTO_NON << kTXFER_PROTO_OFFSET);
    PopulateCdb(start_lba, kCDB_DEFZACBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(ZacCloseZoneExt)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    // Check return status on execution of command
    if (ret != 0) {
        // For now only get sense_key
        long unsigned int sense_key = (sg_cmd_->GetSenseBufp()[1] & 0x0F); //NOLINT
        printf("Command Failed with sense_key: 0x%02lx\n\n", sense_key);
    }
    return ret;
}

int ZacMediator::ZacFinishZoneExt(uint64_t start_lba, uint32_t cmd_field_opts) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->ClearDataBuf();
    sg_cmd_->ConstructIoHeader(SG_DXFER_NONE, &io_hdr);
    cmd_field_opts |= (kZAC_ATA_FINISH_ZONE_EXT_AF << kACT_FIELD_OFFSET);
    cmd_field_opts |= (kZAC_ATA_FINISH_ZONE_EXT_CMD << kZAC_CMD_TMP_OFFSET);
    cmd_field_opts |= (kATA_PROTO_NON << kTXFER_PROTO_OFFSET);
    PopulateCdb(start_lba, kCDB_DEFZACBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(ZacFinishZoneExt)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    // Check return status on execution of command
    if (ret != 0) {
        // For now only get sense_key
        long unsigned int sense_key = (sg_cmd_->GetSenseBufp()[1] & 0x0F); //NOLINT
        printf("Command Failed with sense_key: 0x%02lx\n", sense_key);
    }
    return ret;
}

int ZacMediator::ZacWriteExt(uint64_t start_lba, uint32_t cmd_field_opts, void *data, size_t size) {
    unsigned int ret_page_ct = ((size + 4095) & ~4095) / kLAMARR_LBA_SIZE;

    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->ClearDataBuf();
    sg_cmd_->ConstructIoHeader(SG_DXFER_TO_DEV, &io_hdr, (uint8_t*)data, size);
    cmd_field_opts |= (ret_page_ct << kRETPAGE_SECTOR_OFFSET);
    cmd_field_opts |= (kATA_WRITE_DMA_EXT << kZAC_CMD_TMP_OFFSET);
    cmd_field_opts |= (kATA_PROTO_DMA << kTXFER_PROTO_OFFSET);
    PopulateCdb(start_lba, kCDB_WBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(ZacWriteExt)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    // Check return status on execution of command
    if (ret != 0) {
        // For now only get sense_key
        log_stream_ << "ZacWriteExt start_lba:"
            << start_lba
            << " Command Failed with sense_key: "
            << std::hex << std::setfill('0') << std::setw(2)
            << (sg_cmd_->GetSenseBufp()[1] & 0x0F);
        ZacLogError();
        return ret;
    }
    return (size - io_hdr.resid);
}

unsigned int ZacMediator::CalcRetPageBuff(unsigned int num_req_zones) {
    unsigned int res = (((num_req_zones * kZAC_ZONE_DESC_LENGTH) + kZD_PAGE_HDR_SIZE) + 511) & ~511;
    return res;
}

int ZacMediator::FlushCacheAta() {
    uint32_t cmd_field_opts = 0;
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    SetPartialOff();
    sg_cmd_->ClearDataBuf();
    sg_cmd_->ConstructIoHeader(SG_DXFER_NONE, &io_hdr);
    cmd_field_opts |= (kATA_PROTO_NON << kTXFER_PROTO_OFFSET);
    cmd_field_opts |= (kATA_FLUSH_CACHE_EXT << kZAC_CMD_TMP_OFFSET);
    PopulateCdb(0, kCDB_DEFZACBYTE_2, cmd_field_opts, cdb);
#ifdef KDEBUG
    std::cout << "(FlushCacheAta)" << std::endl;
#endif
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    if (ret != 0) {
        // For now only get sense_key
        long unsigned int sense_key = (sg_cmd_->GetSenseBufp()[1] & 0x0F); //NOLINT
        printf("Command Failed with sense_key: 0x%02lx\n", sense_key);
    }
    return ret;
}

int ZacMediator::ZacReadZoneExt(uint64_t start_lba,
                                unsigned int buff_size,
                                uint32_t cmd_field_opts) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));
    sg_cmd_->AllocateDataBuf(buff_size);
    unsigned int ret_page_ct = buff_size / kZD_PAGE_SIZE;
    sg_cmd_->ConstructIoHeader(SG_DXFER_FROM_DEV, &io_hdr);
    cmd_field_opts |= (ret_page_ct << kRETPAGE_SECTOR_OFFSET);
    cmd_field_opts |= (kATA_READ_DMA_EXT << kZAC_CMD_TMP_OFFSET);
    cmd_field_opts |= (kATA_PROTO_DMA << kTXFER_PROTO_OFFSET);
#ifdef KDEBUG
    std::cout << "(ZacReadZoneExt)" << std::endl;
#endif
    PopulateCdb(start_lba, kCDB_RZACBYTE_2, cmd_field_opts, cdb);
    int ret = sg_cmd_->ExecuteSgCmd(device_fd_, cdb, sizeof(cdb), io_hdr);

    if (ret != 0) {
        // For now only get sense_key
        // Specifically if only a certain amount of bytes is written to
        long unsigned int sense_key = (sg_cmd_->GetSenseBufp()[1] & 0x0F); //NOLINT
        printf("Command Failed with sense_key: 0x%02lx\n", sense_key);
    } else {
        std::vector<uint8_t> *buff_ptr = sg_cmd_->GetDataBufp();
        int fd = fileno(stdout); // write data to stdout, replace with preferred container
        write(fd, buff_ptr->data(), buff_size); // eventually return a data container pointer
        std::cout << std::endl;
    }
    return ret;
}

int ZacMediator::PopulateCdb(uint64_t start_lba, uint8_t byte_two,
                             uint32_t cmd_field_opts, uint8_t *cdb_ptr) {
    cdb_ptr[1] = (((cmd_field_opts & kTXFER_PROTO_MASK) >> kTXFER_PROTO_OFFSET) << 1) | 0x01;
    cdb_ptr[2] = byte_two;
    cdb_ptr[3] = (((cmd_field_opts & kBYTE_THREE_MASK) >> kBYTE_THREE_OFFSET) & 0x3f) | partial_bit_;//NOLINT
    cdb_ptr[4] = ((cmd_field_opts & kACT_FIELD_MASK) >> kACT_FIELD_OFFSET);
    cdb_ptr[5] = (((cmd_field_opts & kRETPAGE_SECTOR_MASK) >> kRETPAGE_SECTOR_OFFSET) >> kBYTE_FIVE_OFFSET) & 0xFF;//NOLINT
    cdb_ptr[6] = ((cmd_field_opts & kRETPAGE_SECTOR_MASK) >> kRETPAGE_SECTOR_OFFSET);
    cdb_ptr[8]  = start_lba & 0xff;
    cdb_ptr[10] = (start_lba >>  8) & 0xff;
    cdb_ptr[12] = (start_lba >> 16) & 0xff;
    cdb_ptr[7]  = (start_lba >> 24) & 0xff;
    cdb_ptr[9]  = (start_lba >> 32) & 0xff;
    cdb_ptr[11] = (start_lba >> 40) & 0xff;
    cdb_ptr[13] = 1 << 6;
    cdb_ptr[14] = ((cmd_field_opts & kZAC_CMD_TMP_MASK) >> kZAC_CMD_TMP_OFFSET);
#ifdef KDEBUG
    std::cout << "----POPULATE_CDB----" << std::endl;
    std::cout << "-------------------\n";
    std::cout << "--> cdb_ptr[1]: " << std::hex << (uint32_t)cdb_ptr[1] << "\n";
    std::cout << "--> cdb_ptr[2]: " << std::hex << (uint32_t)cdb_ptr[2] << "\n";
    std::cout << "--> cdb_ptr[3]: " << std::hex << (uint32_t)cdb_ptr[3] << "\n";
    std::cout << "--> cdb_ptr[4]: " << std::hex << (uint32_t)cdb_ptr[4] << "\n";
    std::cout << "--> cdb_ptr[5]: " << std::hex << (uint32_t)cdb_ptr[5] << "\n";
    std::cout << "--> cdb_ptr[6]: " << std::hex << (uint32_t)cdb_ptr[6] << "\n";
    std::cout << "--> cdb_ptr[7]: " << std::hex << (uint32_t)cdb_ptr[7] << "\n";
    std::cout << "--> cdb_ptr[8]: " << std::hex << (uint32_t)cdb_ptr[8] << "\n";
    std::cout << "--> cdb_ptr[9]: " << std::hex << (uint32_t)cdb_ptr[9] << "\n";
    std::cout << "--> cdb_ptr[10]: " << std::hex << (uint32_t)cdb_ptr[10] << "\n";
    std::cout << "--> cdb_ptr[11]: " << std::hex << (uint32_t)cdb_ptr[11] << "\n";
    std::cout << "--> cdb_ptr[12]: " << std::hex << (uint32_t)cdb_ptr[12] << "\n";
    std::cout << "--> cdb_ptr[13]: " << std::hex << (uint32_t)cdb_ptr[13] << "\n";
    std::cout << "--> cdb_ptr[14]: " << std::hex << (uint32_t)cdb_ptr[14] << "\n";
    std::cout << "--> cdb_ptr[15]: " << std::hex << (uint32_t)cdb_ptr[15] << "\n";
    std::cout << "-------------------" << std::endl;
#endif
    return 0;
}

uint32_t ZacMediator::ParseDword() {
    std::vector<uint8_t> *buff_ptr = sg_cmd_->GetDataBufp();
    uint32_t dword =
     ((uint32_t)(*buff_ptr)[0]
     | ((uint32_t)(*buff_ptr)[1] << 8)
     | ((uint32_t)(*buff_ptr)[2] << 16)
     | ((uint32_t)(*buff_ptr)[3] << 24));
    dword = dword / 64;
    return dword;
}

uint64_t ZacMediator::ParseQword(int position) {
    std::vector<uint8_t> *buff_ptr = sg_cmd_->GetDataBufp();
    uint64_t qword =
        ( (uint64_t)(*buff_ptr)[position]
        | ((uint64_t)(*buff_ptr)[position+1] << 8)
        | ((uint64_t)(*buff_ptr)[position+2] << 16)
        | ((uint64_t)(*buff_ptr)[position+3] << 24)
        | ((uint64_t)(*buff_ptr)[position+4] << 32)
        | ((uint64_t)(*buff_ptr)[position+5] << 40)
        | ((uint64_t)(*buff_ptr)[position+6] << 48)
        | ((uint64_t)(*buff_ptr)[position+7] << 56) );
    return qword;
}

void ZacMediator::PopulateZone(ZacZone* zone, int zone_num) {
    std::vector<uint8_t> *buff_ptr = sg_cmd_->GetDataBufp();
    int zone_descriptor_start_position = (zone_num * kZAC_ZONE_DESC_LENGTH) + kZD_PAGE_HDR_SIZE;
    zone->zone_type = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position]) & 0x0f;
    zone->zone_condition = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position + 1] >> 4) & 0x0f;
    zone->zone_length = ParseQword(zone_descriptor_start_position + 8);
    zone->start_lba = ParseQword(zone_descriptor_start_position + 16);
    zone->write_pointer = ParseQword(zone_descriptor_start_position + 24);
    zone->non_seq = ((uint8_t)(*buff_ptr)[zone_descriptor_start_position + 1] >> 1) & 1;
}

void ZacMediator::OpenLog() {
    if (!log_open_) {
        setlogmask(LOG_UPTO(LOG_INFO));
        openlog("ZAC_HA_ERR", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        log_open_ = true;
    }
}

void ZacMediator::CloseLog() {
    pthread_mutex_lock(&error_log_mutex_);
#ifdef KDEBUG
    std::cout << "--->>--->> ZacMediator:: CloseLog " << std::endl;
#endif
    if (log_open_) {
        closelog();
        log_open_ = false;
    }
    pthread_mutex_unlock(&error_log_mutex_);
}

void ZacMediator::ZacLogError() {
    pthread_mutex_lock(&error_log_mutex_);
#ifdef KDEBUG
    std::cout << "--->>--->> ZacMediator:: ZacLogError" << std::endl;
#endif
    OpenLog();
    syslog(LOG_INFO, "%s", log_stream_.str().c_str());
    log_stream_.str(std::string());
    log_stream_.clear();
    pthread_mutex_unlock(&error_log_mutex_);
}

void ZacMediator::SetPartialOn() {
    partial_bit_ = 0x01;
}

void ZacMediator::SetPartialOff() {
    partial_bit_ = 0x00;
}

uint64_t ZacMediator::CalcLowestLba(uint64_t zone_id) {
    return zone_id << 19;
}

uint64_t ZacMediator::CalcZoneId(uint64_t lba) {
    return lba >> 19;
}

bool ZacMediator::IsValidRO(unsigned int report_option) {
    switch (report_option) {
        case kZAC_RO_ALL: // = 0x00;
        case kZAC_RO_EMPTY: // = 0x01;
        case kZAC_RO_IMP_OPEN: // = 0x02;
        case kZAC_RO_EXP_OPEN: // = 0x03;
        case kZAC_RO_CLOSED: // = 0x04;
        case kZAC_RO_FULL: // = 0x05;
        case kZAC_RO_RDONLY: // = 0x06; //remove?
        case kZAC_RO_OFFLINE: // = 0x07; //remove?
        case kZAC_RO_RESET: // = 0x10;
        case kZAC_RO_NON_SEQ: // = 0x11;
        case kZAC_RO_NOT_WP: // = 0x3f;
            return true;
            break;
        default:
            return false;
            break;
    }
    return false;
}

const char* ZacMediator::ZoneConditionString(unsigned int zone_condition) {
    switch (zone_condition) {
        case kZAC_ZC_NOT_WP: // = 0x00;
            return( "Not-Write-Pointer" );
        case kZAC_ZC_EMPTY: // = 0x01;
            return( "Empty" );
        case kZAC_ZC_IMP_OPEN: // = 0x02;
            return( "Implicit-Open" );
        case kZAC_ZC_EXP_OPEN: // = 0x03;
            return( "Explicit-open" );
        case kZAC_ZC_CLOSED: // = 0x04;
            return( "Closed" );
        case kZAC_ZC_RDONLY: // = 0x0d;
            return( "Read-Only" );
        case kZAC_ZC_FULL: // = 0x0e;
            return( "Full");
        case kZAC_ZC_OFFLINE: // = 0x0f;
            return( "Offline" );
    }

    return( "Unknown-Zone-Condition" );
}

const char* ZacMediator::ReportingOptionString(unsigned int report_option) {
    switch (report_option) {
        case kZAC_RO_ALL: // = 0x00;
            return ( "ALL");
        case kZAC_RO_EMPTY: // = 0x01;
            return ( "EMPTY");
        case kZAC_RO_IMP_OPEN: // = 0x02;
            return ( "IMP-Open");
        case kZAC_RO_EXP_OPEN: // = 0x03;
            return ( "EXP-Open");
        case kZAC_RO_CLOSED: // = 0x04;
            return ( "CLOSED");
        case kZAC_RO_FULL: // = 0x05;
            return ( "FULL");
        case kZAC_RO_RDONLY: // = 0x06; //remove?
            return ( "READ-ONLY");
        case kZAC_RO_OFFLINE: // = 0x07; //remove?
            return ( "OFFLINE");
        case kZAC_RO_RESET: // = 0x10;
            return ( "RESET");
        case kZAC_RO_NON_SEQ: // = 0x11;
            return ( "NON-SEQ (true)");
        case kZAC_RO_NOT_WP: // = 0x3f;
            return ( "NOT-WRITE-POINTER");
    }
    return( "Unknown-Reporting-Option" );
}

//THIS IS FOR TESTING ONLY
void ZacMediator::PrintZoneDescriptors(ZacZone *zone_list_ptr, unsigned int num_req) {
    for (int j = 0; j < (int)num_req; j++) {
        ZacZone z = zone_list_ptr[j];
        if (z.zone_type != kZAC_ZT_CONVENTIONAL) {
            std::cout << "\n-------------- Zone:" << (j+1) << "/" << num_req
                      <<" --------------------\n"
                      << std::dec << "zone ID: " << z.zone_id << "\n"
                      << "zone type: sequential preferred\n"
                      << "zone cond: " << ZoneConditionString(z.zone_condition) << "\n"
                      << "zone->start LBA: " << z.start_lba << "\n"
                      << "zone->Write Pointer LBA: " << z.write_pointer << "\n"
                      << "zone->length: " << z.zone_length << "\n"
                      << "------------------------------------------" << std::endl;
        } else {
            std::cout << "\n-------------- Zone:" << (j+1) << "/" << num_req
                      <<" --------------------\n"
                      << "zone ID: " << z.zone_id << "\n"
                      << "zone type: conventional\n"
                      << "zone cond: " << ZoneConditionString(z.zone_condition) << "\n"
                      << "zone->start LBA: " << z.start_lba << "\n"
                      << "zone->length: " << z.zone_length << "\n"
                      << "------------------------------------------" << std::endl;
        }
    } // end print loop
}

} // namespace zac_ha_cmd
