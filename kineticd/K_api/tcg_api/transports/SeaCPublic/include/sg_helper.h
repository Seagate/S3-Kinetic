//
// sg_helper.h
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

#ifndef SG_HELPER_H
#define SG_HELPER_H

// \file sg_helper.h
// \brief Defines the constants structures and function headers to help parse scsi drives.

#include <stdint.h> // for uint*_t types
#include <stdlib.h> // for size_t types
#include <stdio.h>  // for printf
#include <string.h> // For memset
// \todo Figure out which scsi.h & sg.h should we be including kernel specific or in /usr/..../include
#include <scsi/sg.h>
#include <scsi/scsi.h>
#include "scsi_helper.h"
#include "sat_helper.h"
#include "common.h"
        
// \fn get_device(char * filename)
// \brief Given a device name (e.g. /dev/sg0) returns the device descriptor
// \details Function opens the device & then sends a SG_GET_VERSION_NUM
//          if everything goes well, it returns a sg file descriptor & fills out other info.      
// \todo Add a flags param to allow user to open with O_RDWR, O_RDONLY etc. 
// \param filename name of the device to open
// \returns DEVICE device structure
//int get_device(char * filename, DEVICE);

// \fn decipher_masked_status
// \brief Function to figure out what the masked_status means
// \param masked_status from the sg structure
void decipher_masked_status(unsigned char masked_status);

// \fn send_io(scsi_io_ctx * scsi_io_ctx)
// \brief Function to send a SG_IO ioctl 
// \param SCSI_STATUS
int send_io(SCSI_IO_CTX * scsi_io_ctx);

#endif
