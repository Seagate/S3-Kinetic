#ifndef KS_SETPIN_H
#define KS_SETPIN_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_setpin.h
// Date: 2014/03/18
// Author: Chris N Allo
//
// Description: Types and definitions for ks_setpin function.
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


/**
 * @file ks_setpin.h
 *
 * Implementation:
 * Setting Kinetic Locking/Erase PIN, changing PIN, or resetting PIN will 
 * secure the drive's PIN values by generating a random number for the drive
 * PIN and then wrap that PIN with the Kinetic PIN using NIST SP800-132 key wrap algorithm.
 *
 * Notes:
 * * If the *old_PIN* value is incorrect, an error will be returned.
 * * If *new_PIN* may be null to indicate that there is no corresponding Kinetic PIN,
 *   however the corresponding PIN on the drive will still be encrypted.
 * * As of April 2014, the BandMaster0 and SID Drive PINs are not wrapped by a Kinetic PIN.
 * 
 * @param[in] new_Pin
 * @param[in] old_Pin
 * @param[in] which_Pin
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 *
 */

typedef enum {
  KS_ERASE_PIN = 0,
  KS_LOCKING_PIN
} ks_pin_types; 

extern ks_status ks_setpin(char *new_PIN, char *old_PIN, ks_pin_types which_Pin);

#ifdef __cplusplus
}
#endif
#endif
