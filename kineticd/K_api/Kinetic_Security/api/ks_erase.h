#ifndef KS_ERASE_H
#define KS_ERASE_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_erase.h
// Date: 2014/03/04
// Author: Chris N Allo
//
// Description: Types and definitions for ks_erase function.
//
//-----------------------------------------------------------------------------
// **
// *
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

#include "ks_globals.h"

/**
 * @file ks_erase.h
 *
 * Erase specified SED band(s).
 * NOTE: As a side effect of how Erase works, the Locking PIN for any erased band will be reset to NULL.
 *
 *
 * Implementation Notes:
 * * If setpin has been used to set a Kinetic Erase PIN, that same PIN must be passed in here.
 * * If no setpin has been used to set a Kinetic Erase PIN, NULL must be passed in here.
 *
 * @param[in] Band_Numbers array holding the band numbers to be erased.
 * @param[in] Band_Count number of Band_Numbers values to process.
 * @param[in] Kinetic_Erase_PIN (optional) Kinetic Erase PIN, if set, NULL otherwise.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */

extern ks_status ks_erase(int *Band_Numbers, int Band_Count, char * Kinetic_Erase_PIN);
#ifdef __cplusplus
}
#endif
#endif
