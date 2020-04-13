#ifndef KS_UTILITIES_H
#define KS_UTILITIES_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_utilities.h
// Date: 2014/03/04
// Author: Chris N Allo
//
// Description: utility functions for all ks_routines function.
//
//-----------------------------------------------------------------------------
// **
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

/* From EAPI functions */
#include "parameters.h"


/**
 * @file ks_utilities.h
 *
 * Utility functions global to libksapi
 *
 *
 * Implementation Notes:
 * * local helper functions
 *
 */

extern ks_status Load_DataStoreStructure(SED_datastore_struct *DataStoreData, tcgByteValue *RawDataStore );

extern ks_status Load_DataStoreString(tcgByteValue *RawDataStore, SED_datastore_struct *DataStoreData); 

#ifdef __cplusplus
}
#endif
#endif
