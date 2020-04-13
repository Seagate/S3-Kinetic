#if CRYPTO_MATH_PBKDF
#include <string.h> /* memset */

#include "sc_aes.h"
#include "sc_pbkdf.h"
#include "sc_sha2.h"
#include "sc_hmac.h"

//-----------------------------------------------------------------------------
//
// Header: Math/sc_pbkdf.c
// Date: 2012/07/13
// Author: LeeCheng YU
//
// Description: This is trust module sc_pbkdf code, SP800-132.
//
//-----------------------------------------------------------------------------
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
Pstatus PBKDF( uint8* P, 
				 uint32 PLen,
				 uint8* S,
				 uint32 SLen,
				 uint32 counter,
				 uint32 KLen, 
				 uint8* output )
{
	Pstatus Status = PASS;
        byte U[ U_LEN ]; 
																
	uint32 hlen = SHA256_DIGEST_SIZE; 
	uint32 len, r, uLen, xLen, oLen, offset;

	int i,j;

   SHA256_CTX ctx;
   uint8 ipad[64];
   uint8 opad[64];

#if PBKDF_CHECK_TIMING
   millisecond_timer_value Start = 0;
   millisecond_timer_value Stop = 0;
#endif

/** PBKDF  function 
	Input:	P Password
			S Salt
			C Iteration count
			kLen Length of MK in bits; at most (232-1) „e hLen
			Parameter: PRF HMAC with an approved hash function
			hlen Digest size of the hash function
	Output: mk Master key
	Algorithm:
		If (kLen > (2^32-1) * hLen)
			Return an error indicator and stop ;
		len = Ceiling(kLen / hLen) ;
		r = kLen - (len - 1) * hLen ;
		For i = 1 to len
			Ti = 0;
			U0 = S || Int(i);
			For j = 1 to C
				Uj= HMAC(P, Uj-1)
				Ti = Ti „v Uj
		Return mk = T1 || T2 || ¡K || Tlen<0¡Kr-1>
**/


#if PBKDF_CHECK_TIMING
   Start = GetMillisecondTimerValue();
#endif
	if( KLen > MAX_LEN_ALLOWED )
	{
		return( ABORTED );
	}

	// ceiling/round up len
	len = ( ( KLen + ( hlen - 1 ) ) / hlen );	
	r = KLen - ( ( len - 1 ) * hlen );

	// set XOR length for HMAC looping
	xLen = KLen < hlen? KLen : hlen;
	offset = 0;

	for( i=1; i<=len; i++ )
	{
   		// init T with zeros T = 0
  		memset( output+offset, 0, xLen );
  		// prepare U0 =>  U0 = S || i
		// i = 32-bit encoding of integer i, with most significant bit on the left..
  		M_Memcpy( U, S, SLen );
		U[SLen + 0] = (i & 0xff000000) >> 24;
	    U[SLen + 1] = (i & 0x00ff0000) >> 16;
	    U[SLen + 2] = (i & 0x0000ff00) >> 8;
	    U[SLen + 3] = (i & 0x000000ff);

		// first time the U length may vary depending on Salt
		uLen = SLen + sizeof( int );
		for( j=1; j<=counter; j++)
		{
     	 	// Uj= HMAC(P, U(j-1))
     	 	HMACInit( &ctx, ipad, opad, P, PLen );
     	 	HMACUpdate( &ctx, ( uint8* ) U, j==1? uLen : hlen );
     	 	HMACFinalize(&ctx, ( uint8* ) U, opad);
	 	
     	 	//M_DEBUG_PBKDF_PRN( "\n\r cal-HMAC: ", U, SHA256_DIGEST_SIZE );
			
			//XOR each byte
			for( oLen=0; oLen < xLen; oLen++ )
			{
				*(output+offset+oLen ) ^= *(U+oLen);
		    }
		} //end counter loop
		offset += xLen;
		KLen -= xLen;
		xLen = KLen < hlen? KLen : hlen;
	} //end first for loop
#if PBKDF_CHECK_TIMING
		Stop = GetElapsedMilliseconds( Start );
     	XmitAString("\n\r PBKDF: Time taken for 0x" );
     	XmitAString( ConvertInt32ToASCII( counter ) );   
     	XmitAString(" loops in msec is " );
     	XmitAString( ConvertInt32ToASCII( Stop ) );   
#endif
	return ( Status );
}													


#endif // CRYPTO_MATH_PBKDF
