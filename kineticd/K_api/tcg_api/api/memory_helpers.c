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
 *  Functions for parsing structure components.
 */

#include <string.h>
#include <stdio.h>  /* For sprintf, etc. */
#include <ctype.h>

#include "memory_helpers.h"

/* Note: Left as an optimization for the reader: using memcpy or other BitBlt-ing operations. */

byteSource copyInt64s(byteSource inputPtr, unsigned int numberOfIntsToCopy, uint64_t *destination) {
  if (inputPtr == NULL) {
    return NULL;
  }

  for(;numberOfIntsToCopy > 0; numberOfIntsToCopy--) {
    uint64_t source = 0;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    if (destination != NULL) {
      *destination = source;
      destination++;
    }
  }
  return inputPtr;
}

byteSource copyInt32s(byteSource inputPtr, unsigned int numberOfIntsToCopy, uint32_t *destination) {
  if (inputPtr == NULL) {
    return NULL;
  }

  for(;numberOfIntsToCopy > 0; numberOfIntsToCopy--) {
    uint32_t source = 0;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    if (destination != NULL) {
      *destination = source;
      destination++;
    }
  }
  return inputPtr;
}

byteSource copyInt16s(byteSource inputPtr, unsigned int numberOfIntsToCopy, uint16_t *destination) {
  if (inputPtr == NULL) {
    return NULL;
  }

  for(;numberOfIntsToCopy > 0; numberOfIntsToCopy--) {
    uint16_t source = 0;
    source |= *inputPtr++;
    source <<= 8;
    source |= *inputPtr++;
    if (destination != NULL) {
      *destination = source;
      destination++;
    }
  }
  return inputPtr;
}

byteSource copyBytes(byteSource inputPtr, unsigned int numberOfBytesToCopy, unsigned char *destination) {
  if (inputPtr == NULL) {
    return NULL;
  }

  for(;numberOfBytesToCopy > 0; numberOfBytesToCopy--) {
    if (destination != NULL) {
      *destination++ = *inputPtr;
    }
    inputPtr++;
  }
  return inputPtr;
}

byteSource copyNibbles(byteSource inputPtr, unsigned char *msNibble, unsigned char *lsNibble) {
  unsigned char byte;
  if (inputPtr == NULL) {
    return NULL;
  }
  byte = *inputPtr++;
  if (msNibble != NULL) {
    *msNibble = byte >> 4;
  }
  if (lsNibble != NULL) {
    *lsNibble = byte & 0xF;
  }
  
  return inputPtr;
}


void hexlify(unsigned char *source, unsigned char *printable_result, unsigned int source_len) {
  unsigned int i;
  for (i = 0; i < source_len; i++) {
    sprintf((char *)(printable_result+2*i), "%02x", source[i]);
  }
  printable_result[2*i] = '\0';
}

unsigned char fromHex(unsigned char c) {
  if (isxdigit(c)) {
    if (isdigit(c)) {
      c -= '0';
    } else {
      c = tolower(c);
      c = c - 'a' + 10;
    }
  } else {
    c = 0;
  }
  return c;
}

void unhexlify(unsigned char *hex_str, unsigned char *binary_result) {
  int l, i;
  unsigned char c;
  l = strlen((char *)hex_str)/2;
  for (i = 0; i < l; i++) {
    c = fromHex( hex_str[2*i+1]) + 16*fromHex( hex_str[2*i]);
    binary_result[i] = c;
  }
}

