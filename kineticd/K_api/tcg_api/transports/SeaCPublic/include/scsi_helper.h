//
// scsi_helper.h   Header for helper functions for scsi
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

// \file scsi_helper.h
// \brief Defines the constants structures to help with SCSI implementation

#pragma once

#include <stdint.h>
#include "common.h"
#include "ata_helper.h"

// Defined by SPC3 as the maximum sense length
#define SPC3_SENSE_LEN  (252)

#define SAT_VPD_PAGE				(0x89)

#define INQ_RETURN_DATA_LENGTH      (96) 

#define CDB_LEN_6       (6)
#define CDB_LEN_10      (10)
#define CDB_LEN_12      (12)
#define CDB_LEN_16		(16)

#define SCSI_SENSE_CUR_INFO_FIXED        (0x70)
#define SCSI_SENSE_DEFER_ERR_FIXED       (0x71)
#define SCSI_SENSE_CUR_INFO_DESC         (0x72)
#define SCSI_SENSE_DEFER_ERR_DESC        (0x73)
#define SCSI_SENSE_INFO_VALID_BIT_SET    (0x80)

#define SCSI_SENSE_ADDT_LEN_INDEX        (7)
#define SCSI_DESC_FORMAT_DESC_INDEX      (8)
#define SCSI_DESC_FORMAT_DESC_LEN        (9)
#define SCSI_SENSE_INFO_FIELD_MSB_INDEX  (3)
#define SCSI_FIXED_FORMAT_CMD_INFO_INDEX (8)

//This struct was removed because it uses bitfields which are not stardard between compilers and not portable which can lead to lots of headaches, but is left in for readability.
/*
typedef struct _tSenseData
{
   uint8_t ResponseCode:7;                    // BYTE   0       : 0-6
   uint8_t Valid:1;                           // BYTE   0       : 7
   uint8_t Byte1IsObsolete; //SegmentNumber;  // BYTE   1
   uint8_t SenseKey:4;                        // BYTE   2       : 0-3
   uint8_t Byte2Bit4Reserved:1;               // BYTE   2       : 4
   uint8_t IncorrectLengthIndicator:1;        // BYTE   2       : 5
   uint8_t EndOfMedia:1;                      // BYTE   2       : 6
   uint8_t FileMark:1;                        // BYTE   2       : 7
   uint8_t Information[4];                    // BYTES  3 -  6
   uint8_t AdditionalSenseLength;             // BYTE   7
   uint8_t CommandSpecificInformation[4];     // BYTE   8 - 11
   uint8_t AdditionalSenseCode;               // BYTE  12
   uint8_t AdditionalSenseCodeQualifier;      // BYTE  13
   uint8_t FieldReplaceableUnitCode;          // BYTE  14
   uint8_t SenseKeySpecific[3];               // BYTES 15 - 17
} tSenseData;
*/
typedef struct _tSenseData
{
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
    uint8_t byte6;
    uint8_t byte7;
    uint8_t byte8;
    uint8_t byte9;
    uint8_t byte10;
    uint8_t byte11;
    uint8_t byte12;
    uint8_t byte13;
    uint8_t byte14;
    uint8_t byte15;
    uint8_t byte16;
    uint8_t byte17;
}tSenseData;



// \struct SCSI_STATUS
// \param sense_key
// \param acq
// \param ascq
typedef struct _SCSI_STATUS
{
	uint8_t		status_scsi;
    uint8_t     format;
    uint8_t     sense_key;
    uint8_t     acq;
    uint8_t     ascq;
} SCSI_STATUS;

// \struct SCSI_IO_CTX
// \param device file descriptor
// \param pcdb pointer to the built cdb to send
// \param cdb_len length of the perticular cdb being sent
// \param direction is it XFER_DATA_IN (from the drive) XFER_DATA_OUT (to the device)
// \param pdata pointer to the user data to be read/written 
// \param data_len length of the data to be read/written
// \param psense
// \param sense_sz
// \param timeout
// \param verbose will print some more debug info. 
// \param return_status return status of the scsi
// \param rtfrs will contain the ATA return tfrs, when requested.
typedef struct _SCSI_IO_CTX
{
    DEVICE * device;
    uint8_t *   pcdb;
    uint8_t     cdb_len;
    int8_t      direction;
    uint8_t *   pdata;
    uint32_t    data_len;
    uint8_t *   psense;
    uint32_t    sense_sz;
    uint32_t    timeout; // milli seconds
    uint8_t     verbose;
    SCSI_STATUS return_status;
    ATA_RETURN_TFRS rtfrs;
} SCSI_IO_CTX;

// \struct typedef struct _SCSI_SANITIZE_CMD_OPT
typedef struct _SCSI_SANITIZE_CMD_OPT
{
    uint8_t immed;
    uint8_t ause;
    uint8_t service_action;
    uint8_t * pData;
    uint8_t data_size;
} SCSI_SANITIZE_CMD_OPT;

// \struct typedef struct _SCSI_SECURITY_PROTOCOL_IN_OPT
typedef struct _SCSI_SECURITY_PROTOCOL_IN_OPT
{
    uint8_t security_protocol;
    uint8_t spSpecificMSB;
    uint8_t spSpecificLSB;
    uint8_t inc_512;
    uint32_t data_size;
    uint8_t * pData;    
} SCSI_SECURITY_PROTOCOL_IN_OPT;

// \struct typedef struct _SCSI_SECURITY_PROTOCOL_OUT_OPT
typedef struct _SCSI_SECURITY_PROTOCOL_OUT_OPT
{
    uint8_t security_protocol;
    uint8_t spSpecificMSB;
    uint8_t spSpecificLSB;
    uint8_t inc_512;
    uint32_t data_size;
    uint8_t * pData;    
} SCSI_SECURITY_PROTOCOL_OUT_OPT;

// \struct typedef struct _SCSI_REPORT_OP_CODE_CMD_OPT
typedef struct _SCSI_REPORT_OP_CODE_CMD_OPT
{
    uint8_t rctd; //RCTD bit
    uint8_t reporting_opts;
    uint8_t req_op_code;
    uint16_t req_service_action;
    uint32_t data_size;
    uint8_t * pData;    
} SCSI_REPORT_OP_CODE_CMD_OPT;

#define OPERATION_CODE          (0)
#define REQUEST_SENSE_DATA_SZ   (252)

#define SCSI_REQUEST_SENSE_DESC_BIT_SET     (0x01)

#define SCSI_SECURITY_PROTOCOL_IN           (0xA2)
#define SCSI_SECURITY_PROTOCOL_OUT          (0xB5)
#define SCSI_REPORT_SUPPORTED_OP_CODES      (0xA3)
#define SCSI_RCTD_BIT_SET                   (0x80)
#define SCSI_CTDP_BIT_SET                   (0x02)

#define SCSI_SANITIZE_CMD                   (0x48)
#define SCSI_SANITIZE_CMD_IMMED_BIT_SET     (0x80)
#define SCSI_SANITIZE_CMD_AUSE_BIT_SET      (0x20)
#define SCSI_SANITIZE_CMD_OVERWRITE_MODE    (0x01)
#define SCSI_SANITIZE_CMD_BLOCK_ERASE_MODE  (0x02)
#define SCSI_SANITIZE_CMD_CRYPTO_ERASE_MODE (0x03)
#define SCSI_SANITIZE_CMD_EXIT_FAIL_MODE    (0x1F)

#define SCSI_DOWNLOAD_MICROCODE              (0x04)
#define SCSI_DOWNLOAD_MICROCODE_SAVE         (0x05)
#define SCSI_DOWNLOAD_MICROCODE_OFFSET       (0x06)
#define SCSI_DOWNLOAD_MICROCODE_OFFSET_SAVE  (0x07)

