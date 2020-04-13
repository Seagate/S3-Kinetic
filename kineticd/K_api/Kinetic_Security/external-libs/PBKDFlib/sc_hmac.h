#if !defined(__SC_HMAC_H_INCLUDED__)
#define __SC_HMAC_H_INCLUDED__
//
// Do NOT modify or remove this copyright and confidentiality notice!
//
// Copyright (c) 2001-2013 Seagate Technology, LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology, LLC.
// Portions are also trade secret. Any use, duplication, derivation, distribution
// or disclosure of this code, for any reason, not expressly authorized is
// prohibited. All other rights are expressly reserved by Seagate Technology, LLC.
//

//
// sc_hmac.h : Implementation of the Secure Keyed-Hash Message Authentication Code (HMAC) Algorithm 
//
#if CRYPTO_MATH_HMAC

#include "sc_aes.h"
#include "sc_pbkdf.h"
#include "sc_sha2.h"
#include "sc_hmac.h"
typedef  uint16_t uint16;

#define SC_HMAC_DEBUG_MSG (0)



#define HMAC_SHA256_DIGEST_SIZE 32  //FIPS-198: L: size in bytes
#define HMAC_SHA256_BLOCK_SIZE  64  //FIPS-198: B: size in bytes
#define IPAD_CHAR           0x36 //FIPS-198: ipad
#define OPAD_CHAR           0x5C //FIPS-198: opad

void PrepareIpadOpad(SHA256_CTX *Sha256DigestContext, uint8 *ipad, uint8 *opad, uint8 *HashKeyMaterial, uint16 HashKeyLength);
void HMACInit(SHA256_CTX *Sha256DigestContext, uint8 *ipad, uint8 *opad, uint8 *HashKeyMaterial, uint16 HashKeyLength);
void HMACUpdate(SHA256_CTX *Sha256DigestContext, uint8 *InputData, uint16 DataLength);
void HMACFinalize(SHA256_CTX *Sha256DigestContext, uint8 *BufferOut, uint8 *opad);
void HMAC_GetDigest(uint8 *InputData, uint16 DataLength, uint8* HashKeyMaterial, uint16 HashKeyLength, uint8* DigestHMAC, uint16 HMacDigestSize);
#endif

#endif //#if !defined(__SC_HMAC_H_INCLUDED__)

