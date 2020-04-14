#ifndef KINETIC_HA_ZAC_CMDS_INCLUDES_ZONED_ATA_STANDARDS_H_
#define KINETIC_HA_ZAC_CMDS_INCLUDES_ZONED_ATA_STANDARDS_H_
#include <stdint.h>
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
/// Zoned_Ata_Standards
///     - Used By: @ZacMediator
/// -------------------------------------------------------------------------------
/// @Summary:
/// - Definitions for all Host Aware and Related SCSI command codes & return codes
/// - Codes based upon r4-ZAC from T13
/// - For Seagate Lamarr HA devices (ZAC ATA ST8000AS0022)
/// -------------------------------------------------------------------------------
/// Lamarr Specific Definitions
static const unsigned int kLAMARR_LBA_SIZE = 512;
static const unsigned int kLAMARR_ZONE_LBA_COUNT = 524288;
static const unsigned int kLAMARR_MIN_WRITE = 4096;
static const unsigned int kLAMARR_HIGHESTWP_ZONE = 29808;
static const uint64_t kLAMARR_HIGHEST_LBA = 15628053168;

/// ZAC Zone Types
static const unsigned int kZAC_ZT_CONVENTIONAL         = 0x01;
static const unsigned int kZAC_ZT_SEQUENTIAL_PREF      = 0x03;

/// ZAC Zone Definitions
static const unsigned int kZAC_ZC_NOT_WP = 0x00;
static const unsigned int kZAC_ZC_EMPTY = 0x01;
static const unsigned int kZAC_ZC_IMP_OPEN = 0x02;
static const unsigned int kZAC_ZC_EXP_OPEN = 0x03;
static const unsigned int kZAC_ZC_CLOSED = 0x04;
static const unsigned int kZAC_ZC_RDONLY = 0x0d;
static const unsigned int kZAC_ZC_FULL = 0x0e;
static const unsigned int kZAC_ZC_OFFLINE = 0x0f;

/// ZAC Command Templates
static const unsigned int kZAC_ATA_ZAC_MANAGEMENT_IN = 0x4A;
static const unsigned int kZAC_ATA_ZAC_MANAGEMENT_OUT = 0x9F;

/// Report Zones
static const unsigned int kZAC_ATA_REPORT_ZONES_EXT_AF = 0x00;
static const unsigned int kZAC_ATA_REPORT_ZONES_EXT_CMD = 0x4A;
static const unsigned int kZD_PAGE_SIZE = 512;
static const unsigned int kZD_PAGE_HDR_SIZE = 64;
static const unsigned int kZAC_ZONE_DESC_LENGTH = 64;
static const unsigned int kZAC_ZONE_DESCRIPTOR_OFFSET = 64;

/// Report Zones Reporting Options
static const unsigned int kZAC_RO_ALL = 0x00;
static const unsigned int kZAC_RO_EMPTY = 0x01;
static const unsigned int kZAC_RO_IMP_OPEN = 0x02;
static const unsigned int kZAC_RO_EXP_OPEN = 0x03;
static const unsigned int kZAC_RO_CLOSED = 0x04;
static const unsigned int kZAC_RO_FULL = 0x05;
static const unsigned int kZAC_RO_RDONLY = 0x06; // not supported on lamarr
static const unsigned int kZAC_RO_OFFLINE = 0x07; // not supported on lamarr
static const unsigned int kZAC_RO_RESET = 0x10;
static const unsigned int kZAC_RO_NON_SEQ = 0x11;
static const unsigned int kZAC_RO_NOT_WP = 0x3f;

/// Reset WP
static const unsigned int kZAC_SG_RESET_WRITE_POINTER_CDB_OPCODE = 0x94;
static const unsigned int kZAC_SG_RESET_WRITE_POINTER_CDB_SA = 0x04;
static const unsigned int kZAC_ATA_RESET_WRITE_POINTER_EXT_CMD = kZAC_ATA_ZAC_MANAGEMENT_OUT;
static const unsigned int kZAC_ATA_RESET_WRITE_POINTER_EXT_AF = 0x04;

/// Open Zone
static const unsigned int kZAC_SG_OPEN_ZONE_CDB_OPCODE = 0x94;
static const unsigned int kZAC_SG_OPEN_ZONE_CDB_SA = 0x03;
static const unsigned int kZAC_ATA_OPEN_ZONE_EXT_CMD = kZAC_ATA_ZAC_MANAGEMENT_OUT;
static const unsigned int kZAC_ATA_OPEN_ZONE_EXT_AF = 0x03;

/// Close Zone
static const unsigned int kZAC_SG_CLOSE_ZONE_CDB_OPCODE = 0x94;
static const unsigned int kZAC_SG_CLOSE_ZONE_CDB_SA = 0x01;
static const unsigned int kZAC_ATA_CLOSE_ZONE_EXT_CMD = kZAC_ATA_ZAC_MANAGEMENT_OUT;
static const unsigned int kZAC_ATA_CLOSE_ZONE_EXT_AF = 0x01;

/// Finish Zone
static const unsigned int kZAC_SG_FINISH_ZONE_CDB_OPCODE = 0x94;
static const unsigned int kZAC_SG_FINISH_ZONE_CDB_SA = 0x02;
static const unsigned int kZAC_ATA_FINISH_ZONE_EXT_CMD = kZAC_ATA_ZAC_MANAGEMENT_OUT;
static const unsigned int kZAC_ATA_FINISH_ZONE_EXT_AF = 0x02;

/// SG_ATA
static const unsigned int kZAC_SG_ATA16_CDB_OPCODE_ = 0x85;
static const unsigned int kATA_PROTO_DMA = 0x06;
static const unsigned int kATA_PROTO_NON = 0x03;
static const unsigned int kCDB_WBYTE_2 = 0x06; // CDB Byte Two for Write
static const unsigned int kCDB_RZACBYTE_2 = 0x0e; // CDB Byte Two for Read, Report Zones
static const unsigned int kCDB_DEFZACBYTE_2 = 0x00; // Default CDB Byte Two
static const unsigned int kATA_FLUSH_CACHE_EXT = 0xEA;
static const unsigned int kATA_READ_DMA_EXT = 0x25;
static const unsigned int kATA_WRITE_DMA_EXT = 0x35;
static const unsigned int kZAC_ATA_IDENTIFY = 0xEC;
static const unsigned int kZAC_ATA_EXEC_DEV_DIAGNOSTIC = 0x90;
static const unsigned int kCDB_ATA_DEVDIAG_BYTE_2 = 0x20;

/// SENSE KEY CODES
/// TODO(ggomez): Not sure how many of these we will keep
static const unsigned int NO_SENSE = 0x00;
static const unsigned int RECOVERED_ERROR = 0x01;
static const unsigned int NOT_READY = 0x02;
static const unsigned int MEDIUM_ERROR = 0x03;
static const unsigned int HARDWARE_ERROR = 0x04;
static const unsigned int ILLEGAL_REQUEST = 0x05;
static const unsigned int UNIT_ATTENTION = 0x06;
static const unsigned int DATA_PROTECT = 0x07;
static const unsigned int BLANK_CHECK = 0x08;
static const unsigned int VENDOR_SPECIFIC = 0x09;
static const unsigned int COPY_ABORTED = 0x0A;
static const unsigned int COMMAND_ABORTED = 0x0B;
static const unsigned int VOLUME_OVERFLOW = 0x0D;
static const unsigned int MISCOMPARE = 0x0E;
static const unsigned int COMPLETED = 0x0F;

#endif  // KINETIC_HA_ZAC_CMDS_INCLUDES_ZONED_ATA_STANDARDS_H_
