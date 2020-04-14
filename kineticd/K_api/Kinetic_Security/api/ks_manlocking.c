/*-----------------------------------------------------------------------------
*
*     Header: ks_manlocking.c
*     Date: 2014/03/28
*     Author: doug philips /Chris N Allo
*
*     Description: Manage locking
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

 * Changed Lock status to return actual bit values for rle,rl,wle and wl in a single
 *  decimal value for Kinetic.  8/15/2014 CA
 * Changed lock setting method and enum in .h file so that user can set all 5 possible
 *  values by additive bits
*     
**
 * @file ks_manlocking.c
 *
 * Implementation Notes:
 *    Kinetic Locking Pin entered will only work for Band Master 1 at this time
 *
 * Manage locking for a band.
 *
 * @param[in] BandNumber to operate on.
 * @param[in] operation to perform.
 *   Operation values are:
 *          locking_status = 1
 *          locking_enable = 2
 *          locking_disable = 3
 *          locking_lock = 4 
 *          locking_unlock = 5
 *          lock_on_powercycle_enable = 6
 *          lock_on_powercycle_disable = 7
 * @param[in] BAND_Password needed for all operations except status (for status it is ignored).
 * @param[out] lock_status 
 *   The function always returns the drive lock status as a single bit encoded integer as follows:
 *      Read_Write_Lock_Enable = 0x02,
 *      Read_Write_Lock = 0x04,
 *      Lock_On_Powercycle = 0x08
 *
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 * NOTE: If there is an error with the status operation, the value pointed to by *lock_status* may or
 *       may not be changed.
 * */


/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Local functions */
#include "ks_globals.h"
#include "ks_setup.h"
#include "ks_manlocking.h"
#include "ks_utilities.h"
#include "ks_debug.h"

/* From EAPI functions */
#include "low_level.h"
#include "high_level.h"
#include "parameters.h"
#include "utilities.h"
#include "memory_helpers.h"
#include "debug.h"
#include "tcg_constants.h"

/* From PBKDF functions */
#include "types.h"
#include "sc_pbkdf.h"
#include "aes_gcm.h"
#include "sc_aes.h"
#include "sc_aescrypt.h"
#include "sc_hmac.h"
#include "sc_sha2.h"

/* NOTES:
 * Must run ks_setup function before this call or it will return an error
 *    
 */

discovered_data discovery_data;
/** The parameters for the last Set method call that was done by a high_level
 *  function.
 */
static parameters lastToSet;
  
ks_status ks_manlocking(int BandNumber,
                            locking_operation operation,
                            char * BAND_Password,
                            uint8_t *lock_status) {

  uint64_t 		rle,wle,wl,rl,lopc;
  int			GCM_Error;
  tcgByteValue		MasterKey;
  tcgByteValue		Retrieved_Pin;
  tcgByteValue		BAND_PW;
  parameters 		getParms;
  parameters 		*toSet;
  const uint8          	PBKDF_NULL[] = "";
  SED_datastore_struct	DataStoreData;
  tcgByteValue 		RawDataStore;
#ifdef INTERNAL_DEBUG
  tcgByteValue		Temp_val;
#endif
  char                  info_str[200];

  if ((BandNumber < 0) || (BandNumber >= MAX_SETBANDS)) {
    KS_DEBUG(KS_PARAMETER_ERROR,KS_RETURN, "Band Number out of Range ");
  }

  if ((operation == locking_status) && (lock_status == NULL)) {
    KS_DEBUG(KS_PARAMETER_ERROR,KS_RETURN, "Lock Status Requested but Lock Status is NULL ");
  }

  if ((operation != locking_status) && (BAND_Password == NULL)) {
    KS_DEBUG(KS_PARAMETER_ERROR,KS_RETURN, "Set Lock Requested but Locking Pin is NULL ");
  }

   /* Device Identifier is set by ks_security, if that hasnt been called it will be NULL */   
   if (Device_Identifier == NULL) {
     /* we cant continue since ks_setup has not been run. it loads discovery data structure */
     KS_DEBUG(KS_SYSTEM_SETUP_FAILURE, KS_APIRETURN,"KS_MANLOCKING:General Failure: Device Identifier not set:  Need to call KS_SETUP ");
   }

   if (Debug_Info == 2) {
     /* if debug set to 2 we leave after discovery */
     /* print discovery data here before we leave */
     printDiscoveryData(&discovery_data);
     KS_DEBUG(0,KS_INFO,"KS_MANLOCKING:Debug Level 2 [Discovery] completion");
     return KS_DEBUG_EXIT;
   }

  if (operation == locking_status)
   {
      KS_DEBUG(getBandLockingEnterprise(BandNumber, &rle, &wle, &rl, &wl, &lopc), KS_RETURN, "Get Band Locking Completed"); 

     /* CA changed lock status to return a coded value to incorporate all Xle,Xl,lopc values to be returned */
      *lock_status = 0 + (rle << 1) + (rl << 2) + (lopc << 3);
      sprintf(info_str,"Lock Bits are: rle = %d, wle = %d, rl = %d, wl = %d lopc = %d lock_status = %x\n", (int)rle,(int)wle,(int)rl,(int)wl,(int)lopc,*lock_status);
      KS_DEBUG(0,KS_INFO, info_str);
      return (KS_SUCCESS);  /* exit function we are done */
   } 

  /* Initialize Enable and Lock flags */
  rle=wle=wl=rl=2;  /* no change flag */

  switch (operation) {
    case locking_status:{ /* 1 */
    /* NOTE: For Enterprise drives, read and write locks be kept in sync.
     * But let's make as few assumptions as possible. If either read or write
     * are locked and enabled, then the band is locked from a Kinetic perspective.
     */
      KS_DEBUG(getBandLockingEnterprise(BandNumber, &rle, &wle, &rl, &wl, &lopc), KS_RETURN, "Get Band Locking Completed"); 
      /* *lock_status = ((rle && rl) || (wle && wl));*/
     /* CA changed lock status to return a coded value to incorporate all rle,rl,wle,wl values to be returned */
      *lock_status = rle + (rl << 1) + (wle << 2) + (wl << 3) + (lopc << 4);
      return (*lock_status);  /* exit function we are done */
      break;
    }
    case locking_enable: { 
      rle = wle = 1;
      rl = wl = 2;  /* 2 is used just as a flag here*/
      break;
    }
    case locking_disable:{ 
      rle = wle = 0;
      rl = wl = 2;
      break;
    }
    case locking_lock:  { 
      rl = wl = 1;
      rle = wle = 2;
      break;
    }
    case locking_unlock: { 
      rl = wl = 0;
      rle = wle = 2;
      break;
    }
    case lock_on_powercycle_enable:
    case lock_on_powercycle_disable: { 
      /* do nothing, it is handled later */
      break;
    }
    default: {
       KS_DEBUG(KS_PARAMETER_ERROR,KS_RETURN, "Operation Specified is not supported ");
    }
  } /* end switch */

  /* If we got here we have work to do.....  */

  /* open a new session under LOCKING SP and start a new transaction */
  KS_DEBUG(issueStartSession("LockingSP"), KS_RETURN, "Start Session LockingSP");

  /* Start a transaction */
  KS_DEBUG(issueStartTransaction(), KS_CLOSE, "Start Transaction Completed");

  /* clear parameters */
  resetParameters(&getParms);

  /* Need to read the DataStore on the drive and get the BandMaster[BandNumber] PIN */
  KS_DEBUG(issueGet(tableNameToUID("DataStore"),NULL,&getParms),KS_CLOSE,"Get Data Store info Completed"); 

  if (countParameters(&getParms) != 1) {
     KS_DEBUG (KS_GENERAL_FAILURE, KS_ABORT, "Get Failed: DataStore did not return 1");
  }

  /* unload parameters into Raw Data Store String */
  KS_DEBUG(getByteValue(&getParms, &RawDataStore), KS_ABORT, "Get Byte Value ");

  KS_DEBUG(Load_DataStoreStructure(&DataStoreData,&RawDataStore), KS_ABORT, "Loaded Data Store Structure ");

  tcgByteValueFromString(&BAND_PW, BAND_Password);

  MasterKey.len = PIN_SIZE;
  /* need to use a Bandmaster pin to authenticate to be able to store new entries in Data Store table*/
  /* we have the encrypted value so we need to decrypt the BandMaster[x] pin to use for authentication */
  /* generate a master Key (SALT, IV, NEW_PW) */
  KS_DEBUG((PBKDF( (uint8 *)BAND_PW.data, 
                   (uint32 )BAND_PW.len,
                   (uint8 *)DataStoreData.SaltBM[BandNumber],
                   (uint32 )SALT_SIZE,
                   (uint32 )ITERATION_COUNT, 
                   (uint32 )MasterKey.len, 
                   (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated New Master Key to Decrypt BandmasterX");

  GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.BandMasterPin[BandNumber],PIN_SIZE,
                                      (uint8 *)PBKDF_NULL,0,
                                      (uint8 *)DataStoreData.TagBM[BandNumber], TAG_SIZE,
                                      (uint8 *)DataStoreData.IV, IV_SIZE, 
                                      (uint8 *)MasterKey.data, MasterKey.len,
                                      (uint8 *)Retrieved_Pin.data,
                                      (uint32 *)&Retrieved_Pin.len );

  if ((GCM_Error > 0) || (Retrieved_Pin.len != PIN_SIZE)) {
    if (GCM_Error == 4) 
      sprintf(info_str,"DECRYPTGCM failure BM PIN %d [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]", BandNumber);
    else
      sprintf(info_str,"DECRYPTGCM failure BM PIN %d GCM_Error = %d \n", BandNumber, GCM_Error);
   KS_DEBUG(GCM_Error,KS_ABORT, info_str);
  }
#ifdef INTERNAL_DEBUG
  if (Debug_Info >= DEBUG_CRYPTO){
    printf("KS_MANAGELOCKING:Debug  BandNumber = %d \n ",BandNumber);
    printf("  Master Key     = %s\n ",tcgByteValueDebugAllHexStr(&MasterKey));

    tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.BandMasterPin[BandNumber], PIN_SIZE);
    printf("  BM[%d] Enc PIN = %s\n ",BandNumber,tcgByteValueDebugAllHexStr(&Temp_val));

    printf("  BM[%d] Ret PIN = %s\n ",BandNumber,tcgByteValueDebugAllHexStr(&Retrieved_Pin) );

    tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.TagBM[BandNumber], TAG_SIZE);    
    printf("  TAG Value      = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );

    tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.SaltBM[BandNumber], SALT_SIZE); 
    printf("  SALT Value     = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );

    tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.IV, IV_SIZE); 
    printf("  IV Value       = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );
  }
#endif

  /* remove masterkey */
  memset(MasterKey.data, 0, MasterKey.len);
  MasterKey.len = 0;

  /* at this point we have the unecrypted pin of the Band we are trying to change the locking on */
  toSet = &lastToSet; 
  resetParameters(toSet);

  /* Empty cell_block - cell_block does not apply to Set on Objects */
  setStartList(toSet);
  setEndList(toSet);

  /* Values - list of list of Column-name/value named-pairs */
  setStartList(toSet);
  setStartList(toSet);
  /* We must pass the column's values to the Set method in the order they
     occur in the table in order to set all of them in one method call.
  */

  /* For Enterprise drives and TCG1 and TCG2 we have to set Both Read and Write at the same
     time for enable and lock changes.  This will need to be changed in future releases when 
     individual control will be incorporated.  */

  if (rle != 2) {
    setNamedIntValue(toSet, tcgTmpName("ReadLockEnabled"), rle);
  }
  if (wle != 2) {
    setNamedIntValue(toSet, tcgTmpName("WriteLockEnabled"), wle);
  }
  if (rl != 2) {
    setNamedIntValue(toSet, tcgTmpName("ReadLocked"), rl);
  }
  if (wl != 2) {
    setNamedIntValue(toSet, tcgTmpName("WriteLocked"), wl);
  }
  if ((operation == lock_on_powercycle_enable) || (operation == lock_on_powercycle_disable))
   {
   /* to set LOR_POWERCYCLE send a LockOnReset list with the LOR_POWERCYCLE value (0)
      to turn it off send the startlist for LockOnReset but with no value */
    setStartName(toSet);
    setByteValue(toSet, tcgTmpName("LockOnReset"));
    setStartList(toSet);
    if (operation == lock_on_powercycle_enable) 
     {
       setIntValue(toSet, LOR_POWERCYCLE);
     }  /* end lock on powercycle */

    setEndList(toSet);
    setEndName(toSet);
   }
   
  /* Complete TCG command */
  setEndList(toSet);
  setEndList(toSet);

  KS_DEBUG(issueAuthenticate(bandNumberToAuthority(BandNumber), &Retrieved_Pin), KS_CLOSE, "Authenticated Band ");
  KS_DEBUG(issueSet(bandNumberToUID(BandNumber), toSet), KS_CLOSE, "Set Band Values as Specified ");
  KS_DEBUG(issueCommitTransaction(), KS_CLOSE, "Commited Transaction ");
  KS_DEBUG(issueCloseSession(), KS_INFO, "Close Session with Locking SP ");

  /* Now lets check to see the value it is set to */
  KS_DEBUG(getBandLockingEnterprise(BandNumber, &rle, &wle, &rl, &wl, &lopc), KS_RETURN, "Check Lock state after set"); 

  /* CA changed lock status to return a coded value to incorporate all rle,rl,wle,wl values to be returned */
  *lock_status = 0 + (rle << 1) + (rl << 2) + (lopc << 3);  /* only need rle and rl since wle and wl follow in TCG2 */
  sprintf(info_str,"After Set rle = %d, wle = %d, rl = %d, wl = %d lopc = %d lock_status = %x\n", (int)rle,(int)wle,(int)rl,(int)wl,(int)lopc,*lock_status);
  KS_DEBUG(0,KS_INFO, info_str);
  return (KS_SUCCESS);
  
}

