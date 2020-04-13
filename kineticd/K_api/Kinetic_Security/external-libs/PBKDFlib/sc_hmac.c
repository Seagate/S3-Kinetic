#if CRYPTO_MATH_HMAC
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
// sc_hmac.c : Implementation of the Secure Keyed-Hash Message Authentication Code (HMAC) Algorithm 
//
#include <string.h> /* memset */

#include "sc_sha2.h"
#include "sc_hmac.h"
typedef  uint16_t uint16;

#if SC_HMAC_DEBUG_MSG
static void PrintArray(void * BufferAdd, uint32 Length)
{                                  
   uint32 i;

   if (Length > 15)                     
      XmitAString("\r\n");              
   if (Length > 2560)                   
      Length = 2560;                      
   for (i=0; i<Length; i++)             
   {                                      
      XmitAString(ConvertInt8ToASCII(*((uint8 *)(BufferAdd)+i)));
      XmitAString(" ");
      if ((i & 0xF) == 0xF)         
      {                             
         XmitAString("\r\n");           
      }                             
   }                                
   if ((i & 0xF) != 0x0)            
   {                                
      XmitAString("\r\n");              
   }                                
}
#endif //SC_APDU_HMAC_DEBUG_MSG

void PrepareIpadOpad(SHA256_CTX *Sha256DigestContext, uint8 *ipad, uint8 *opad, uint8 *HashKeyMaterial, uint16 HashKeyLength)
{
   uint16 loopindex = 0;
   uint8 *dest;
   uint8 *src;
   uint8 keyzero[HMAC_SHA256_BLOCK_SIZE];
   uint16 keylength;

   memset(keyzero, 0x00, HMAC_SHA256_BLOCK_SIZE);

   if (HashKeyLength > HMAC_SHA256_BLOCK_SIZE)
   {
      // Hash the key first.
      SHA256Init(Sha256DigestContext);
      SHA256Update(Sha256DigestContext, HashKeyMaterial  , HashKeyLength );
      SHA256Final(Sha256DigestContext, keyzero);
      keylength = HMAC_SHA256_DIGEST_SIZE;
   }
   else
   {
      M_Memcpy(keyzero, HashKeyMaterial, HashKeyLength);
      keylength = HashKeyLength;
   }

   // ipad
   src = keyzero;
   memset(ipad, IPAD_CHAR, HMAC_SHA256_BLOCK_SIZE);
   for (loopindex = 0; loopindex < keylength; loopindex++)
   {
      *ipad++ ^= *src++;//keyzero[loopindex];//
   }

   // opad
   src = keyzero;
   memset(opad, OPAD_CHAR, HMAC_SHA256_BLOCK_SIZE);
   for (loopindex = 0; loopindex < keylength; loopindex++)
   {
      *opad++ ^= *src++;//keyzero[loopindex];//
   }
}

void HMACInit(SHA256_CTX *Sha256DigestContext, uint8 *ipad, uint8 *opad, uint8 *HashKeyMaterial, uint16 HashKeyLength)
{
   #if SC_HMAC_DEBUG_MSG
   XmitAString( "\r\nHMAC init " );
   #endif //SC_APDU_HMAC_DEBUG_MSG

   PrepareIpadOpad(Sha256DigestContext, ipad, opad, HashKeyMaterial, HashKeyLength);
   SHA256Init(Sha256DigestContext);
   SHA256Update(Sha256DigestContext, ipad, HMAC_SHA256_BLOCK_SIZE );
}
void HMACUpdate(SHA256_CTX *Sha256DigestContext, uint8 *InputData, uint16 DataLength)
{
   #if SC_HMAC_DEBUG_MSG
   XmitAString( "\r\nHMAC Calc " );
   #endif //SC_APDU_HMAC_DEBUG_MSG

   // it is meant to update many times with the input data; 
   SHA256Update(Sha256DigestContext, InputData, DataLength );
}
void HMACFinalize(SHA256_CTX *Sha256DigestContext, uint8 *BufferOut, uint8 *opad)
{
   uint8 firstHashDigest[HMAC_SHA256_DIGEST_SIZE];

   SHA256Final(Sha256DigestContext, firstHashDigest);

   #if SC_HMAC_DEBUG_MSG
   XmitAString( "\r\nHMAC Finalize " );
   #endif //SC_APDU_HMAC_DEBUG_MSG

   SHA256Init(Sha256DigestContext);
   SHA256Update(Sha256DigestContext, opad   , HMAC_SHA256_BLOCK_SIZE );
   SHA256Update(Sha256DigestContext, firstHashDigest, HMAC_SHA256_DIGEST_SIZE );
   SHA256Final(Sha256DigestContext, BufferOut);

   #if SC_HMAC_DEBUG_MSG
   XmitAString( "\r\nTC5: HMACdigest is " );
   PrintArray(BufferOut, 32);
   #endif //SC_APDU_HMAC_DEBUG_MSG
}

void HMAC_GetDigest(uint8 *InputData, uint16 DataLength, uint8* HashKeyMaterial, uint16 HashKeyLength, uint8* DigestHMAC, uint16 HMacDigestSize)
{
   SHA256_CTX ctx;
   uint8 ipad[64];
   uint8 opad[64];

   HMACInit(&ctx, ipad, opad, HashKeyMaterial, HashKeyLength);
   HMACUpdate(&ctx, InputData, DataLength);
   HMACFinalize(&ctx, ipad, opad);

   if ( HMacDigestSize > HMAC_SHA256_DIGEST_SIZE ) // truncate the output length
   {
      HMacDigestSize = HMAC_SHA256_DIGEST_SIZE;
   }
   
   M_Memcpy(DigestHMAC, ipad, HMacDigestSize);  // copy result back to the DigestHMAC buffer
}
#endif // #if CRYPTO_MATH_HMAC

