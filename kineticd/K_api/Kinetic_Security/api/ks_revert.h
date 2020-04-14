#ifndef KS_REVERT_H
#define KS_REVERT_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_revert.h
// Date: 2014/03/26
// Author: Chris N Allo
//
// Description: Types and definitions for ks_erase function.
//
//-----------------------------------------------------------------------------
// // **
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
 * @file ks_revert.h
 *
 */

/**
 * Implementation Notes:
 * * The Drive PSID is printed on the label and is not readable over the interface.
 * Note: As of April 2014, invoking the function will completely revert the drive back to a factory state,
 * wiping all existing content in all bands.
 *
 * @param[in] PSID a null terminated C-String of the drive's PSID.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */

extern ks_status ks_revert(char * PSID);

#ifdef __cplusplus
}
#endif
#endif
