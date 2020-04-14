#if CRYPTO_MATH_PBKDF
#include <stdint.h>

//-----------------------------------------------------------------------------
//
// Header: Math/sc_pbkdf.h
// Date: 2012/07/13
// Author: LeeCheng YU
//
// Description: This is trust module sc_pbkdf code, SP800-132.
//
//-----------------------------------------------------------------------------
#if !defined(__SC_FPBKDF_H_INCLUDED__)
#define __SC_FPBKDF_H_INCLUDED__
//
// Do NOT modify or remove this copyright and confidentiality notice!
//
// Copyright (c) 2001-2014 Seagate Technology, LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology, LLC.
// Portions are also trade secret. Any use, duplication, derivation, distribution
// or disclosure of this code, for any reason, not expressly authorized is
// prohibited. All other rights are expressly reserved by Seagate Technology, LLC.
//
//
// PBKDF HMAC HASH-256 functions:
// With reference to NIST Special Publication 800-132
// Recommendation for Password-Based Key Derivation. Part 1: Storage Applications (December 2010)
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
typedef  uint8_t uint8;
typedef  uint16_t uint16;
typedef  uint32_t uint32;
typedef  uint32_t Pstatus;
typedef  int8_t byte;
#define PASS 1
#define ABORTED -1

#define MAX_LEN_ALLOWED 4294967294U // 2^32 - 1
#define U_LEN  64+4 // 


// #defines
#define PBKDF_CHECK_TIMING  0

// debug print
#define PBKDF_DEBUG 0

#if PBKDF_DEBUG
#define M_DEBUG_PBKDF_PRN( msg, output, len )                         \
{                                                                      \
   uint32 i = 0;                                                       \
   XmitAString(msg );                                            \
   for( i=0; i<len; i++)                                               \
   {                                                                   \
      XmitAString( ConvertInt8ToASCII(*(output+i)) );            \
      XmitAString(" " );                                         \
   }                                                                   \
}                                                                      \

#else
#define  M_DEBUG_PBKDF_PRN( msg, output, len )
#endif

Pstatus PBKDF( uint8* P, 
				 uint32 PLen,
				 uint8* S,
				 uint32 SLen,
				 uint32 counter,
				 uint32 KLen, 
				 uint8* output );

#endif /* __SC_FPBKDF_H_INCLUDED__ */
#endif // CRYPTO_MATH_PBKDF

