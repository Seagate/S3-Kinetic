//
// common.h
// Do NOT modify or remove this copyright and confidentiality notice.
//
// Copyright 2012 - 2014 Seagate Technology LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology LLC 
// and may be covered under one or more Non-Disclosure Agreements. 
// All or portions are also trade secret. 
// Any use, modification, duplication, derivation, distribution or disclosure
// of this code, for any reason, not expressly authorized is prohibited. 
// All other rights are expressly reserved by Seagate Technology LLC.
// 
// ***************************************************************************************

#pragma once 

#ifdef __linux__
#else
#include <Windows.h>
#include <ntddscsi.h>
#endif

// \file common.h
// \brief Defines the constants structures and function headers that are common to OS & Non-OS code. 
#define MAX_PDS_SUPPORTED		(256)
#define SERIAL_NUM_LEN          (16)

typedef enum _eDataTransferDirection {
   XFER_NO_DATA,
   XFER_DATA_IN,     // Transfer from target to host
   XFER_DATA_OUT,    // Transfer from host to target
   XFER_DATA_OUT_IN, // Transfer from host to target, followed by target to host
   XFER_DATA_IN_OUT, // Transfer from target to host, followed by host to target
} eDataTransferDirection;

typedef enum _eDriveType {
	UNKNOWN_DRIVE,
    ATA_DRIVE,
    SCSI_DRIVE,
	RAID_DRIVE,
} eDriveType;

typedef enum _eInterfaceType {
	UNKNOWN_INTERFACE,
    IDE_INTERFACE,
    SCSI_INTERFACE,
	RAID_INTERFACE	
} eInterfaceType;

typedef enum _eATA_CMDS {
    ATA_NOP                         = 0x00,
    ATASET                          = 0x04,
    ATAPI_RESET                     = 0x08,
    ATA_DEV_RESET                   = 0x08,
    ATA_RECALIBRATE                 = 0x10,
    ATA_READ_SECT                   = 0x20,
    ATA_READ_SECT_NORETRY,
    ATA_READ_LONG_RETRY,
    ATA_READ_LONG_NORETRY,
    ATA_READ_SECT_EXT,
    ATA_READ_DMA_EXT,
    ATA_READ_DMA_QUE_EXT,
    ATA_READ_MAX_ADDRESS_EXT,
    ATA_READ_READ_MULTIPLE_EXT      = 0x29,
    ATA_READ_STREAM_DMA_EXT         = 0x2A,
    ATA_READ_STREAM_EXT,
    ATA_READ_LOG_EXT                = 0x2F,
    ATA_WRITE_SECT,
    ATA_WRITE_SECT_NORETRY,
    ATA_WRITE_LONG_RETRY,
    ATA_WRITE_LONG_NORETRY,
    ATA_WRITE_SECT_EXT,
    ATA_WRITE_DMA_EXT,
    ATA_WRITE_DMA_QUE_EXT,
    ATA_SET_MAX_EXT,
    ATA_WRITE_MULTIPLE_EXT          = 0x39,
    ATA_WRITE_STREAM_DMA_EXT        = 0x3A,
    ATA_WRITE_STREAM_EXT,
    ATA_WRITE_SECTV_RETRY,
    ATA_WRITE_DMA_FUA_EXT,
    ATA_WRITE_DMA_QUE_FUA_EXT,
    ATA_WRITE_LOG_EXT,
    ATA_READ_VERIFY_RETRY,
    ATA_READ_VERIFY_NORETRY,
    ATA_READ_VERIFY_EXT,
    ATA_WRITE_UNCORRECTABLE_EXT     = 0x45,
    ATA_FORMAT_TRACK                = 0x50,
    ATA_PIO_TRUSTED_RECIEVE         = 0x5C,
    ATA_DMA_TRUSTED_RECIEVE,
    ATA_PIO_TRUSTED_SEND,
    ATA_DMA_TRUSTED_SEND,
    ATA_READ_FPDMA_QUEUED,
    ATA_WRITE_FPDMA_QUEUED,
    ATA_FPDMA_NON_DATA              = 0x63,
    ATA_SEND_FPDMA,
    ATA_SEEK                        = 0x70,
    ATA_EXEC_DRV_DIAG               = 0x90,
    ATA_INIT_DRV_PARAM,
    ATA_DLND_CODE,
    ATAPI_COMMAND                   = 0xA0,
    ATAPI_IDENTIFY,
    ATA_SMART                       = 0xB0,
    ATA_DEV_CONFIG,
    ATA_SANITIZE                    = 0xB4,
    ATA_READ_MULTIPLE               = 0xC4,
    ATA_WRITE_MULTIPLE,
    ATA_SET_MULTIPLE,
    ATA_READ_DMA_QUEUED,
    ATA_READ_DMA_RETRY,
    ATA_READ_DMA_NORETRY,
    ATA_WRITE_DMA_RETRY,
    ATA_WRITE_DMA_NORETRY,
    ATA_WRITE_DMA_QUEUED,
    ATA_WRITE_MULTIPLE_FUA_EXT      = 0xCE,
    ATA_GET_MEDIA_STATUS            = 0xDA,
    ATA_ACK_MEDIA_CHANGE,
    ATA_POST_BOOT,
    ATA_PRE_BOOT,
    ATA_DOOR_LOCK,
    ATA_DOOR_UNLOCK,
    ATA_STANDBY_IMMD,
    ATA_IDLE_IMMEDIATE,
    ATA_STANDBY,
    ATA_IDLE,
    ATA_READ_BUF,
    ATA_CHECK_POWER_MODE,
    ATA_SLEEP,
    ATA_FLUSH_CACHE,
    ATA_WRITE_BUF,
    ATA_FLUSH_CACHE_EXT             = 0xEA,
    ATA_IDENTIFY                    = 0xEC,
    ATA_IDENTIFY_DMA                = 0xEE,
    ATA_SET_FEATURE,
    ATA_SECURITY_SET_PASS           = 0xF1,
    ATA_SECURITY_UNLOCK,
    ATA_SECURITY_ERASE_PREP,
    ATA_SECURITY_ERASE_UNIT,
    ATA_SECURITY_FREEZE_LOCK,
    ATA_SECURITY_DISABLE_PASS,
    ATA_LEGACY_TRUSTED_RECIEVE,
    ATA_READ_MAX_ADDRESS,
    ATA_SET_MAX,
    ATA_LEGACY_TRUSTED_SEND         = 0xFB,
    ATA_SEEK_EXT
} eATA_CMDS;


// \struct typedef struct _SANITIZE_ENABLED_OPTS
typedef struct _SANITIZE_ENABLED_OPTS
{
    unsigned char sanitize_cmd_enabled;
    unsigned char block_erase;
    unsigned char over_write;
    unsigned char crypto;
    unsigned char exit_fail_mode;
} SANITIZE_ENABLED_OPTS;

typedef struct _DRIVE_INFO {
    eDriveType drive_type;
	eInterfaceType interface_type;
	unsigned char lun;
	unsigned short device_id; //This is 2-byte long by intention 
    char serial_number[SERIAL_NUM_LEN];    
    char T10_vendor_ident[8+1];
    char product_identification[16+1];
    char product_revision[4+1];
	SANITIZE_ENABLED_OPTS sanitize_opts;
} DRIVE_INFO; 

typedef struct _RAID_DRIVE_INFO {
	unsigned int controller_id;
	unsigned int ld_number; //index into ld list
	unsigned char target_id; //This is byte long by intention
	unsigned char prl; // Primary Raid Level
	unsigned int  number_of_pds;
	int current_pt_target_id;	
	DRIVE_INFO	  physical_drive[MAX_PDS_SUPPORTED];
} RAID_DRIVE_INFO;

// \struct typedef struct _DEVICE
typedef struct _DEVICE
{
#ifdef __linux__
	int fd;
#else
	HANDLE fd;
	SCSI_ADDRESS scsi_addr;
#endif
	unsigned int last_error; // errno in linux or GetLastError in windows. 
	int  os_drive_number;
	DRIVE_INFO drive_info;
	RAID_DRIVE_INFO raid_info;
} DEVICE;

#define M_Byte0(l)  ((uint8_t)(0xFF & ((l) >>  0)))
#define M_Byte1(l)  ((uint8_t)(0xFF & ((l) >>  8)))
#define M_Byte2(l)  ((uint8_t)(0xFF & ((l) >> 16)))
#define M_Byte3(l)  ((uint8_t)(0xFF & ((l) >> 24)))
#define M_Byte4(l)  ((uint8_t)(0xFF & ((l) >> 32)))
#define M_Byte5(l)  ((uint8_t)(0xFF & ((l) >> 40)))
#define M_Byte6(l)  ((uint8_t)(0xFF & ((l) >> 48)))
#define M_Byte7(l)  ((uint8_t)(0xFF & ((l) >> 56)))

// Bit access macros

#define M_BitN(n)   (1 << n)

#define BIT0      (M_BitN(0))
#define BIT1      (M_BitN(1))
#define BIT2      (M_BitN(2))
#define BIT3      (M_BitN(3))
#define BIT4      (M_BitN(4))
#define BIT5      (M_BitN(5))
#define BIT6      (M_BitN(6))
#define BIT7      (M_BitN(7))
#define BIT8      (M_BitN(8))
#define BIT9      (M_BitN(9))
#define BIT10     (M_BitN(10))
#define BIT11     (M_BitN(11))
#define BIT12     (M_BitN(12))
#define BIT13     (M_BitN(13))
#define BIT14     (M_BitN(14))
#define BIT15     (M_BitN(15))
#define BIT16     (M_BitN(16))
#define BIT17     (M_BitN(17))
#define BIT18     (M_BitN(18))
#define BIT19     (M_BitN(19))
#define BIT20     (M_BitN(20))
#define BIT21     (M_BitN(21))
#define BIT22     (M_BitN(22))
#define BIT23     (M_BitN(23))
#define BIT24     (M_BitN(24))
#define BIT25     (M_BitN(25))
#define BIT26     (M_BitN(26))
#define BIT27     (M_BitN(27))
#define BIT28     (M_BitN(28))
#define BIT29     (M_BitN(29))
#define BIT30     (M_BitN(30))
#define BIT31     (M_BitN(31))

// Big endian parameter order, little endian value
#define M_BytesTo4ByteValue(b3, b2, b1, b0)                   (        \
   (uint32_t)(  ((uint32_t)(b3) << 24) | ((uint32_t)(b2) << 16) |          \
               ((uint32_t)(b1) <<  8) | ((uint32_t)(b0) <<  0)  )         \
                                                               )


// Big endian parameter order, little endian value
#define M_BytesTo2ByteValue(b1, b0)                           (        \
   (uint16_t)(  ((uint16_t)(b1) << 8) | ((uint16_t)(b0) <<  0)  )          \
                                                               )

// \fn get_fd(char * filename, DEVICE * device))
// \brief Given a device name (e.g.\\PhysicalDrive0) returns the file descriptor
// \details Function opens the device
//          if everything goes well, it returns a windows file handle
// \todo Add a flags param to allow user to open with O_RDWR, O_RDONLY etc. 
// \param filename name of the device to open
/// \param DEVICE file descriptor, -1 if the handle is invalid. 
// \returns EXIT_SUCCESS if all went well. 
int get_device( char * filename, DEVICE * device);

