#ifndef UTILITIES_H
#define UTILITIES_H

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

/*! @file
 *  Misc. utilities, converstion functions, etc.
 */

#include <string.h>

/** UIDs are exactly 8 bytes long, but it is more convenient to represent
 *  them as pointers.
 */
typedef unsigned char *uid;


/** Map bandNumbers to UIDs.
 *  @param[in] bandNumber whose UID is desired.
 *  @return If the bandNumber is out of range (to large or small), then NULL is returned.
 *  @return points to fixed static memory and should not be changed.
 *  @return value is only good until the next call to this function.
 */
uid bandNumberToUID(int bandNumber);

/** Map bandNumbers to UIDs of associated K_AES_256 objects.
 *  @param[in] bandNumber whose K_AES_256 object UID is desired.
 *  @return If the bandNumber is out of range (to large or small), then NULL is returned.
 *  @return points to fixed static memory and should not be changed.
 *  @return value is only good until the next call to this function.
 */
uid bandNumberToMEKUID(int bandNumber);

/** Map bandNumbers to controlling Authority names.
 *  @param[in] bandNumber whose controlling Authority is desired.
 *  @return If the bandNumber is out of range (to large or small), then NULL is returned.
 *  @return The return value points into fixed static memory and should not be changed!
 *  @return The return value is only good until the next call to this function.
 */
char *bandNumberToAuthority(int bandNumber);

/** Map a Port name to the name of the Authority controlling it.
 *  @param[in] portName whose controlling Authority name is desired.
 *  @return If portName is unknown, then NULL is returned.
 *  @return The return value points into fixed static memory and should not be changed!
 *  @return The return value is only good until the next call to this function.
 */
char *portNameToAuthority(char *portName);

/** Map a PIN name to its UID.
 *  @param[in] pinName whose UID is desired.
 *  @return If the pinName is unknown, then NULL is returned.
 *  @return value points into fixed static memory and should not be changed!
 *  @return value is only good until the next call to this function.
 */
uid pinNameToUID(char *pinName);

/** Map an Authority name to its UID.
 *  @param[in] authorityName whose UID is desired.
 *  @return If the authorityName is unknown, then NULL is returned.
 *  @return value points into fixed static memory and should not be changed!
 *  @return value is only good until the next call to this function.
 */
uid authorityNameToUID(char *authorityName);

/** Map a Port name to its UID.
 *  @param[in] portName whose UID is desired.
 *  @return If the portName is unknown, then NULL is returned.
 *  @return value points into fixed static memory and should not be changed!
 *  @return value is only good until the next call to this function.
 */
uid portNameToUID(char *portName);

/** Map an SP Name to its UID.
 *  @param[in] spName whose UID is desired.
 *  @return If the spName is unknown, then NULL is returned.
 *  @return value points into fixed static memory and should not be changed!
 *  @return value is only good until the next call to this function.
 */
uid spNameToUID(char *spName);

/** Map a Table Name to its Table UID.
 *  @param[in] tableName whose UID is desired.
 *  @return If the tableName is unknown, then NULL is returned.
 *  @return value points into fixed static memory and should not be changed!
 *  @return value is only good until the next call to this function.
 */
uid tableNameToUID(char *tableName);

#endif
