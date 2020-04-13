#if CRYPTO_MATH_SHA256 || CRYPTO_MATH_SHA384 || CRYPTO_MATH_SHA512

#if !defined(__SC_SHA2_H_INCLUDED__)
#define __SC_SHA2_H_INCLUDED__

//
// Do NOT modify or remove this copyright and confidentiality notice!
//
// Copyright (c) 2001-2011 Seagate Technology, LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology, LLC.
// Portions are also trade secret. Any use, duplication, derivation, distribution
// or disclosure of this code, for any reason, not expressly authorized is
// prohibited. All other rights are expressly reserved by Seagate Technology, LLC.
//
//
// Author: Pao-Chi Hwang, Plus Five Consulting, Inc.
// 

//--------------------------------------------------------------------------
//
// SHA1 Digest function operations.
//
//--------------------------------------------------------------------------
#include <stdint.h>

typedef uint32_t uint32;
typedef uint64_t uint64;

#define SHA256_DIGEST_SIZE (256 / 8)
#define SHA384_DIGEST_SIZE (384 / 8)
#define SHA512_DIGEST_SIZE (512 / 8)

#define SHA256_BLOCK_SIZE  ( 512 / 8)
#define SHA512_BLOCK_SIZE  (1024 / 8)
#define SHA384_BLOCK_SIZE  SHA512_BLOCK_SIZE


//  DO NOT MODIFY THIS STRUCT. Assembly routines rely on the 
//  exact offsets of the members.
typedef struct {
    unsigned int tot_len;
    unsigned int len;
    unsigned char block[2 * SHA256_BLOCK_SIZE];
    uint32 h[8];
} SHA256_CTX;

typedef struct {
    unsigned int tot_len;
    unsigned int len;
    unsigned char block[2 * SHA512_BLOCK_SIZE];
    uint64 h[8];
} SHA512_CTX;

typedef SHA512_CTX SHA384_CTX;

// Message digest functions

void SHA256Init(SHA256_CTX * ctx);
void SHA256Update(SHA256_CTX *ctx, unsigned char *message, 
		   unsigned int len);
void SHA256Final(SHA256_CTX *ctx, unsigned char *digest);
void PBKDF_SHA256(unsigned char *message, unsigned int len, 
	    unsigned char *digest);

void SHA384Init(SHA384_CTX *ctx);
void SHA384Update(SHA384_CTX *ctx, unsigned char *message,
                   unsigned int len);
void SHA384Final(SHA384_CTX *ctx, unsigned char *digest);
void SHA384(unsigned char *message, unsigned int len, 
	    unsigned char *digest);

void SHA512Init(SHA512_CTX *ctx);
void SHA512Update(SHA512_CTX *ctx, unsigned char *message, 
		   unsigned int len);
void SHA512Final(SHA512_CTX *ctx, unsigned char *digest);
void SHA512(unsigned char *message, unsigned int len, 
	    unsigned char *digest);

#endif // !defined(__SC_SHA2_H_INCLUDED__)
#endif // CRYPTO_MATH_SHA256 || CRYPTO_MATH_SHA384 || CRYPTO_MATH_SHA512

