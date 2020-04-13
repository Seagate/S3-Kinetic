#if CRYPTO_MATH_AES
#include "sc_aes.h"
#include "sc_aescrypt.h"
//-----------------------------------------------------------------------------
//
// Header: Math/sc_aes.c
// Date: 2010/01/07
// Author: sumanth.j.venkata
//
// Description: This is trust module sc_cbn code. See SeaCos crypto library.
//
//-----------------------------------------------------------------------------

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
// Derived from Dr. Brian Gladman's public domain implementation.
//

//--------------------------------------------------------------------------
//
// Portable C version of AES functions
//
//--------------------------------------------------------------------------

/* Encrypt or decrypt one or more blocks of data in ECB mode 
 * 0 - bad function return value
 * 1 - good function return value
 */
int AesCryptBlocks(unsigned char * key,  // key
                   int keyLen,                // key length (16 or 32)
                   unsigned char * block,// 128 bit data, en/decrypted in place
                   int decrypt,          // 0=>encrypt, 1=>decrypt
                   int NumBlocks)        // Number of 128-bit blocks in message
{

   int i;
   for(i=0; i<NumBlocks; i++)
   {
      if ( AesCrypt(key, keyLen, block, decrypt) == 0 )
      {
         return 0;
      }
      block += AES_BLOCK_SIZE;
   }

   return 1;
}



/* Encrypt or decrypt a block of data in ECB mode 
 * 0 - bad function return value
 * 1 - good function return value
 */
int AesCrypt(unsigned char * key,  // key
              int keyLen,                // key length (16 or 32)
              unsigned char * block,// 128 bit data, en/decrypted in place
              int decrypt)               // 0=>encrypt, 1=>decrypt
{
  aes_ctx ctx;
  aes_rval rval;
  
  if (decrypt) {
    if ((rval = aes_dec_key((unsigned char*)key, keyLen, &ctx))!=0)
      rval = aes_dec_blk(block, block, &ctx);
  }
  else {
    if ((rval = aes_enc_key((unsigned char*)key, keyLen, &ctx))!=0)
      rval = aes_enc_blk(block, block, &ctx);
  }

  return rval;
}

/* AES-G one way function 
 * 0 - bad function return value
 * 1 - good function return value
 */
int AesG( unsigned char * key,  // key
	   int keyLen,           // key length (16 or 32)
	   unsigned char *in,    // 128 bit input data
	   unsigned char *out)   // 128 bit output data
{
  int i;
  aes_rval rval;

  M_Memcpy( out, in, 16);
  if ((rval = AesCrypt(key, keyLen, out, 0)) == aes_bad)
    return rval;;

  // XOR 
  for (i=0;i<16;i++)
    out[i] ^= in[i];

  return rval;
}

// Function to perform AES encryption in Cipher Block Chaining Mode
// The initialization vector, iv, is passed in and then updated
// to equal the last block of ciphertext, so long messages
// can be transformed via a sequence of calls to this function.
// The in and out arguments can point to the same location.
// Returns non-zero if inLength is not multiple of 16 bytes.
int AesCbcEncrypt                   
     ( unsigned char * key,    // key
       int keyLen,                  // key length (16 or 32)
       unsigned char * out,          // plaintext appears here.
       unsigned char * in,     // ciphertext read from this location.
       unsigned int inLen,             // byte length of input.
       unsigned char * iv )          // 16 byte IV updated by function.
{
  unsigned int i, j;
  aes_rval rval;

  for (i=0;i<inLen;i+=16) {
    // do CBC chaining prior to encryption
    for (j=0; j<16; j++)
      iv[j] ^= in[j+i];

    if ((rval = AesCrypt(key, keyLen, iv, 0)) == aes_bad)
      return rval;

    for (j=0; j<16; j++)
      out[j+i] = iv[j];
  }

  return (rval);
}

// Function to perform AES decryption in Cipher Block Chaining Mode
// The initialization vector, iv, is passed in and then updated
// to equal the last block of ciphertext, so long messages
// can be transformed via a sequence of calls to this function.
// The in and out arguments can point to the same location.
// Returns non-zero if inLength is not multiple of 16 bytes.
int AesCbcDecrypt                   
     ( unsigned char * key,    // 16 byte key
       int keyLen,                  // key length (16 or 32)
       unsigned char * out,          // plaintext appears here.
       unsigned char * in,     // ciphertext read from this location.
       unsigned int inLen,                // byte length of input.
       unsigned char * iv )          // 16 byte IV updated by function.
{
  unsigned int i, j;
  unsigned char prevBlk[16];
  aes_rval rval;
  
  M_Memcpy( out, in, inLen);

  for (i=0;i<inLen;i+=16) {
    M_Memcpy( prevBlk, out+i, 16);

    if ((rval = AesCrypt(key, keyLen, out+i, 1)) == aes_bad)
      return rval;

    for (j=0; j<16; j++)
      out[j+i] ^= iv[j];

    M_Memcpy( iv, prevBlk, 16);
  }

  return rval;
}
#endif // CRYPTO_MATH_AES
