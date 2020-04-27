#ifndef KS_INITIALIZE_H
#define KS_INITIALIZE_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_initialize.h
// Date: 2014/02/28
// Author: Chris N Allo
//
// Description: Types and definitions for ks_initialize function.
//
//-----------------------------------------------------------------------------
// **
/*
 * Do NOT modify or remove this copyright and confidentiality notice.
 *
 * Copyright 2014 Seagate Technology LLC.
 *
 * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 * and may be covered under one or more Non-Disclosure Agreements. All or
 * portions are also trade secret. Any use, modification, duplication,
 * derivation, distribution or disclosure of this code, for any reason,
 * not expressly authorized is prohibited. All other rights are expressly
 * reserved by Seagate Technology LLC.
 *
 */
//


/**
 * @file ks_initialize.h
 *
 * Function that sets up a bare drive for Drive-level Security (SED) for Kinetic.
 * Implementation Notes:
 * This is used to initialize a "fresh drive".
 *
 */

#include "ks_globals.h"


/**
 * Setup SED bands and take ownership of key SED credentials.
 *
 * Bands are to be allocated starting at *band_start_LBA* and sized
 * according to the values in *band_sizes*. Bands are allocated
 * back to back with no gaps between.
 *
 * Implementation Notes:
 * * PIN values shall be generated by invoking TCG Random method.
 * * PIN values shall be stored on the drive in the TCG DataStore table.
 * * All pins will be stored in the data store table with a NULL passwd.
 *
 * @param[in] band_sizes the size of each band to be allocated.
 * @param[in] band_sizes_count number of valid *band_sizes* values.
 * @param[in] band_start_LBA the LBA of the first band in *band_sizes*.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */

extern ks_status ks_initialize(band_size_t *band_sizes, uint8_t band_sizes_count, band_start_t band_start_LBA);

#ifdef __cplusplus
}
#endif
#endif