//
// scsi_helper_func.h   Header for helper functions for scsi
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

// \file sat_helper_func.h
// \brief Defines the function headers to help with SCSI implementation

#pragma once

//#ifndef SCSI_HELPER_FUNC_H
//#define SCSI_HELPER_FUNC_H

#include <stdint.h>
#include "common.h"
#include "scsi_helper.h"

// \fn remove_spaces(char* source)
// \brief Function to remove spaces in a buffer
// \param char * buffer to work on
void remove_spaces(char* source);

void print_sense_buffer(uint8_t * pbuf, int size);

// \fn decipher_and_print_acq_ascq 
// \brief Incomplete list of figuring out what ACQ/ASCQ mean
// \param acq  SCSI ACQ value
// \param ascq SCSI ASCQ value
void decipher_and_print_acq_ascq(uint8_t acq, uint8_t ascq);

void decipher_and_print_sense_key(uint8_t sense_key);

void get_acq_ascq(uint8_t * pbuf, uint8_t * acq, uint8_t * ascq);

uint8_t get_sense_key(uint8_t * pbuf);

// \fn send_inq(DEVICE device, uint8_t * pdata, uint32_t data_len, uint8_t evpd)
// \brief Sends an INQUIRY (0x12) command to the scsi device & returns data.
// \details SCSI command details: http://en.wikipedia.org/wiki/SCSI_Inquiry_Command
// \param device pointer to device structure
// \param pdata data to the user buffer to put in the data
// \param data_len length of the user buffer, must be at least INQ_DATA_LENGTH
// \param page_code VPD Page code
// \param evpd set or not set the evpd bit [Note: User has to set it for VPD pages, see SPC]
// \returns a negative number if fails
int send_inq(DEVICE * device, uint8_t * pdata, uint32_t data_len, uint8_t page_code, uint8_t evpd);

// \fn send_cdb(DEVICE * device, uint8_t * pdata, uint32_t data_len , uint8_t * cdb, uint8_t cdb_len, uint32_t data_direction)
// \brief Sends a cdb to the scsi device & returns data.
// \details SCSI command details
// \param device         - pointer to device structure
// \param pdata          - data to the user buffer to put in the data
// \param data_len       - length of the user buffer, must be at least INQ_DATA_LENGTH
// \param cdb            - pointer to the cdb the user has set up and wishes to send to the scsi device
// \param cdb_len        - the length of the cdb the user wishes to send
// \param data_direction - the data direction of the command the user is sending
// \returns a negative number if fails
int send_cdb(DEVICE * device, uint8_t * pdata, uint32_t data_len , uint8_t * cdb, uint8_t cdb_len, uint8_t data_direction);

// \fn fill_in_device_info(DEVICE device)
// \brief Sends a set of INQUIRY commands & fills in the device information 
// \param device device struture
// \returns a negative number if fails
int fill_in_device_info(DEVICE * device);

// \fn copy_inquiry_data(unsigned char * pbuf, DRIVE_INFO * info)
// \brief copy in the necessary data to our struct from INQ data. 
void copy_inquiry_data(unsigned char * pbuf, DRIVE_INFO * info);

// \fn copy_serial_number(unsigned char * pbuf, DRIVE_INFO * info)
// \brief copy the serial number off of 0x80 VPD page data. 
void copy_serial_number(unsigned char * pbuf, DRIVE_INFO * info);

// /brief check if the device is sat compliant 
// \returns 1 if it is . 
int is_device_sat_compliant( DEVICE * device, DRIVE_INFO * info);

// \fn get_scsi_sanitize_device_feature(DEVICE * device, DRIVE_INFO * info)
// \brief Gets SCSI SANITIZE Device feature set. 
// \param device pointer to DEVICE structure
// \param info pointer to DRIVE_INFO structure
int get_scsi_sanitize_device_feature(DEVICE * device, DRIVE_INFO * info);

//#endif
