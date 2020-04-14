//
// win_helper.h
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

// \file win_helper.h
// \brief Defines the constants structures and function headers to help parse scsi drives.

#include <stdint.h> // for uint*_t types
#include <stdlib.h> // for size_t types
#include <stdio.h>  // for printf
#include <string.h> // For memset
#include "scsi_helper.h"
#include "sat_helper.h"
#include "common.h"

/**
 * @brief Scsi passthrough context structure.
 */
typedef struct _tSPTIoContext {
   SCSI_PASS_THROUGH_DIRECT Sptd;
   uint8_t SenseBuffer[SPC3_SENSE_LEN];// If we do auto-sense, we need to allocate 252 bytes, according to SPC-3.   
   DWORD SptBufLen;
} tSPTIoContext;

// \fn send_io(scsi_io_ctx * scsi_io_ctx)
// \brief Function to send a ioctl after converting it from the SCSI_IO_CTX to OS tSPTIoContext
// \param SCSI_IO_CTX
int send_io(SCSI_IO_CTX * scsi_io_ctx);

// \fn print_bus_type (BYTE type)
// \nbrief Funtion to print in human readable format the BusType of a device
// \param BYTE which is STORAGE_BUS_TYPE windows enum
void print_bus_type (BYTE type);

