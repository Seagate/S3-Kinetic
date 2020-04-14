
/**
 * @file ks_setup.c
 *
 * Functions that provide Drive-level Security (SED) for Kinetic.
 * Implementation Notes:
 * To be implemented on SED drives but to abstract above the TCG security layer.
 * Interleave Read/Write with Security commands?
 *
 *
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

/* Local includes */
#include "ks_globals.h"
#include "ks_setup.h"
#include "ks_debug.h"

/* From EAPI functions */
#include "low_level.h"
#include "high_level.h"
#include "transport.h"
#include "transport_locator.h"
#include "utilities.h"

parameters 	bandData;
char  		info_str[200];

#define GETBANDCOLUMNVALUE(name, destination) { \
    s = getNamedIntValue(&bandData, tcgTmpName(name), &destination); \
    if (hasError(s)) { \
      sprintf(info_str,"Error getting column data for '%s' for band %d\n", name, bandNumber); \
      KS_DEBUG(0,KS_INFO, info_str);\
      result = 1; \
      continue; \
    } \
}


/* Return 0 if bands are consistent with freshly manufactured drive.
 * Return non-0 if bands have been configured or if band info is not available.
 */
int bandsConfigured(void) {
  int result = 0;
  int bandNumber;
  status s;

  for (bandNumber = 0; bandNumber < MAX_BAND_SIZE; bandNumber++) {
    uint64_t startingLBA, length, writeLockEnabled, readLockEnabled ;
    resetParameters(&bandData);
    s = issueGet(bandNumberToUID(bandNumber), NULL, &bandData);
    if (hasError(s)) {
       sprintf(info_str,"Cannot read LockingInfo table entry for band %d\n", bandNumber);
       KS_DEBUG(0,KS_INFO, info_str);
       result = 1;
       continue;
    }

    GETBANDCOLUMNVALUE("RangeStart", startingLBA);
    GETBANDCOLUMNVALUE("RangeLength", length);
    GETBANDCOLUMNVALUE("WriteLockEnabled", writeLockEnabled);
    GETBANDCOLUMNVALUE("ReadLockEnabled", readLockEnabled);
  }
  return result;
}
/* ISSUES:
 * A device identifier string will be passed into this routine
 * ks_setup will create global storage for this identifier and
 * call discover to load a global discovered_data structure
 * that will be used for all subsequent functions.
 * This routine must be called first before any other subsequent
 * ks_XXX calls or an error will be output.
 *
 * Setup parameters global to all Kinetic Security API calls
 *
 */ 
discovered_data discovery_data;
   
ks_status ks_setup(char *Device_ID) {
    transport 	*tloc_ptr;
    status 	EA_Status;
    int 	configured;
 /*
 * setup Device Identifier passed in to main routine */
  strcpy (Device_Identifier, Device_ID);

  sprintf(info_str,"KS_SETUP: Device_Identifier %s \n", Device_Identifier);
  KS_DEBUG(0,KS_INFO, info_str);

  tloc_ptr = transport_locator(Device_Identifier);
  if (tloc_ptr == NULL) {
    KS_DEBUG(KS_DISCOVERY_ERROR, KS_RETURN, "TLOC failure Discovering Drive Specified ");
  }  /* end if tloc_ptr check */

  EA_Status = discover(tloc_ptr, &discovery_data);
  if (EA_Status != SUCCESS) {
     /* return EAPI information if we are in debug mode */
     sprintf(info_str,"KS_SETUP:discovery failed: Device Identifier = %s status = %d '%s'\n", \
             Device_Identifier, EA_Status, lastTcgStatusDescription() );
     KS_DEBUG(0,KS_INFO, info_str);

    /* make sure device identifier is NULL for subsequent functions */
    memset(Device_Identifier, 0, DEV_ID_SIZE);
    /* Issue a stack reset command here to clean things up and close the session */
    issueStackReset(NULL);  /* we use NULL as comId per Doug P. */
    return KS_DISCOVERY_ERROR;
  } 
  else {  /* check if device is not in the factory default configuration state */
   /* need to open a session, do check then close session */
     KS_DEBUG(issueStartSession("LockingSP"), KS_INFO, "Start Session Band Configured check");

     configured = bandsConfigured();
     if (configured != 0) {
       /* make sure device identifier is NULL for subsequent functions */
       memset(Device_Identifier, 0, DEV_ID_SIZE);
     }
     sprintf(info_str,"KS_SETUP:Info: configured = %d\n", configured);
     KS_DEBUG(0,KS_INFO, info_str);
     KS_DEBUG(issueCloseSession(), KS_INFO, "Closed Session Band Configured check");
  }

  sprintf(info_str,"KS_SETUP:Success: Device Identifier = %s status = %d configured = %d\n",Device_Identifier, EA_Status, configured);
  KS_DEBUG(0,KS_INFO, info_str);
  return KS_SUCCESS;
}



