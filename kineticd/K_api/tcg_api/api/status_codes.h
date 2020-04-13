#ifndef STATUS_CODES_H
#define STATUS_CODES_H

/*
 * Do NOT modify or remove this copyright and confidentiality notice.
 *
 * Copyright 2013 - 2014 Seagate Technology LLC.
 *
 * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 * and may be covered under one or more Non-Disclosure Agreements. All or
 * portions are also trade secret. Any use, modification, duplication,
 * derivation, distribution or disclosure of this code, for any reason,
 * not expressly authorized is prohibited. All other rights are expressly
 * reserved by Seagate Technology LLC.
 *
 */

#include <stdint.h>
typedef int status;

/** aka errno for TCG stack, only set on NON SUCCESS */
extern status lastTcgStatus(void);

/** helper to set lastTcgStatus (and return it) */
extern status setLastTcgStatus(status);

/** additional info for lastTcgStatus */
extern char *lastTcgStatusDescription(void);

/** Internal use only. Used in convenience macros, hence
 *  defined here to keep the compiler happy.
 */
extern status _status_code(status value, char *extraMsg);

/** Internal use only. Used in convenience macros, hence
 *  defined here to keep the compiler happy.
 */
extern status _method_status_code(uint64_t value, char *extraMsg);

#define hasError(statusValue) (statusValue != SUCCESS)

/** Helper macro that will return if a non-success status is returned by the
 *  given expression. Also sets lastTcgStatus.
 */
#define TA_CHECK(x) {if (setLastTcgStatus(x)) { return lastTcgStatus(); } }

/* Status code SUCCESS is the only one to be used directly in the code,
 * the others are for use by the helper macros.
 */
#define SUCCESS 0
#define _ERROR 1
#define _API_ERROR 2
#define _INTERNAL_ERROR 3
#define _TCG_METHOD_ERROR 4

/** Used to indicate an error of some (generic) kind has occurred */
#define ERROR_(x)  _status_code(_ERROR, x)

/** a status value indicating an error in the use of the API.
 *  API_ERRORs should be returned before any drive traffic is initiated.
 */
#define API_ERROR(msg)  _status_code(_API_ERROR, msg)

/** a status value indicating an error in the internal processing
 *  of the various libraries. These should not occur, but are defined
 *  in case they are needed.
 */
#define INTERNAL_ERROR(msg) _status_code(_INTERNAL_ERROR, msg)

/** a status value indicating that a TCG Method call failed when it was
 *  expected to succeed.
 */
#define METHOD_ERROR(tcg_status_code, msg) _method_status_code(tcg_status_code, msg)
#endif
