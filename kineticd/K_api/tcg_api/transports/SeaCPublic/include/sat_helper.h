//
// sat_helper.h
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

// \file sat_helper.h
// \brief Defines the constants structures to help with SAT implementation


#pragma once

#include <stdint.h>
#include "ata_helper.h"

#define SAT_VPD_CMD_CODE_IDX	(56)

#define SAT_DESCRIPTOR_CODE     (0x09)
#define SAT_ADDT_DESC_LEN       (0x0C)
#define SAT_DESC_LEN            (14)    // 0x0E

#define SAT_DESC_EXTEND_BIT     (0x01)

#define SAT_EXTEND_BIT_SET  (0x01)
//These defines are 1 shifted for byte 1 of SAT cdb
#define SAT_ATA_HW_RESET    (0x00)
#define SAT_ATA_SW_RESET    (0x01 << 1)
#define SAT_NON_DATA        (0x03 << 1)
#define SAT_PIO_DATA_IN     (0x04 << 1)
#define SAT_PIO_DATA_OUT    (0x05 << 1)
#define SAT_DMA             (0x06 << 1)
#define SAT_DMA_QUEUED      (0x07 << 1)
#define SAT_EXE_DEV_DIAG    (0x08 << 1)
#define SAT_NODATA_RESET    (0x09 << 1)
#define SAT_UDMA_DATA_IN    (0x0A << 1)
#define SAT_UDMA_DATA_OUT   (0x0B << 1)
#define SAT_FPDMA           (0x0C << 1)
#define SAT_RET_RESP_INFO   (0x0F << 1)

// SAT Spec Byte 2 specifics 

// T_LENGTH Field Values based on SAT spec Table 139 — T_LENGTH field
#define SAT_T_LEN_XFER_NO_DATA  (0x00) // No data is transferred
#define SAT_T_LEN_XFER_FET      (0x01) // The transfer length is an unsigned integer specified in the FEATURES (7:0) field.
#define SAT_T_LEN_XFER_SEC_CNT  (0x02) // The transfer length is an unsigned integer specified in the SECTOR_COUNT (7:0) field.
#define SAT_T_LEN_XFER_TPSIU    (0x03) // The transfer length is an unsigned integer specified in the TPSIU

#define SAT_BYTE_BLOCK_BIT_SET  (0x04)
// T_DIR 
#define SAT_T_DIR_DATA_OUT      (0x00)
#define SAT_T_DIR_DATA_IN       (0x08) // or (0x01 << 3) i.e. bit 3 is set) 
#define SAT_T_TYPE_BIT_SET      (0x10)
#define SAT_CK_COND_BIT_SET     (0x20)

// scsi.h rebrand. Values for T10/04-262r7 
#define	SAT_ATA_16		      0x85	/* 16-byte pass-thru */
#define	SAT_ATA_12		      0xa1	/* 12-byte pass-thru */

// \struct typedef struct _SAT_ATA_PASS_THROUGH
// \brief structure to contain the ATA Pass Through structure
typedef struct _SAT_ATA_PASS_THROUGH
{
    uint8_t byte_0_op_code;
    uint8_t byte_1_ext_proto_mc; 
    uint8_t byte_2_bit_settings; // 
    uint8_t byte_3_feature_ext;
    uint8_t byte_4_feature;
    uint8_t byte_5_sc_ext;
    uint8_t byte_6_sc;
    uint8_t byte_7_lba_low_ext;
    uint8_t byte_8_lba_low;         // Sector Number
    uint8_t byte_9_lba_mid_ext;
    uint8_t byte_10_lba_mid;        // Cyl Low
    uint8_t byte_11_lba_high_ext;
    uint8_t byte_12_lba_high;       // Cyl High
    uint8_t byte_13_device;
    uint8_t byte_14_cmd;
    uint8_t byte_15_ctrl; 
} SAT_ATA_PASS_THROUGH, *P_SAT_ATA_PASS_THROUGH;

// \struct typedef struct _SAT_ATA_RETURN_DESC
// \brief structure that contains SAT specific ATA Return Descriptor 
typedef struct _SAT_ATA_RETURN_DESC
{
    uint8_t desc_code; // Check these to make sure everyting is valid. 
    uint8_t desc_len; 
    uint8_t resv_and_extend_bit;
    uint8_t error_byte;
    uint8_t sector_cnt_ext;
    uint8_t sector_cnt;
    uint8_t lba_low_ext;
    uint8_t lba_low;
    uint8_t lba_mid_ext;
    uint8_t lba_mid;
    uint8_t lba_high_ext;
    uint8_t lba_high;
    uint8_t device;
    uint8_t status_byte;
} SAT_ATA_RETURN_DESC;

