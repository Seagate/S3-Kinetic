#ifndef MEMORY_HELPERS_H
#define MEMORY_HELPERS_H

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
 *  Functions for copying structured data from memory blobs.
 */

#include <stdint.h>

typedef unsigned char *byteSource;

extern byteSource copyInt64s(byteSource inputPtr, unsigned int numberOfIntsToCopy, uint64_t *destination);
extern byteSource copyInt32s(byteSource inputPtr, unsigned int numberOfIntsToCopy, uint32_t *destination);
extern byteSource copyInt16s(byteSource inputPtr, unsigned int numberOfIntsToCopy, uint16_t *destination);
extern byteSource copyBytes(byteSource inputPtr, unsigned int numberOfBytesToCopy, unsigned char *destination);
extern byteSource copyNibbles(byteSource inputPtr, unsigned char *msNibble, unsigned char *lsNibble);

/** BIT extracts the given most-significant-bit-first, 0-based bitNumber from the given byte value */
#define BIT(value, bitNumber) (((value) >> (bitNumber)) & 1)

extern void hexlify(unsigned char *source, unsigned char *printable_result, unsigned int source_len);
extern unsigned char fromHex(unsigned char c);
extern void unhexlify(unsigned char *hex_str, unsigned char *binary_result);

#endif
