// Do NOT modify or remove this copyright and confidentiality notice!
//
// Copyright (c) 2001-2013 Seagate Technology, LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology, LLC.
// Portions are also trade secret. Any use, duplication, derivation, distribution
// or disclosure of this code, for any reason, not expressly authorized is
// prohibited. All other rights are expressly reserved by Seagate Technology, LLC.
//
#if ( CRYPTO_MATH )
//----------------------------------------------------------
//
// Header: Math/types.h
// Date: 2010/01/07
// Author: sumanth.j.venkata
//
// Description: This is trust module types code. See Trusted Drive Math library.
//
//----------------------------------------------------------

//--------------------------------------------------------------------------
//	Typedef definitions and library macros.
//--------------------------------------------------------------------------
typedef uint8 byte;

#ifndef _TYPES_H_
	#define	_TYPES_H_

	typedef signed char signed_byte;

#ifndef _WIN32
	typedef signed int signed_word;
    typedef unsigned long sc_time;
#endif

	#define	WORD_MAX	0xFFFF
	#define	WORD_SZ		2

typedef unsigned long long dword;
	typedef dword track_type;
	typedef  byte error_code;
	typedef  byte head_type;
	typedef uint16  sector_type;

	#define	DWORD_MAX	0xFFFFFFFFUL
	#define	DWORD_SZ	4

	typedef struct qword
	{
		dword	LeastSignificantDWord;
		dword	MostSignificantDWord;
	} qword;

	// Definitions for checking bits
	//	Convert minutes to seconds
	#define MINUTES_TO_SECONDS( minutes )	((minutes) * SECONDS_PER_MINUTE)

	// Mask ROM Defines
	#define SIZEOFMASKROM 0x10000			// Size of the mask rom in words
	#define OFFSET 0x200					// Offset to start calculating the checksum
	#define BUFFERTOREADFROM 0x800			// Bufer number corresponding to bottom of WINC = 2
	#define CHECKSUMLOCATION 0x0FFFE		// Check sum entry location in the serial flash
	#define BUFFERCODEDOWNLOADED 0x400		// Buffer the .sfl file is downloaed to
	
	typedef union double_parm
	{
		dword BigParm;

		struct   
		{
			uint16 low;
			uint16 high;
		} split;

	} double_parm;	  
#endif

#endif // #if ( CRYPTO_MATH )
