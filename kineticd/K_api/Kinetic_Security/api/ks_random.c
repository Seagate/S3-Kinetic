/*-----------------------------------------------------------------------------
*
*     Header: ks_random.c
*     Date: 2014/03/18
*     Author: Chris N Allo
*
*     Description: Generate random numbers
* 
*-----------------------------------------------------------------------------
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
*     
**
 * @file ks_random.c
 *
 * Generates the number of random bytes the user specified
 *      from the drive's Random Number Generator
 *
 */

/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Local functions */
#include "ks_globals.h"
#include "ks_setup.h"
#include "ks_random.h"
#include "ks_debug.h"

/* From EAPI functions */
#include "low_level.h"
#include "high_level.h"
#include "parameters.h"
#include "utilities.h"
#include "memory_helpers.h"
#include "debug.h"


/* This is the MAXimum number of random bytes the drive can return at once, so larger requests
 * will need to make more than one call to the drive.
 */

#define GEN_RANDOM_MAX  32

/* NOTES:
 * Must run ks_setup function before this call or it will return an error
 *    
 */
   
ks_status ks_random(unsigned char *RandomString, uint64_t Num_Random_Bytes) {

   uint64_t		Full_Calls, Call_Size;
   unsigned char 	randomData[32];
   uint64_t		stroffset;


  /* We assume ks_setup command was called before this routine that will have done initial questioning
   * of the drive.  If it hasnt been called the Device_Identifier string will be NULL and we should quit.
   */
   
   /* Device Identifier is set by ks_security, if that hasnt been called it will be NULL */   
   if (Device_Identifier == NULL) {
     /* we cant continue since ks_setup has not been run. it loads discovery data structure */
     if (Debug_Info > DEBUG_OFF) 
       KS_DEBUG (0, KS_INFO,"KS_RANDOM:General Failure: Device Identifier = NULL  Need to run KS_SETUP ");
     /* Issue a stack reset command here to clean things up and close the session */
     issueStackReset(NULL);  /* we use NULL as comId per Doug P. */
     return (KS_SYSTEM_SETUP_FAILURE);
   }

   if (Debug_Info == 2) { 
     /* if debug set to 2 we leave after we enter */
     KS_DEBUG(KS_DEBUG_EXIT, KS_RETURN, "KS_RANDOM:Debug Level 2 [Immediate Return]");
     return(KS_DEBUG_EXIT);
   }

   /* determine how many calls to generateRandom we need to make.  generateRandom returns max 32 bytes per call */
   /* generateRandom will open a session and close session when done*/
   Full_Calls = Num_Random_Bytes / GEN_RANDOM_MAX;
   Call_Size = (uint64_t)GEN_RANDOM_MAX;

   stroffset = 0;
   
   /* grab as many full 32 byte calls as we need */
   for(; Full_Calls > 0; Full_Calls--) {
     KS_DEBUG(generateRandom("AdminSP", &Call_Size, (char *)randomData),KS_INFO, "Generate Random FULL");
     memmove(RandomString+stroffset, randomData, GEN_RANDOM_MAX );
     stroffset += GEN_RANDOM_MAX;
    }

   Call_Size = Num_Random_Bytes % (uint64_t)GEN_RANDOM_MAX;

   if (Call_Size > 0) {
     /* now grab partial data to finish the request */
     KS_DEBUG(generateRandom("AdminSP", &Call_Size, (char *)randomData),KS_INFO,"Generate Random Partial");
     memmove(RandomString+stroffset, randomData, Call_Size );
     stroffset += Call_Size;
   }

   if (Debug_Info == DEBUG_CRYPTO) {
     /* lets print the RandomString */
     dumpBuffer(RandomString, RandomString+stroffset);
   }

  return KS_SUCCESS;
}



