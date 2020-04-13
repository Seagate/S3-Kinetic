//
// sat_helper_func.h
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
// \brief Defines the function headers to help with SAT implementation

#pragma once

//#ifndef SAT_HELPER_FUNC_H
//#define SAT_HELPER_FUNC_H

#include <stdint.h>
#include "sat_helper.h"

// If the SAT_ATA_RETURN_DESC.desc_code is non-zero, we have good data.
SAT_ATA_RETURN_DESC get_ata_TFRs_from_sense(uint8_t * psense);

//#endif
