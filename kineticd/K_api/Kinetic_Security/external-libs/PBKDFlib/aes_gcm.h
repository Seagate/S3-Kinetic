#if CRYPTO_MATH_AES_GCM
//-----------------------------------------------------------------------------
//
// Header: Math/sc_aes.h
// Date: 2013/07/12
// Author: tim.j.courtney
//
// Description: This is trust module aes_gcm header.
//
//-----------------------------------------------------------------------------

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
//--------------------------------------------------------------------------
#ifndef AES_GCM_MODE_H
#define AES_GCM_MODE_H

//******************************************************************************
// Thees three flags should be set to 0 (disabled) for normal operation
#define RUN_AES_GCM_GALOIS_MULTIPLY_TESTS 0  // Trun on to enable the Galois multiply test vectors and code to be compiled, must call AES_GCM_Mode_Test()
#define RUN_AES_GCM_TEST_VECTORS          0  // Set to 1 to enable the AES GCM mode test vectors and code to be compiled, must call AES_GCM_Mode_Test()
#define AES_GCM_VERBOSE_OUTPUT            0  // Set to 1 to enable verbose mode output

//******************************************************************************

#define BLOCK_LEN          AES_BLOCK_SIZE
#define MAX_KEY_LEN        AES_256_KEY_SIZE

// Both of these will need to be increased in the future but should be large enough for key wrap for now
#define FW_MAX_PCLEN       512   // This constant controls the maximum size of the plain text in bytes
#define FW_MAX_ALEN        512   // This constant controls the maximum size of the associated data in bytes

#define MAX_PLAIN_TEXT_LEN 0x7FFFFFCE
#define MAX_ADATA_LEN_HI   0xFFFFFFFF
#define MAX_ADATA_LEN_LO   0xFFFFFFFF

//******************************************************************************
typedef enum
{
   NO_ERROR,                               // 0
   ERROR_PLAINTEXT_TOO_LARGE,              // 1
   ERROR_ASSOCIATED_DATA_TOO_LARGE,        // 2
   ERROR_CYPHERTEXT_TOO_LARGE,             // 3
   ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH, // 4
} GCM_error;

//******************************************************************************
GCM_error EncryptGCM_Mode( uint8 *P8, uint32 PLen, uint8 *A8, uint32 ALen, uint8 *IV8, uint16 IVLen, uint8 *K8, uint16 KLen, uint8 *C8, uint32 *CLen, uint8 *T8, uint16 *TLen );
GCM_error DecryptGCM_Mode( uint8 *C8, uint32 CLen, uint8 *A8, uint32 ALen, uint8 *T8, uint16 TLen, uint8 *IV8, uint16 IVLen, uint8 *K8, uint16 KLen, uint8 *P8, uint32 *PLen );
GCM_error EncryptGCM_Mode_Wrapper( uint8 *P, uint32 PLen, uint8 *A, uint32 ALen, uint8 *IV, uint16 IVLen, uint8 *K, uint16 KLen, uint8 *C, uint32 *CL, uint8 *T, uint16 *TL );
GCM_error DecryptGCM_Mode_Wrapper( uint8 *C, uint32 CLen, uint8 *A, uint32 ALen, uint8 *T, uint16 TLen, uint8 *IV, uint16 IVLen, uint8 *K, uint16 KLen, uint8 *P, uint32 *PL );
void AES_GCM_Mode_Test( void );

//******************************************************************************
#endif
#endif// #if CRYPTO_MATH_AES_GCM

//******************************************************************************
