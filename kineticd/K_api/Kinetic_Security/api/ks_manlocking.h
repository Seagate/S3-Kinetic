#ifndef KS_MANLOCKING_H
#define KS_MANLOCKING_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_manlocking.h
// Date: 2014/03/28
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
 * @file ks_manlocking.h
 * 
 * Query and manipulate the locks on the drive's bands.
 *
 * NOTE: While configuration/layout of the bands is done once (by *ks_initialize*), bands can be locked or
 *       unlocked at anytime. For a band to be locked, the band's lock must be enabled and the lock must be
 *       set.
 * NOTE: Enabling the locking capability does not immediately lock the device. It does, however, set up the
 *       drive so that the drive will automatically lock on the next power cycle regardless of the current
 *       lock status. To lock the band, the band lock must be enabled and then either another call made to
 *       lock the band, or a power-cycle (or both).
 */

typedef enum locking_operation {
    locking_status = 1,
    locking_enable,
    locking_disable,
    locking_lock,
    locking_unlock,
    lock_on_powercycle_enable,
    lock_on_powercycle_disable
} locking_operation;

#ifdef old
typedef enum Lock_Status {
    Get_Lock_Status = 0x01,   
    Read_Write_Lock_Enable = 0x02,
    Read_Write_Lock = 0x04,
    Lock_On_Powercycle = 0x08
} Lock_Status;
#endif

/**
 * Manage locking for a band.
 *
 * @param[in] band_number to operate on.
 * @param[in] operation to perform.
 * @param[in] BAND_Password needed for all operations except status (for status it is ignored).
 * @param[out] lock_status pointed to value is set to 0 if the band is unlocked, 1 if the band is locked.
 *             (ignored for non-status operations).
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 * NOTE: If there is an error with the status operation, the value pointed to by *lock_status* may or
 *       may not be changed.
 *
 */
extern ks_status ks_manlocking(int band_number,
                               locking_operation operation,
                               char * BAND_Password,
                               uint8_t * lock_status);

#ifdef __cplusplus
}
#endif
#endif
