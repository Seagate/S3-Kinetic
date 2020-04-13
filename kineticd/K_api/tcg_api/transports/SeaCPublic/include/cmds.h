//
// cmds.h   Generic defines & prototypes 
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
#pragma once

#include <stdio.h>
#include <time.h>
#include <stdint.h>

#include "common.h"
#include "ata_helper.h"
#include "sat_helper.h"
#include "scsi_helper.h"

// \todo Revisit drive sector size for 4k sector drives
#define DRIVE_SEC_SIZE         (512) 

// \file seagat_pub_cmds.h
// \brief Defines the structures and function headers for commands. 

#define ATA_IDENTIFY_SANITIZE_INDEX         (59)
// If bit 12 of word 59 is set to one the device supports the Sanitize Device feature set.
#define ATA_IDENTIFY_SANITIZE_SUPPORTED     (0x1000)
// If bit 13 of word 59 is set to one, then the device supports the 
// Sanitize Device feature set CRYPTO SCRAMBLE EXT command
#define ATA_IDENTIFY_CRYPTO_SUPPORTED       (0x2000)
// If bit 14 of word 59 is set to one, then the device supports the 
// Sanitize Device feature set OVERWRITE EXT command
#define ATA_IDENTIFY_OVERWRITE_SUPPORTED    (0x4000)
// If bit 15 of word 59 is set to one, then the device supports the 
// Sanitize Device feature set BLOCK ERASE EXT command
#define ATA_IDENTIFY_BLOCK_ERASE_SUPPORTED  (0x8000)

#define ATA_SANITIZE_CMD                    (0xB4)
#define ATA_SANITIZE_CRYPTO_FEAT            (0x11)                      
#define ATA_SANITIZE_CRYPTO_LBA             (0x43727970)
#define ATA_SANITIZE_BLOCK_ERASE_FEAT       (0x12)
#define ATA_SANITIZE_BLOCK_ERASE_LBA        (0x426B4572)
#define ATA_SANITIZE_OVERWRITE_FEAT         (0x14)
#define ATA_SANITIZE_OVERWRITE_LBA          (0x4F57)
#define ATA_SANITIZE_FREEZE_LOCK_FEAT       (0x20)
#define ATA_SANITIZE_FREEZE_LOCK_LBA        (0x46724C6B)
#define ATA_SANITIZE_STATUS_FEAT            (0x00)

#define ATA_SANITIZE_CLEAR_OPR_FAILED       (0x01)
#define ATA_SANITIZE_FAILURE_MODE_BIT_SET   (0x10)
#define ATA_SANITIZE_INVERT_PAT_BIT_SET     (0x80)

// \struct typedef struct _ATA_SANITIZE_CMD_OPT
typedef struct _ATA_SANITIZE_CMD_OPT
{
    uint8_t invert_pattern; // Only for SANITIZE OVERWRITE mode
    uint8_t failure_mode;
    uint8_t clear_failure; // Only For SANITIZE STATUS
    uint64_t lba;          // Only for custom commands
    uint8_t * pData;
    uint8_t data_size;     // CAUTION: This is by intention uint8_t .
} ATA_SANITIZE_CMD_OPT;

// \fn get_sanitize_device_feature(DEVICE device)
// \brief Function to find out which of the sanitize feature options are supported, if any
// \param device file descriptor
// \return 0 == success, < 0 something went wrong 
int get_sanitize_device_feature(DEVICE * device);

// \fn ata_pt_cmd(DEVICE device, ATA_PT_CMD_OPTS cmd_opts)
// \brief Function to send a ATA Spec cmd as a passthrough
// \param cmd_opts ata command options
// \param device file descriptor
// \return 0 == success, < 0 something went wrong (-2 unsupported opts)
int ata_pt_cmd(DEVICE * device, ATA_PT_CMD_OPTS * ata_cmd_opts);
 
// \fn ata_sanitize_cmd(DEVICE device, ATA_SANITIZE_CMD_OPT cmd_opts)
// \brief Function to send a ATA Sanitize command
// \param device file descriptor
// \param ATA_SANITIZE_CMD_OPT struct containing the sanitize command options.  
int ata_sanitize_cmd(DEVICE * device, ATA_SANITIZE_CMD_OPT cmd_opts);

// \fn ata_get_sanitize_status(DEVICE device, uint8_t clear_failure, ATA_SANITIZE_STATUS status)
// \brief Function to send request scsi sense data
// \param device file descriptor
// \param ATA_SANITIZE_STATUS status return status to be filled in. 
int ata_get_sanitize_status(DEVICE * device, ATA_SANITIZE_CMD_OPT cmd_opts);

// \fn get_ata_sanitize_device_feature(DEVICE * device, DRIVE_INFO * info)
// \brief Function to get the SANITIZE Device Features from an ATA drive
int get_ata_sanitize_device_feature(DEVICE * device, DRIVE_INFO * info);

// \fn scsi_sanitize_cmd(DEVICE device, SCSI_SANITIZE_CMD_OPT cmd_opts)
// \brief Function to send a SCSI Sanitize command
// \param device file descriptor
// \param SCSI_SANITIZE_CMD_OPT struct containing the sanitize command options.  
int scsi_sanitize_cmd(DEVICE * device, SCSI_SANITIZE_CMD_OPT cmd_opts);

// \fn scsi_security_protocol_in(DEVICE device, SCSI_SECURITY_PROTOCOL_IN_OPT cmd_opts)
// \brief Function to send SECURITY PROTOCOL IN command
// \param device file descriptor
// \parm  cmd_opts command options to security protocol in cmd opts. 
int scsi_security_protocol_in(DEVICE * device, SCSI_SECURITY_PROTOCOL_IN_OPT cmd_opts);

// \fn scsi_security_protocol_out(DEVICE device, SCSI_SECURITY_PROTOCOL_OUT_OPT cmd_opts)
// \brief Function to send SECURITY PROTOCOL OUT command
// \param device file descriptor
// \parm  cmd_opts command options to security protocol in cmd opts. 
int scsi_security_protocol_out(DEVICE * device, SCSI_SECURITY_PROTOCOL_OUT_OPT cmd_opts);

// \fn scsi_request_sense_cmd(DEVICE device, uint8_t * pdata, uint8_t data_size)
// \brief Function to send request scsi sense data
// \param device file descriptor
// \param uint8_t desc_bit Fixed or Descripter format. 
// \param pdata data in the .  
// \param data_size size of the data requested. 252 should be the default
int scsi_request_sense_cmd(DEVICE * device, uint8_t desc_bit, uint8_t * pdata, uint8_t data_size);

// \fn scsi_report_supported_operation_codes(DEVICE device, SCSI_REPORT_OP_CODE_CMD_OPT cmd_opts)
// \brief Function to send a ATA Spec Secure Erase to the linux sg device
// \param device file descriptor
// \parm  cmd_opts command options to report scsi operation codes. 
int scsi_report_supported_operation_codes(DEVICE * device, SCSI_REPORT_OP_CODE_CMD_OPT cmd_opts);

// \fn ata_secure_erase(DEVICE device)
// \brief Function to send a ATA Spec Secure Erase to the linux sg device
// \param device file descriptor
int ata_secure_erase(DEVICE * device);

// \fn run_DST(DEVICE * device, char * pserial_number, int isSATA, int DSTType)
// \brief Function to send a ATA Spec DST or SCSI spec DST to linux sg device
// \param device file descriptor
// \param pserial_number serial number of the device for logging purposes
// \param isSATA set to one for ATA devices, 0 for scsi devices. Used to select a scsi DST or ATA DST
// \param DSTType 1 = short DST, 2 = Long DST
int run_DST(DEVICE * device, char * pserial_number, int isSATA, int DSTType);

// \fn run_secureerase(DEVICE * device, char * pserial_number, int isSATA, int enhanced)
// \brief Function to send a ATA Spec Secure Erase or SCSI spec Secure Erase to the linux sg device
// \param device file descriptor
// \param pserial_number serial number of the device for logging purposes
// \param isSATA set to one for ATA devices, 0 for scsi devices. Used to select a scsi secure erase or ATA secure erase
// \param enhanced set to a value greater than 1 to run an ehnaced secure erase
int run_secureerase(DEVICE * device, char * pserial_number, int isSATA, int enhanced);

// \fn run_SMART_Check(DEVICE * device, char * pserial_number)
// \brief Function to perform a SMART check on an ATA device
// \param device file descriptor
// \param pserial_number serial number of the device for logging purposes
int run_SMART_Check(DEVICE * device, char * pserial_number);

// \fn getATAIdentifyInfo(DEVICE * device, int *isSSD)
// \brief Function to send a ATA Spec identify to a linux sg device
// \param device file descriptor
// \param isSSD out parameter that returns if the device is an SSD drive
int get_ATA_identify_info(DEVICE * device, int *isSSD,uint64_t *maxLBA);

// \fn ata_read_log_ext_cmd(DEVICE * device, uint8_t log_address, uint16_t page_number, uint8_t * pData, uint32_t data_size);
// \brief Function to send a ATA Spec read_log_ext
// \param device file descriptor
// \param uint8_t log_address to be read as described in A.2 of ATA Spec. 
// \param uint16_t page_number specifies the first log page to be read from the log_address
// \param uint8_t * pData data buffer to be filled
// \param uint32_t data_size size of the buffer to be filled. 
// \return 0 == success, < 0 something went wrong
int ata_read_log_ext_cmd(DEVICE * device, uint8_t log_address, uint16_t page_number, uint8_t * pData, uint32_t data_size);

// \fn ata_SMART_cmd(DEVICE * device, uint8_t feature, uint8_t * pData, uint32_t data_size);
// \brief Function to send a ATA Spec SMART commands other than SMART R/W Log commands
// \param device file descriptor
// \param uint8_t feature feature register
// \param uint8_t * pData data buffer to be filled
// \param uint32_t data_size size of the buffer to be filled. 
// \return 0 == success, < 0 something went wrong 
int ata_SMART_cmd(DEVICE * device, uint8_t feature, uint8_t * pData, uint32_t data_size);


