//-----------------------------------------------------------------------------
//
// Header: ks_debug.c
// Date: 2014/03/14
// Author: Chris N Allo
//
// Description: Debug handler for EA return status
//
//-----------------------------------------------------------------------------

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
/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local functions */
#include "ks_globals.h"
#include "ks_debug.h"

/* From EAPI functions */



/**
 * @file ks_debug.c
 *
 * THIS IS A MACRO DEFINED IN ks_debug.h
 *
 * Output debug information to console and log file depending on debug level.
 * If debug level is > 0 data is output through printf or method coded in this call
 * 
 * Next step will be the action type specified by user
 * This function will output information only if the debug level > 0 and the status != 0
 * If action type is KS_INFO this function will only print Info string even if Ret_Status != 0
 *
 * Implementation Notes:
 *
 * @param[in] Enterprise API status returned.
 * @param[in] Action type to perform
 * @param[in] Informational String  (info or lastTcgStatusDescription() )
 * @return SUCCESS if all goes well, otherwise nothing
 *
 */

/******************************************************************************************************/

/* Ks_debug will take a ret_status, action_type, and info string */

/* void ks_debug(status Ret_Status, ks_action_type Action, char * Info_String); */









