#ifndef KINETIC_HA_ZAC_CMDS_INCLUDES_ZAC_MEDIATOR_H_
#define KINETIC_HA_ZAC_CMDS_INCLUDES_ZAC_MEDIATOR_H_
#include "zoned_ata_standards.h"
#include "ata_cmd_handler.h"
#include <set>
#include <ostream>
#include <iostream>
#include <sstream>

namespace zac_ha_cmd {

static const unsigned int ZONED_DEVICE_INFO_LENGTH = 32;
/////////////////////////////////////////////////////////
/// ZacZone Struct is for Testing & Debug Purposes Only
typedef struct ZacZone {
    uint64_t                    zone_length;
    uint64_t                    start_lba;
    uint64_t                    write_pointer;
    uint64_t                    zone_id;
    uint8_t                     zone_type;
    uint8_t                     zone_condition;
    uint8_t                     non_seq;

    friend std::ostream& operator<<(std::ostream& os, const ZacZone& zone) {
        os << "ZAC zone:  len " << zone.zone_length << ", start LBA " << zone.start_lba
           << ", WP " << zone. write_pointer << ", id " << zone.zone_id << ", type " << (uint16_t)zone.zone_type
           << ", cond " << (uint16_t)zone.zone_condition << ", non seq " << (uint16_t)zone.non_seq;
        return os;
    }
} ZacZone;

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
/// ZacMediator  --  Kinetic Host Aware Command Set
/// -------------------------------------------------------------------------------
/// @Summary:
/// - The ZacMediator defines/controls how @smrdb and the Zoned-device ATA Command Set
///   interact. Loose coupling between these colleague objects is achieved by requiring
///   communication to occur with the ZacMediator, rather than with each other
///
/// - The ZacMediator controls the construction of all Host Aware commands
///   and leverages ATA Command Handler to issue them to the underlying SCSI device
///
/// - The command codes & definitions are stored within @zoned_ata_standards
/// -------------------------------------------------------------------------------
/// Associated Jira Tasks
/// - https://jira.seagate.com/jira/browse/ASOLAMARR-451
/// - https://jira.seagate.com/jira/browse/ASOLAMARR-189
/// - https://jira.seagate.com/jira/browse/ASOLAMARR-458
/// -------------------------------------------------------------------------------
/// @Member Variables
/// - @device_fd_           -- smr device file descriptor
/// - @sg_cmd_              -- Pointer to the @ATA_CMD_HANDLER class
/// - @allocate_zone_mutex_ -- Guard for Reset WP calls
/// - @error_log_mutex_     -- Gaurd for Open/Close/Write to syslog
/// - @log_open_            -- Bool indicating if syslog is open
/// - @log_stream_          -- Shared StringStream for constructing syslog events
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
///             │   ├── zac_mediator.cc -- CURRENT FILE
///             │   ├── main.cc  --  "Standalone `zac_local` library functions"
///             │   └── makefile
///             │
///             ├── src/
///             │    └── zac_ha_exercise_drive.cc  --  "X86 Unit Tests"
///             │
///             └── CMakeLists.txt  --  "Compilation Rules for linking to SMRDB"
/// -------------------------------------------------------------------------------
class ZacMediator {
    public:
    static pthread_mutex_t allocate_zone_mutex_;
    static pthread_mutex_t error_log_mutex_;
    /// Helper variables for @cmd_field_opts byte field bit twiddling
    static const unsigned int kRETPAGE_SECTOR_MASK = 0xFFF00000;
    static const unsigned int kZAC_CMD_TMP_MASK = 0xFF000;
    static const unsigned int kBYTE_THREE_MASK = 0xFC0;
    static const unsigned int kACT_FIELD_MASK = 0x38;
    static const unsigned int kTXFER_PROTO_MASK = 0x07;
    static const uint8_t kRETPAGE_SECTOR_OFFSET = 20;
    static const uint8_t kZAC_CMD_TMP_OFFSET = 12;
    static const uint8_t kBYTE_FIVE_OFFSET = 8;
    static const uint8_t kBYTE_THREE_OFFSET = 6;
    static const uint8_t kACT_FIELD_OFFSET = 3;
    static const uint8_t kTXFER_PROTO_OFFSET = 0;
    /// Other global helper variables
    static const uint8_t kLAST_CONVENTIONAL_ZONE_ID = 63;
    static const int kSIZE_LIMIT = 524288; // Max size for single write cmd
    static const unsigned int kMAX_ZONE_DESC_RET = 2499; // Max # zone-desc returned for ReportZones

    ////////////////////////////////////////////////////////////////////////////////////
    /// ZacMediator() -- Constructor
    /// -------------------------------------------------------------------------------
    /// Assigns member variable @sg_cmd_ to provided @sg_ptr
    /// Initializes ATA device File Descriptor device_fd_ to default value of 0
    /// Initializes syslog state log_open_ to false
    /// -------------------------------------------------------------------------------
    /// - @param[in] sg_ptr -- Pointer to Ata Command Handler object
    /// -------------------------------------------------------------------------------
    explicit ZacMediator(AtaCmdHandler* sg_ptr);
    ZacMediator(const ZacMediator &rhs) = delete; // dissallow copy & assign
    ZacMediator& operator=(const ZacMediator &rhs) = delete; // dissallow copy & assign

    ////////////////////////////////////////////////////////////////////////////////////
    /// ~ZacMediator() -- Destructor
    /// -------------------------------------------------------------------------------
    /// Sets member variable @sg_cmd_ to nullptr
    /// closes syslog via private CloseLog() function
    /// -------------------------------------------------------------------------------
    ~ZacMediator();

    ////////////////////////////////////////////////////////////////////////////////////
    /// OpenDevice()
    /// -------------------------------------------------------------------------------
    /// Open device handle specified by @device_path parameter
    /// -------------------------------------------------------------------------------
    /// - @param[in] device_path -- ATA device handle (e.g. /dev/sda)
    /// - @return[out] int       -- Device File Descriptor for Success. Negative errno for fail
    /// -------------------------------------------------------------------------------
    int OpenDevice(std::string device_path);

    ////////////////////////////////////////////////////////////////////////////////////
    /// CloseDevice()
    /// -------------------------------------------------------------------------------
    /// Close device specified by member variable @device_fd_ file descriptor
    /// -------------------------------------------------------------------------------
    /// - @return[out] int -- 0 for successful close. Negative errno for failure
    /// -------------------------------------------------------------------------------
    int CloseDevice();

    /////////////////////////////////////////////////////////
    /// ToString Methods for Debug and Testing
    const char* ZoneConditionString(unsigned int zone_condition);
    const char* ReportingOptionString(unsigned int report_option);

    ////////////////////////////////////////////////////////////////////////////////////
    /// ReportZones (several permutations)
    /// - Caller: smrdb/smrdisk/Disk.h (primary user)
    /// -------------------------------------------------------------------------------
    /// Interface for SMRDB to call Report Zones Ext 4Ah/00h for several pre-determined
    /// zone conditions
    ///
    /// Returns # of zones reported with certain zone conditiion(open, closed, empty, full etc.)
    /// -------------------------------------------------------------------------------
    /// - @param[in] start_lba      -- lba to start report from
    /// - @param[in] *num_zone_desc -- determine size of returnpage (7 zone_desc = 1 page of 512b)
    /// - @return[out] status       -- number of zones satisfying zone state requested
    /// -------------------------------------------------------------------------------
    int ReportOpenZones(uint64_t start_lba, unsigned int num_zone_desc);
    int ReportClosedZones(uint64_t start_lba, unsigned int num_zone_desc);
    int ReportEmptyZones(uint64_t start_lba, unsigned int num_zone_desc);
    int ReportFullZones(uint64_t start_lba, unsigned int num_zone_desc);
    int ReportAllZones(uint64_t start_lba, unsigned int num_zone_desc);

    ////////////////////////////////////////////////////////////////////////////////////
    /// ReportZonesCustom(start_lba, num_zone_desc, report_option)
    /// - Caller: can be called anywhere zac_mediator is found
    /// -------------------------------------------------------------------------------
    /// Interface for Report Zones EXT command 4Ah/00h, DMA
    /// Allows for any valid reporting option to be supplied by user
    /// -------------------------------------------------------------------------------
    /// - @param[in] zone_id  -- starting LBA for Report Zones
    /// - @param[in] num_zone_desc -- how many zone descriptors will be sent back
    /// - @param[in] report_option -- "kZAC_RO_<zone-cond>"
    ///      --> zone-cond:(ALL,EMPTY,IMP_OPEN,EXP_OPEN,CLOSE,FULL,NON_SEQ,NOT_WP)
    /// - @return[out] status -- ReportZonesExt exit status
    /// -------------------------------------------------------------------------------
    int ReportZonesCustom(uint64_t start_lba,
                          unsigned int num_zone_desc,
                          unsigned int report_option = kZAC_RO_ALL);

    ////////////////////////////////////////////////////////////////////////////////////
    /// AllocateZone(zone_id, *lba)  -   9Fh/04h, Non-Data
    /// - Caller: smrdb/smrdisk/Disk.h (primary user)
    /// -------------------------------------------------------------------------------
    /// Interface for SMRDB to Reset WP Ext 9Fh/04h
    /// If valid zoneid, calc zone's low lba, build CDB, call ZacResetWpExt
    /// If ResetWpExt fail, sleep 2ms, retry 1x. Failures log to /mnt/util/zacwp.log
    /// Regardless of ResetWpExt status, always return success
    /// Will result in media cache hit on fail, but will keep us operational
    /// -------------------------------------------------------------------------------
    /// - @param[in] zone_id  -- zone to reset (zone's lowest lba calculated within)
    /// - @param[in] *lba     -- populated with zone_id's lowest lba on success
    /// - @return[out] status -- ResetWpExt exit status(note: testing always success)
    /// -------------------------------------------------------------------------------
    int AllocateZone(unsigned int zone_id, uint64_t *lba);

    ////////////////////////////////////////////////////////////////////////////////////
    /// AllocateZone(zone_id)  -   9Fh/04h, Non-Data
    /// - Caller: smrdb/smrdisk/Disk.h (primary user)
    /// -------------------------------------------------------------------------------
    /// Interface for SMRDB to Reset WP Ext 9Fh/04h on zone @zone_id
    /// If valid zoneid, calc zone's low lba, build CDB, call ZacResetWpExt
    /// Will call reset wp once and return status.
    /// -------------------------------------------------------------------------------
    /// - @param[in] zone_id  -- zone to reset (zone's lowest lba calculated within)
    /// - @return[out] status -- ResetWpExt exit status(note: testing always success)
    /// -------------------------------------------------------------------------------
    int AllocateZone(unsigned int zone_id);

    ////////////////////////////////////////////////////////////////////////////////////
    /// ResetAllZones()  -   9Fh/04h, Non-Data
    /// -------------------------------------------------------------------------------
    /// Reset WP on ALL device WP zones
    /// -------------------------------------------------------------------------------
    /// - @return[out] int - Status of RWP call
    /// -------------------------------------------------------------------------------
    int ResetAllZones();

    ////////////////////////////////////////////////////////////////////////////////////
    /// OpenZone(start_lba)
    /// -------------------------------------------------------------------------------
    /// EXPLICITLY open zone associated with @start_lba
    /// -------------------------------------------------------------------------------
    /// - @return[out] int - Status of Open Zone call
    /// -------------------------------------------------------------------------------
    int OpenZone(uint64_t start_lba);

    ////////////////////////////////////////////////////////////////////////////////////
    /// OpenAllZones()
    /// -------------------------------------------------------------------------------
    /// EXPLICITLY open all zones on device
    /// -------------------------------------------------------------------------------
    /// - @return[out] int - Status of Open Zone call
    /// -------------------------------------------------------------------------------
    int OpenAllZones();

    int CloseZone(uint64_t start_lba);
    int CloseAllZones();

    ////////////////////////////////////////////////////////////////////////////////////
    /// FinishZone(start_lba)  -   9Fh/02h, Non-Data
    /// -------------------------------------------------------------------------------
    /// Move WP to last lba zone and set zone state to Full
    /// This invalidates the WP of the zone and must be reset if
    /// any write operations wish to target this zone
    /// -------------------------------------------------------------------------------
    /// - @param[in] start_lba - first lba of target zone to finish
    /// - @return[out] int - status of Finish Zone Call
    /// -------------------------------------------------------------------------------
    int FinishZone(uint64_t start_lba);
    int FinishAllZones();

    ////////////////////////////////////////////////////////////////////////////////////
    /// Write @data to lba @wp_lba
    int WriteZone(uint64_t wp_lba, void *data, size_t size);

    ////////////////////////////////////////////////////////////////////////////////////
    /// ATA flush cache command. - @return[out]: status of command
    int FlushCacheAta();

    ////////////////////////////////////////////////////////////////////////////////////
    /// Read contents of Zone @zone_id. Print to std out
    int ReadZone(unsigned int zone_id); //need to add parameters for read size etc.

    ////////////////////////////////////////////////////////////////////////////////////
    /// GetZoneConditionSnapShot()
    /// - Caller: can be called anywhere zac_mediator is found.
    /// - Primary Purpose: Logging & Debug
    /// -------------------------------------------------------------------------------
    /// Using the Report Zones EXT command
    /// For Each Zone Condition / Reporting Option, retrieve # of zones in each Zone Condition
    ///
    ///Supported Zone Conditions:
    /// - ALL Zones, FULL Zones, EMPTY Zones, IMP_OPEN Zones, EXP_OPEN Zones,
    ///    CLOSED Zones, RESET Zones, NON_SEQ Zones, NOT_WP Zones.
    /// -------------------------------------------------------------------------------
    /// - @return[out] std::string  -- formatted string with counts for each zone condition
    /// -------------------------------------------------------------------------------
    std::string GetZoneConditionSnapShot();

    ////////////////////////////////////////////////////////////////////////////////////
    /// GetZoneConditionSnapShot(std::set<unsigned int>)  *overloaded*
    /// - Caller: can be called anywhere zac_mediator is found.
    /// - Primary Purpose: Logging & Debug
    /// -------------------------------------------------------------------------------
    /// Using the Report Zones EXT command
    /// For Each Zone Condition / Reporting Option, retrieve # of zones in each Zone Condition
    /// Overloaded method, which accepts a "white list" set of reporting options. This allows
    /// the caller to retreive information associated with only the reporting options in
    /// @target_report_options
    /// -------------------------------------------------------------------------------
    /// - @param[in] target_report_options  -- whitelist set of requested zone conditions to report
    /// - @return[out] std::string  -- formatted string with counts for each zone condition
    /// -------------------------------------------------------------------------------
    std::string GetZoneConditionSnapShot(std::set<unsigned int> &target_report_options);

    ////////////////////////////////////////////////////////////////////////////////////
    /// GetZoneDescriptorList(zone_list, start_zone_id, report_option, num_zones_requested)
    /// - Caller: Testing Method only, can be called anywhere zac_mediator is found
    /// -------------------------------------------------------------------------------
    /// Using the Report Zones EXT command 4Ah/00h, DMA
    /// Report and Sequentially populate @zone_list with @num_zones_requested number of zones
    /// starting from  @starting_zone_id
    ///
    /// -------------------------------------------------------------------------------
    /// - @param[in] starting_zone_id -- id as starting point for report zones call
    /// - @param[in] report_option -- zone condition to report (i.e. report all open from zoneid)
    /// - @param[in] *zone_list_ptr -- Empty Array of ZacZone structs to populate
    /// - @return[out] boolean  -- status of report zones call
    /// - @return[out] *zone_list_ptr  -- Populated Array of ZacZone structs
    /// -------------------------------------------------------------------------------
    bool GetZoneDescriptorList(ZacZone* zone_list_ptr,
                               uint64_t start_zone_id,
                               unsigned int report_option = kZAC_RO_ALL,
                               unsigned int num_zones_requested = 0);

    ////////////////////////////////////////////////////////////////////////////////////
    /// GetPairZoneDescriptors(zone_list, start_zone_id, second_zone_id)  - Populate 2 Zone Structs
    /// - Caller: Testing Method only, can be called anywhere zac_mediator is found
    /// -------------------------------------------------------------------------------
    /// Using the Report Zones EXT command 4Ah/00h, DMA
    /// Report and populate zaczone list with zone descriptor info
    /// for the two target zone id's (@start_zone_id & @second_zone_id)
    ///
    /// -------------------------------------------------------------------------------
    /// - @param[in] start_zone_id  --  id as starting point for report zones call
    /// - @param[in] second_zone_id -- second id to populate with zone descriptor information
    /// - @param[in] *zone_list_ptr -- Empty Array of ZacZone structs to populate
    /// - @return[out] boolean  -- status of report zones call
    /// - @return[out] *zone_list_ptr  -- Populated Array of ZacZone structs
    /// -------------------------------------------------------------------------------
    /// Check if zones are adjacent If !adjacent, & distance between them is too large..
    /// Two calls must be made
    bool GetPairZoneDescriptors(ZacZone* zone_list_ptr,
                         uint64_t start_zone_id,
                         uint64_t second_zone_id);

    ////////////////////////////////////////////////////////////////////////////////////
    /// GetZoneInfo(zone, zone_id)
    /// - Caller: Testing Method only, can be called anywhere zac_mediator is found
    /// -------------------------------------------------------------------------------
    /// Using the Report Zones EXT command 4Ah/00h, DMA
    /// ReportZones from @zone_id & populate @zone struct with descriptor details (only one zone)
    ///
    /// -------------------------------------------------------------------------------
    /// - @param[in] zone  --  ZacZone struct to be populated
    /// - @param[in] zone_id -- id of zone to populate and starting point for report zones call
    /// - @return[out] zone -- Populated ZacZone struct
    /// -------------------------------------------------------------------------------
    void GetZoneInfo(ZacZone *zone_ptr, unsigned int zone_id);

    /////////////////////////////////////////////////////////
    /// PrintZoneDescriptors(*zone_list_ptr, num_req)
    /// print out details of already populated zone structs contained in @zone_arr
    /// For debugging, extra logging and testing only (will not be called by other methods)
    /// --------------------------------------------------------------------------------
    /// - @param[in] zone_list_ptr  --  POPULATED Array of ZacZone structs
    /// - @param[in] num_req -- number of zones to print to std out
    /// -------------------------------------------------------------------------------
    void PrintZoneDescriptors(ZacZone *zone_list_ptr, unsigned int num_req = 0);

    private:
    /////////////////////////////////////////////////////////
    /// REPORT ZONES EXT command -- 4Ah/00h, DMA
    int ZacReportZonesExt(uint64_t start_lba,
                          unsigned int buff_size,
                          uint32_t cmd_field_opts);

    /////////////////////////////////////////////////////////
    /// RESET WRITE POINTER EXT command -- 9Fh/04h, Non-Data
    int ZacResetWpExt(uint64_t start_lba, uint32_t cmd_field_opts);

    /////////////////////////////////////////////////////////
    /// OPEN ZONE EXT command -- 9Fh/03h, Non-Data
    int ZacOpenZoneExt(uint64_t start_lba, uint32_t cmd_field_opts);

    /////////////////////////////////////////////////////////
    /// CLOSE ZONE EXT command -- 9Fh/01h, Non-Data
    int ZacCloseZoneExt(uint64_t start_lba, uint32_t cmd_field_opts);

    /////////////////////////////////////////////////////////
    /// FINISH ZONE EXT command -- 9Fh/02h, Non-Data
    int ZacFinishZoneExt(uint64_t start_lba, uint32_t cmd_field_opts);

    /////////////////////////////////////////////////////////
    /// WRITE EXT command -- 9Fh/02h, Data
    int ZacWriteExt(uint64_t start_lba, uint32_t cmd_field_opts, void *data, size_t size);

    /////////////////////////////////////////////////////////
    /// Determine Size (bytes) Required for Return Page Result (Report Zones)
    unsigned int CalcRetPageBuff(unsigned int num_req_zones);

    /////////////////////////////////////////////////////////
    /// Read from lba + size ATA passthrough
    int ZacReadZoneExt(uint64_t start_lba, unsigned int buff_size, uint32_t cmd_field_opts);

    /////////////////////////////////////////////////////////
    /// Fill ATA 16 CDB with fields from @cmd_field_opts_
    int PopulateCdb(uint64_t start_lba, uint8_t byte_two, uint32_t cmd_field_opts, uint8_t *cdb);

    /////////////////////////////////////////////////////////
    /// Helper Functions
    uint32_t ParseDword();
    uint64_t ParseQword(int position);

    /////////////////////////////////////////////////////////
    /// populate zone struct w/ descriptor contents from buffer
    void PopulateZone(ZacZone* zone, int zone_num);

    /////////////////////////////////////////////////////////
    /// Set Partial bit for Report Zones Cmd (affects return page ct & # zone descriptors returned)
    /// NOTE: Lamarr HA ignores the partial bit
    void SetPartialOn();
    void SetPartialOff();

    /////////////////////////////////////////////////////////
    /// Calculate Zone's first lba
    uint64_t CalcLowestLba(uint64_t zone_id);

    /////////////////////////////////////////////////////////
    /// Calculate Zone ID from lba
    uint64_t CalcZoneId(uint64_t lba);

    /////////////////////////////////////////////////////////
    /// Validate Reporting Option
    bool IsValidRO(unsigned int report_option);

    /////////////////////////////////////////////////////////
    /// Determine if zone id is within device limits (zone >= 0 && zone <= higheste wp zone)
    inline bool IsValidZoneId(unsigned int id) { return ( id <= kLAMARR_HIGHESTWP_ZONE); } //NOLINT

    /////////////////////////////////////////////////////////
    /// Return True if zone id > last conventional and <= highest wp zone
    inline bool IsWpZoneId(unsigned int id) { return ((id >= kLAST_CONVENTIONAL_ZONE_ID) && (id <= kLAMARR_HIGHESTWP_ZONE)); } //NOLINT
    inline bool IsValidLBA(uint64_t lba) { return lba <= kLAMARR_HIGHEST_LBA; }

    /////////////////////////////////////////////////////////
    /// Debug Logging
    void OpenLog();
    void CloseLog();
    void ZacLogError();

    /////////////////////////////////////////////////////////
    /// Member Variables
    bool log_open_;
    int device_fd_;
    uint8_t partial_bit_;
    AtaCmdHandler* sg_cmd_;
    std::stringstream log_stream_;
};

} // namespace zac_ha_cmd

#endif  // KINETIC_HA_ZAC_CMDS_INCLUDES_ZAC_MEDIATOR_H_
