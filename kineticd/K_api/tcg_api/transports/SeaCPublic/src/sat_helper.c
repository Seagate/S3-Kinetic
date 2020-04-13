
//
// sat_helper.c   Implementation for helper functions for SAT (SCSI-to-ATA Translation) layer
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

#include <stdio.h>
#ifdef __linux__
#include <sys/types.h>
#endif
#include <string.h>
#include "scsi_helper_func.h"
#include "sat_helper_func.h"

uint8_t is_valid_sat_desc(uint8_t * psense)
{
    uint8_t ret = 0;
    uint8_t format = psense[0]& 0x7F; //Stripping the last bit.

    if (psense[SCSI_SENSE_ADDT_LEN_INDEX] > 0)
    {
        switch(format)
        {
            case SCSI_SENSE_CUR_INFO_DESC:
            case SCSI_SENSE_DEFER_ERR_DESC:
                if ( 
                    (psense[SCSI_DESC_FORMAT_DESC_INDEX] == SAT_DESCRIPTOR_CODE) 
                    && (psense[SCSI_DESC_FORMAT_DESC_LEN] == SAT_ADDT_DESC_LEN)
                    )
                {
                    ret = 1;
                }
                break;
            default:
                break;
        }
    }

    return ret;
}

// If the SAT_ATA_RETURN_DESC.desc_code is non-zero, we have good data.
SAT_ATA_RETURN_DESC get_ata_TFRs_from_sense(uint8_t * psense)
{
    SAT_ATA_RETURN_DESC ata_ret_desc = { 0 };
    uint8_t format = psense[0]& 0x7F; //Stripping the last bit.

    if(is_valid_sat_desc(psense))
    {
        memcpy(&ata_ret_desc,&psense[SCSI_DESC_FORMAT_DESC_INDEX],sizeof(SAT_ATA_RETURN_DESC));
    }
    else
    {
        //SAT implementation on certain HBAs is not fully tested, so the fixed format
        //sometimes cause issues.
        if ( (format == SCSI_SENSE_CUR_INFO_FIXED) || (format == SCSI_SENSE_DEFER_ERR_FIXED) )
        {
            ata_ret_desc.desc_code = format; // Make sure this is documented. 

            if (psense[0] & SCSI_SENSE_INFO_VALID_BIT_SET)
            {
                // As per SAT spec Table 146
                ata_ret_desc.error_byte = psense[SCSI_SENSE_INFO_FIELD_MSB_INDEX];
                ata_ret_desc.status_byte = psense[SCSI_SENSE_INFO_FIELD_MSB_INDEX + 1];
                ata_ret_desc.device = psense[SCSI_SENSE_INFO_FIELD_MSB_INDEX + 2];
                ata_ret_desc.sector_cnt = psense[SCSI_SENSE_INFO_FIELD_MSB_INDEX + 3];

                // Don't know if checking the extend bit makes a diference here. 
                // Look at Table 147 SAT spec. 
                ata_ret_desc.lba_high = psense[SCSI_FIXED_FORMAT_CMD_INFO_INDEX + 1];
                ata_ret_desc.lba_mid = psense[SCSI_FIXED_FORMAT_CMD_INFO_INDEX + 2];
                ata_ret_desc.lba_low = psense[SCSI_FIXED_FORMAT_CMD_INFO_INDEX + 3];
            }
        }
    }
    return ata_ret_desc;
}
