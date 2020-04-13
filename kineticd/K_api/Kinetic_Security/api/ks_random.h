#ifndef KS_RANDOM_H
#define KS_RANDOM_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_random.h
// Date: 2014/03/18
// Author: Chris N Allo
//
// Description: Types and definitions for ks_random function.
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


/**
 * @file ks_random.h
 *
 * Returns random bytes from the drive's Random Number Generator.
 *
 */

/**
 * @param[out] Random_String points to a buffer to hold the random bytes requested.
 * @param[in] Num_Random_Bytes how many random bytes to be returned.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 *
 * Notes:
 * * If *Random_String* does not point to at least *Num_Random_Bytes* bytes, corruption may happen.
 * * *Num_Random_Bytes* will not be NULL terminated and may contain NULLs, as random data is wont to do.
 * * This may involve several calls down to the drive, depending on the value of
 * *Num_Random_Bytes*. As a result, data pointed to by *buffer* may or may not be partially
 * overwritten if an error is returned.
 */


extern ks_status ks_random(unsigned char *Random_String, uint64_t Num_Random_Bytes);

#ifdef __cplusplus
}
#endif
#endif
