#if CRYPTO_MATH_AES
#if !defined(__SC_AES_H_INCLUDED__)
#define __SC_AES_H_INCLUDED__
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
// Derived from Dr Brian Gladman's public domain implementation.
//

//--------------------------------------------------------------------------
//
//
// AES implementation
// (based on public domain code by Dr. Brian Gladman)
//


#define AES_DEBUG         1
#define AES_BLOCK_SIZE    16
#define AES_128_KEY_SIZE  16
#define AES_256_KEY_SIZE  32
#define AES_ERROR_KEY     0x01
#define AES_ERROR_ENC     0x02
#define AES_ERROR_DEC     0x03

//
// AES API
//

// Function en/decrypts one 128-bit block
int AesCrypt( unsigned char * key,
               int keyLen,          // key length (16 or 32)
               unsigned char * block,// data, en/decrypted in place
               int decrypt);              // 0=>encrypt, 1=>decrypt

int AesCryptBlocks(unsigned char * key,  // key
                   int keyLen,                // key length (16 or 32)
                   unsigned char * block,// 128 bit data, en/decrypted in place
                   int decrypt,          // 0=>encrypt, 1=>decrypt
                   int NumBlocks);        // Number of 128-bit blocks in message

// Function to do AES-G one way function on a 128-bit block
int AesG( unsigned char * key,  // key
	   int keyLen,           // key length (16 or 32)
	   unsigned char *in,
	   unsigned char *out);

// Function to perform AES decryption in Cipher Block Chaining Mode.
// The initialization vector, iv, is passed in and then updated
// to equal the last block of ciphertext, so long messages
// can be transformed via a sequence of calls to this function.
// The in and out arguments can point to the same location.
// Returns non-zero if inLength is not multiple of 16 bytes.
int AesCbcDecrypt                   
     ( unsigned char * key,    // key
       int keyLen,                  // key length (16 or 32)
       unsigned char * out,          // plaintext appears here.
       unsigned char * in,     // ciphertext read from this location.
       unsigned int inLen,             // byte length of input.
       unsigned char * iv );         // 16 byte IV updated by function.

// Function to perform AES encryption in Cipher Block Chaining Mode.
// The initialization vector, iv, is passed in and then updated
// to equal the last block of ciphertext, so long messages
// can be transformed via a sequence of calls to this function.
// Returns non-zero if inLength is not multiple of 16 bytes.
int AesCbcEncrypt                   
     ( unsigned char * key,    // 16 byte key
       int keyLen,                  // key length (16 or 32)
       unsigned char * out,          // ciphertext appears here.
       unsigned char * in,     // plaintext read from this location.
       unsigned int inLen,             // byte length of input.
       unsigned char * iv );         // 8 byte IV updated by function.
    
#endif // !defined(__SC_AES_H_INCLUDED__)
#endif // CRYPTO_MATH_AES
