#ifndef KS_SETUP_H
#define KS_SETUP_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_setup.h
// Date: 2014/03/05
// Author: Chris N Allo
//
// Description: Types and definitions for ks_setup function.
//
//-----------------------------------------------------------------------------
//
// * Do NOT modify or remove this copyright and confidentiality notice.
 // *
 // * Copyright 2014 Seagate Technology LLC.
 // *
 // * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 // * and may be covered under one or more Non-Disclosure Agreements. All or
 // * portions are also trade secret. Any use, modification, duplication,
 // * derivation, distribution or disclosure of this code, for any reason,
 // * not expressly authorized is prohibited. All other rights are expressly
 // * reserved by Seagate Technology LLC.
 // *
//
// **


/**
 * @file ks_setup.h
 *
 * Functions that provide global initialization for the Security (SED) for Kinetic.
 * Implementation Notes:
 * @param[in] Device_ID name of the device we are to be talking to
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 *
 */

#include "ks_globals.h"

/**
 * Setup parameters global to all Kinetic Security API calls
 *
 */

extern ks_status ks_setup(char *Device_ID);

#ifdef __cplusplus
}
#endif
#endif
