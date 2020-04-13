//
// ata_helper.h
// 
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
// *****************************************************************************

// \file ata_helper.h
// \brief Defines the constants structures to help with ATA Specification

#pragma once

#include <stdint.h>
#include "common.h"

#define ATA_GOOD_STATUS         (0x50)

#define ATA_CMD_READ_LOG_EXT				(0x2F)
#define ATA_CMD_WRITE_LOG_EXT				(0x3F)
#define ATA_CMD_DL_MICROCODE                (0x92)
#define ATA_CMD_STBY_IMMI					(0xE0)
#define ATA_CMD_SMART						(0xB0)

#define ATA_DL_MICROCODE_OFFSET_SAVE		(0x03)
#define ATA_DL_MICROCODE_SAVE				(0x07)

#define ATA_SMART_READ_DATA             0xD0
#define ATA_SMART_RDATTR_THRESH         0xD1
#define ATA_SMART_SW_AUTOSVAE           0xD2
#define ATA_SMART_SAVE_ATTRVALUE        0xD3
#define ATA_SMART_EXEC_OFFLINE_IMM      0xD4
#define ATA_SMART_READ_LOG              0xD5
#define ATA_SMART_WRITE_LOG             0xD6
#define ATA_SMART_WRATTR_THRESH         0xD7

#define ATA_SMART_ENABLE                0xD8
#define ATA_SMART_DISABLE               0xD9
#define ATA_SMART_RTSMART               0xDA

#define ATA_SMART_SIG_MID               0x4F
#define ATA_SMART_SIG_HI                0xC2

typedef enum _eAtaCmdType {
   ATA_CMD_TYPE_UNKNOWN,
   ATA_CMD_TYPE_TASKFILE,
   ATA_CMD_TYPE_EXTENDED_TASKFILE,
   ATA_CMD_TYPE_NON_TASKFILE,
   ATA_CMD_TYPE_SOFT_RESET,
   ATA_CMD_TYPE_HARD_RESET,
   ATA_CMD_TYPE_FPDMA,
   ATA_CMD_TYPE_PACKET
} eAtaCmdType;

typedef enum _eAtaProtocol {
   ATA_PROTOCOL_UNKNOWN,      // initial setting
   ATA_PROTOCOL_PIO,          // various, includes r/w
   ATA_PROTOCOL_DMA,          // various, includes r/w
   ATA_PROTOCOL_NO_DATA,      // various (e.g. NOP)
   ATA_PROTOCOL_DEV_RESET,    // DEVICE RESET
   ATA_PROTOCOL_DEV_DIAG,     // EXECUTE DEVICE DIAGNOSTIC
   ATA_PROTOCOL_DMA_QUE,      // various, includes r/w
   ATA_PROTOCOL_PACKET,       // PACKET
   ATA_PROTOCOL_PACKET_DMA,   // PACKET
   ATA_PROTOCOL_DMA_FPDMA,    // READ/WRITE FPDMA QUEUED
   ATA_PROTOCOL_SOFT_RESET,   // Needed for SAT spec. Table 101 (Protocol field)
   ATA_PROTOCOL_HARD_RESET,   // Needed for SAT spec. Table 101 (Protocol field)
   ATA_PROTOCOL_RET_INFO,     // Needed for SAT spec. Table 101 (Protocol field)
   ATA_PROTOCOL_MAX_VALUE,    // Error check terminator
} eAtaProtocol;


// \struct ATA_RETURN_TFRS
// \param error
// \param secCntExt
// \param secCnt
// \param lbaLowExt
// \param lbaLow
// \param lbaMidExt
// \param lbaMid
// \param lbaHiExt
// \param lbaHiExt
// \param device
// \param status
typedef struct _ATA_RETURN_TFRS
{
    uint8_t     error;
    uint8_t     secCntExt;
    uint8_t     secCnt;
    uint8_t     lbaLowExt;
    uint8_t     lbaLow;
    uint8_t     lbaMidExt;
    uint8_t     lbaMid;
    uint8_t     lbaHiExt;
    uint8_t     lbaHi;
    uint8_t     device;
    uint8_t     status;
} ATA_RETURN_TFRS;

typedef struct _ATA_LBA_TFR_BLOCK {
   uint8_t CommandStatus;
   uint8_t ErrorFeature;

   uint8_t LbaLow;
   uint8_t LbaMid;
   uint8_t LbaHi;
   uint8_t DeviceHead;

   uint8_t LbaLow48;
   uint8_t LbaMid48;
   uint8_t LbaHi48;
   uint8_t Feature48;

   uint8_t SectorCount;
   uint8_t SectorCount48;
   uint8_t icc;
   uint8_t DeviceControl;
   // Pad it out to 16 bytes
   uint8_t aux1;
   uint8_t aux2;
   uint8_t aux3;
   uint8_t aux4;
} ATA_LBA_TFR_BLOCK;

// \struct typedef struct _ATA_PT_CMD_OPTS
typedef struct _ATA_PT_CMD_OPTS
{
    eAtaCmdType cmd_type;
    eDataTransferDirection cmd_direction; 
    eAtaProtocol cmd_protocol;
    ATA_LBA_TFR_BLOCK tfr;
    ATA_RETURN_TFRS rtfr;
    uint8_t * pData;
    uint32_t data_size;
    uint8_t * psense_data;
    uint8_t sense_size;  
} ATA_PT_CMD_OPTS;