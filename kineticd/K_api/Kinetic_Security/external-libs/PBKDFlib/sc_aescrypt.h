#if CRYPTO_MATH_AES

#if !defined(__AES_H_INCLUDED__)
#define __AES_H_INCLUDED__
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
// AES implementation
// (based on public domain code by Dr Brian Gladman)
// This file contains the definitions required to use AES (Rijndael) in C.
//

typedef unsigned char      aes_08t;
typedef   unsigned long    aes_32t;

/*  BLOCK_SIZE is in BYTES: 16, 24, 32 or undefined for aes.c and 16, 20, 
    24, 28, 32 or undefined for aespp.c.  When left undefined a slower 
    version that provides variable block length is compiled.    
*/

#define BLOCK_SIZE  16

/* key schedule length (in 32-bit words)    */

#define KS_LENGTH   4 * BLOCK_SIZE

#if defined(__cplusplus)
extern "C"
{
#endif

typedef unsigned int aes_fret;   /* type for function return value       */
#define aes_bad      0           /* bad function return value            */
#define aes_good     1           /* good function return value           */
#define aes_rval     aes_fret

typedef struct                     /* the AES context for encryption   */
{   aes_32t    k_sch[KS_LENGTH];   /* the encryption key schedule      */
    aes_32t    n_rnd;              /* the number of cipher rounds      */
    aes_32t    n_blk;              /* the number of bytes in the state */
} aes_ctx;

aes_rval aes_enc_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1]);
aes_rval aes_enc_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);

aes_rval aes_dec_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1]);
aes_rval aes_dec_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);

#if defined(__cplusplus)
}
#endif

#endif // !defined(__AES_H_INCLUDED__)
#endif // CRYPTO_MATH_AES
