#ifndef KS_GLOBALS_H
#define KS_GLOBALS_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_globals.h
// Date: 2014/03/05
// Author: Chris N Allo
//
// Description: Types and definitions for all ks_*  functions.
//
//-----------------------------------------------------------------------------


// **
/*
 * Do NOT modify or remove this copyright and confidentiality notice.
 *
 * Copyright 2014 Seagate Technology LLC.
 *
 * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 * and may be covered under one or more Non-Disclosure Agreements. All or
 * portions are also trade secret. Any use, modification, duplication,
 * derivation, distribution or disclosure of this code, for any reason,
 * not expressly authorized is prohibited. All other rights are expressly
 * reserved by Seagate Technology LLC.
 *
 */
//
// **


/**
 * @file ks_globals.h
 *
 * Definitions and globals for all KS_XXX Security functions.
 *
 */

/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 	1
#define FALSE 	0

/* Global error codes passed out of Kinetic Security API to User */
typedef enum {
  KS_SUCCESS = 0,
  KS_SYSTEM_SETUP_FAILURE,
  KS_GENERAL_FAILURE,
  KS_PIN_UNWRAP_FAILURE,
  KS_DATA_LOCKED_FAILURE,
  KS_BAND_SET_FAILURE,
  KS_LOCKING_FAILURE,
  KS_PARAMETER_ERROR,
  KS_PROTOCOL_ERROR,
  KS_DISCOVERY_ERROR,
  KS_INITIALIZE_FAILURE,
  KS_ERASE_FAILURE,
  KS_PORT_ERROR,
  KS_DEBUG_EXIT
} ks_status;

int Debug_Info;

/* Band Definitions. */
typedef uint64_t	band_size_t;
typedef uint64_t 	band_start_t;
typedef int 		band_number_t;

extern const char CURRENT_VERSION[];

#define BAND_NUMBER_VALID(x) (((x) >= 0) && ((x) < MAX_SETBANDS))

#define MAX_SUPPORTED_BANDS   2  /* change this when we will be able to support more bands */
#define MAX_SUPPORTABLE_BANDS 9  /* Maximum number of bands the code will support in this release */
#define MAX_BAND_SIZE	16       /* Maximum number of bands that can be supported in CTG spec */
#define PIN_SIZE 	32
#define HEADER_SIZE 	32
#define SALT_SIZE 	16
#define IV_SIZE		12
#define TAG_SIZE	16
#define MAX_SETBANDS     9   /* bands 0-8 can be encrypted separately */

#define MAX_DATASTORE_SIZE  1024 /* size of datastore area on drive that we are supporting */

#define ITERATION_COUNT	1000

/* DataStore definitions Version 2 */
typedef struct {
  char 		Header[HEADER_SIZE]; /*32*/   /* version description */
  unsigned char IV[IV_SIZE];	     /*12*/   /* common for all locks */
  unsigned char SaltSID[SALT_SIZE];  /*16*/
  unsigned char TagSID[TAG_SIZE];    /*16*/  	
  unsigned char SIDPin[PIN_SIZE];    /*32*/	/* SALT+TAG+PIN = 64 */
  unsigned char SaltEM[SALT_SIZE];		
  unsigned char TagEM[TAG_SIZE];
  unsigned char EraseMasterPin[PIN_SIZE];	/* SALT+TAG+PIN = 64 */
  unsigned char SaltBM[MAX_SETBANDS][SALT_SIZE];  	
  unsigned char TagBM[MAX_SETBANDS][TAG_SIZE];
  unsigned char BandMasterPin[MAX_SETBANDS][PIN_SIZE];/* EACH SALT+TAG+PIN = 64 */
  unsigned char SaltBM915[SALT_SIZE];  			
  unsigned char TagBM915[TAG_SIZE];
  unsigned char BandMasterPin915[MAX_BAND_SIZE-MAX_SETBANDS][PIN_SIZE]; /* 256 = Salt+Tag+7 bands*/
} SED_datastore_struct;

/* Transport Definitions */
#define DEV_ID_SIZE	32
extern char Device_Identifier[];   /* is this the size it should be? */


#ifdef __cplusplus
}
#endif
#endif
