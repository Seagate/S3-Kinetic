/*-----------------------------------------------------------------------------
*
*     Header: ks_mandiags.c
*     Date: 2014/03/18
*     Author: Chris N Allo
*
*     Description: Manage Diagnostics
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
**
 * @file ks_mandiags.c
 *
 * Implementation Notes:
 * This function operates in 4 modes :                                             
 *   KS_GET_STATUS:   function will determine status of port and return either     
 *                    PORT_LOCKED (33) or PORT_UNLOCKED (55)

 *   KS_CHALLENGE:    Function will open a session to the LockingSP.  It will retrieve
 *                    the SID pin data from the Datastore area and decrypt the 
 *                    SID PIN then issue an authenticate to the SID PIN. if all is a 
 *                    success to this point the function will issue an authenticate to 
 *                    MakerSymK to get a challenge string. this module will present the 
 *                    returned challenge string to the user with a KS_SUCCESS unless an     
 *                    error occurs. This function also returns a "session info" string
 *                    which will need to be entered on the next function call to
 *                    the ks_mandiags for the KS_RESPONSE task.  
 *                                             
 *   KS_RESPONSE:     Function will issue an authenticate to MakerSymK but use the 
 *                    user input "response" string and the session info string. 
 *                    Upon success it will formulate a command string to unlock the Diag
 *                    port. if all is a success to this point the function will
 *                    return a PORT_UNLOCKED code to the user else an error. 
 *  
 *   KS_CHALLENGE_AND_RESPONSE: 
 *                    Function will open a session to the LockingSP.  It will retrieve
 *                    the SID pin data from the Datastore area and decrypt the 
 *                    SID PIN then issue an authenticate to the SID PIN. If all is a 
 *                    success to this point the function will issue an authenticate to 
 *                    MakerSymK to get a challenge string. this module will present the 
 *                    returned challenge string to the user.  It will then wait for the 
 *                    user to use the TDCI tool and enter the response string returned.
 *                    Upon success it will formulate a command string to unlock the Diag
 *                    port. if all is a success to this point the function will
 *                    return a PORT_UNLOCKED code to the user else an error.              
 * * @param[in] Device Identifier
 * * @param[in] Task  (what to do 0 = get status  1 = challenge  2 = response )
 * * @param[in] Lock  (whether to lock or unlock the port [0 = unlock, 1 = lock])
 * * @param[in/out] challenge or response string
 * * @param[out/in] Session Info string
 * * @return PORT_LOCKED, PORT_UNLOCKED or SUCCESS if all goes well, otherwise the first error encountered.
  *
 */

/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* Local functions */
#include "ks_globals.h"
#include "ks_mandiags.h"
#include "ks_utilities.h"
#include "ks_debug.h"

/* From EAPI functions */
#include "low_level.h"
#include "transport.h"
#include "transport_locator.h"
#include "high_level.h"
#include "tcg_constants.h"
#include "debug.h"

/* From PBKDF functions */
#include "types.h"
#include "sc_pbkdf.h"
#include "aes_gcm.h"
#include "sc_aes.h"
#include "sc_aescrypt.h"
#include "sc_hmac.h"
#include "sc_sha2.h"

static parameters lastToSet;

int ks_mandiags(char *Device_ID, tasks Task, uint64_t lock, char *Challenge_Response, char *Session_Info) {
    transport 		*tloc_ptr;
    status 		EA_Status;
    uint32_t	 	idx;
    int                 port_status;
    tcgByteValue        RawDataStore;
    tcgByteValue        Chall_Resp;
    SED_datastore_struct DataStoreData;
    parameters 		getParms;
    char                info_str[200];
    int			GCM_Error;
    tcgByteValue	Retrieved_Pin;
    tcgByteValue	MasterKey;
    const uint8         PBKDF_NULL[] = "";
    parameters 		*toSet;
    unsigned char	temp[100];
    discovered_data 	discovery_data;
	

  /* default port_status */
  port_status = KS_GENERAL_FAILURE;

 /* setup Device Identifier passed in to main routine */
  strcpy (Device_Identifier, Device_ID);
 
  sprintf(info_str,"KS_MANDIAGS: Device_Identifier %s \n", Device_Identifier);
  KS_DEBUG(0,KS_INFO, info_str);

  /* we only need to do the discovery to see if the port is locked or unlocked      */
  /* if the user is only asking for status or is in the Challenge phase we return after we make the determination */

  if ((Task == KS_GET_STATUS) || (Task == KS_CHALLENGE) || (Task == KS_PAM_CHALLENGE) || (Task == KS_CHALLENGE_AND_RESPONSE)) {
    /* lets make sure we dont have any sessions hanging open*/
    /* Issue a stack reset command here to clean things up and close the session */
    issueStackReset(NULL);
    issueCloseSession();
    
    KS_DEBUG(0, KS_INFO, "Closing any Open sessions");

    KS_DEBUG(0,KS_INFO, "Doing Discovery of Drive to determine Port Status");
    tloc_ptr = transport_locator(Device_Identifier);

    if (tloc_ptr == NULL) {
      KS_DEBUG(KS_DISCOVERY_ERROR, KS_RETURN, "TLOC failure Discovering Drive Specified ");
    }  /* end if tloc_ptr check */


    EA_Status = discover(tloc_ptr, &discovery_data);
    if (EA_Status != SUCCESS) {
      /* return EAPI information if we are in debug mode */
      sprintf(info_str,"KS_MANDIAGS:discovery failed: Device Identifier = %s status = %d '%s'\n", \
                Device_Identifier, EA_Status, lastTcgStatusDescription() );
      KS_DEBUG(0,KS_INFO, info_str);

      /* Issue a stack reset command here to clean things up and close the session */
      issueStackReset(NULL);  /* we use NULL as comId per Doug P. */
      return KS_DISCOVERY_ERROR;
    } 

    /* Look for specific values in discovered data.  if found, return success else return KS_GENERAL_FAILURE 
    * looking for PORT with UID value of 00 01 00 02 00 01 00 
    * since the feature descriptor only uses the last 4 bytes to identify
    * the port, it is expected that the Enterprise API will return 
    * 00 01 00 01 h 
    */

    /* Logical port count should be greater than 0 */
    if ((discovery_data.fdLogicalPortSeen == 1) && (discovery_data.logicalPortCount > 0)) {
      /* loop through each PortID to see if we have this pattern '0x00010001' */
      for (idx = 0; idx < discovery_data.logicalPortCount; idx++) {
         /* show info to user */
         sprintf(info_str," 0x%08x(%d)\n", discovery_data.logicalPortIDs[idx], discovery_data.logicalPortLocked[idx]);
         KS_DEBUG(0,KS_INFO, info_str);
        
       if (discovery_data.logicalPortIDs[idx] == (uint64_t) 0x00010001) {
           if (discovery_data.logicalPortLocked[idx] == 0x1) {
             /* found the Port ID we are looking for and it is locked */
             port_status = PORT_LOCKED;
	     KS_DEBUG(0,KS_INFO,"Diag Port Locked");
           }
           else {
             port_status = PORT_UNLOCKED;
	     KS_DEBUG(0,KS_INFO,"Diag Port Unlocked");
           }
           /* end for loop. we have gotten the info we need */
          break;
         }  /* if port = 0x00010001 */
       } /* end for */
     }
     else {
       /* port count == 0 or logical port seen != 1*/
       return(KS_PORT_ERROR); 
     }

  } /* end get_status or doing challenge */
 
  switch (Task) {
    case KS_GET_STATUS:{ /* 0 */
      /* we are done here.  return to user with Port status */
      return(port_status);
      break;
    } 
    case KS_CHALLENGE: /* 1 */
    case KS_PAM_CHALLENGE: /* 4 */
      {
      /* if the port is already unlocked and they want to unlock we can skip this */
      if ((port_status == PORT_UNLOCKED) && (lock == UNLOCK_PORT)){
        KS_DEBUG(0,KS_INFO, "Challenge and Response skipped...Port is already unlocked");
        break;
      }
      /* if the port is already locked and they want to lock we can skip this */
      if ((port_status == PORT_LOCKED) && (lock == LOCK_PORT)){
        KS_DEBUG(0,KS_INFO, "Challenge and Response skipped...Port is already locked");
        break;
      }

      /* A little debug information here */
      if (lock == LOCK_PORT) {
        KS_DEBUG(0,KS_INFO, "TASK is to LOCK Diagnostics Port");
      }
      else {
       if (lock == UNLOCK_PORT) {
        KS_DEBUG(0,KS_INFO, "TASK is to UNLOCK Diagnostics Port");
       }
       else  /* assume authorize only */
        KS_DEBUG(0,KS_INFO, "TASK is to Authorize");
      }

      /* lets get the data store information and authenticate SID first */
      KS_DEBUG(issueStartSession("LockingSP"),KS_INFO,"Start Session Locking SP");

      /* Need to read the DataStore on the drive and get the SID PIN */
      KS_DEBUG(issueGet(tableNameToUID("DataStore"),NULL,&getParms),KS_ABORT,"Get Data Store info"); 

      if (countParameters(&getParms) != 1) {
         KS_DEBUG (1, KS_CLOSE, "Get on DataStore did not return 1");
      }

      /* get string into  Raw Data Store String*/
      KS_DEBUG(getByteValue(&getParms, &RawDataStore), KS_CLOSE, "Get Byte Value ");

      /* to authenticate to SID we need to close the LockingSP session and reopen as AdminSP */
      KS_DEBUG(issueCloseSession(), KS_INFO, "Closed Session LockingSP");

      /* Load the data store structure with data received from the string*/
      KS_DEBUG(Load_DataStoreStructure(&DataStoreData,&RawDataStore), KS_RETURN, "Load Data Store Structure"); 
    
      /***************** unencrypt SID Pin pin loaded from data store table *******************/

      /* generate a master Key (SALT, IV, new_pin) using the Kinetic erase password passed in*/

      MasterKey.len = PIN_SIZE;
      /* Once the SID pin is no longer encrypted with a NULL password this code will need to change */
      KS_DEBUG((PBKDF( (uint8 *)PBKDF_NULL, 
                       (uint32 )0,
                       (uint8*)DataStoreData.SaltSID,
                       (uint32 )SALT_SIZE,
                       (uint32 )ITERATION_COUNT, \
                       (uint32 )MasterKey.len, 
                       (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated Master Key to decrypt SID Pin");

      GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.SIDPin,PIN_SIZE,
                                          (uint8 *)PBKDF_NULL,0,
                                          (uint8 *)DataStoreData.TagSID, TAG_SIZE,
                                          (uint8 *)DataStoreData.IV, IV_SIZE, 
                                          (uint8 *)MasterKey.data, MasterKey.len,
                                          (uint8 *)Retrieved_Pin.data, 
                                          (uint32 *) &Retrieved_Pin.len );

      if (GCM_Error > 0 )  {
        if (GCM_Error == 4) 
          sprintf(info_str,"DECRYPTGCM failure SIDPIN [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"DECRYPTGCM failure SIDPIN GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(0,KS_RETURN, info_str);
      }

      /* remove masterkey */
      memset(MasterKey.data, 0, MasterKey.len);
      MasterKey.len = PIN_SIZE;
   
      /* need to start an AdminSP session to do all the other vification and port setting */
      KS_DEBUG(issueStartSession("AdminSP"), KS_INFO, "Start Session ADminSP to Authenticate the SID"); 

      /* Authenticate with unecrypted SID pin */
      EA_Status = issueAuthenticate("SID", &Retrieved_Pin) ;

      /* remove generated pin data */
      memset(Retrieved_Pin.data, 0, Retrieved_Pin.len);
      Retrieved_Pin.len = 0;

      /*********** do Challenge first, wait for user input then do response *****************/

      /* authenticate with MakerSymK and NULL response to get Challenge */
      KS_DEBUG(issueAuthenticateCR("MakerSymK", &Chall_Resp, NULL), KS_CLOSE, "CHALLENGE_AND_RESPONSE: Challenge Authenticate Completed") ;

      /* Present Challenge and Session Info */
      hexlify(Chall_Resp.data, temp, 32);
      if (Task == KS_CHALLENGE )
        printf("\nChallenge for TDCITool Key Authentication Drive Challenge:\n Copy and past this string in the TDCI Tool ==>'%s'\n", temp);
     
      /* load these strings so we can use them if we make 2 function calls (needed for PAM ) */ 
      strcpy(Challenge_Response, (char *)temp);
      strcpy(Session_Info, saveState());

      if (Task == KS_CHALLENGE )
        printf(" Save this info to be used as parameter 5 in the Response call ==>'%s'\n",Session_Info);

      /* leave session open to be handled by KS_RESPONSE */
      return (port_status);  /* exit function we are done */
      break;
    } 
    case KS_RESPONSE:{ /* 2 */
      port_status = KS_PORT_ERROR;  /* default to error */
      if (tloc_ptr == NULL)
       { /* we need to get a new tloc_ptr.  This handles the case where this is called from 2 different program stacks */
         /* Session should already be open, Discover and restore session info.   */
         tloc_ptr = transport_locator(Device_Identifier);

         EA_Status = discoverNoMSID(tloc_ptr, &discovery_data);
         if (EA_Status != SUCCESS) {
           sprintf(info_str,"KS_MANDIAGS:discoverNOMSID failed: Device Identifier = %s status = %d '%s'\n", \
                     Device_Identifier, EA_Status, lastTcgStatusDescription() );
           KS_DEBUG(0,KS_INFO, info_str);

           /* Issue a stack reset command here to clean things up and close the session */
           issueStackReset(NULL);  /* we use NULL as comId per Doug P. */
           return KS_DISCOVERY_ERROR;
         } /* end if EA_Status != Success */

         loadState(Session_Info);
       } /* end if tloc_ptr == NULL */

      unhexlify((unsigned char *)Challenge_Response, Chall_Resp.data);
      Chall_Resp.len = strlen((const char *)Chall_Resp.data) ;
      /* make sure we are only 32 long here optional */Chall_Resp.len = 32;
    
      /* Authenticate with MakerSymK and Response */
      KS_DEBUG(issueAuthenticateCR("MakerSymK", NULL, &Chall_Resp),KS_CLOSE, "Authenticate with MakerSymK and Response") ;

      /* remove response data */
      memset(Chall_Resp.data, 0, Chall_Resp.len);
      Chall_Resp.len = 0;

      if (lock != AUTHORIZE_PORT) {
        /* now lets actually lock or unlock the port */
        /*locked = 0;*/
        toSet = &lastToSet;
        resetParameters(toSet);
        /* Empty cell_block - cell_block does not apply to Set on Objects */
        setStartList(toSet);
        setEndList(toSet);
        /* Values - list of list of Column-name/value named-pairs */
        setStartList(toSet);
        setStartList(toSet);
        KS_DEBUG(setNamedIntValue(toSet, tcgTmpName("PortLocked"), (uint64_t) lock),KS_CLOSE,"Set Named Int Value PortLocked ");
        setStartName(toSet);
        KS_DEBUG(setByteValue(toSet, tcgTmpName("LockOnReset")),KS_CLOSE,"Set Byte Value LockOnReset ");
        setStartList(toSet);
        KS_DEBUG(setIntValue(toSet, LOR_POWERCYCLE),KS_CLOSE,"Set Named Int Value LOR_POWERCYCLE ");
        setEndList(toSet);
        setEndName(toSet);
        setEndList(toSet);
        setEndList(toSet);

        EA_Status = issueSet(portNameToUID("Diagnostics"), toSet);
        KS_DEBUG(EA_Status,KS_INFO, "Issue Set Diagnostics Port to Unlock Completed");

        if (EA_Status != SUCCESS) 
        {
           /* return a port error, something happened */
           port_status = KS_PORT_ERROR;
           KS_DEBUG(port_status, KS_INFO, "Port Error:  Cannot Lock/unlock port ");
        }
        else 
        {
           /* we have successfully done a challenge-response and SID authenticate return PORT_UNLOCKED or PORT_LOCKED */
           if (lock == UNLOCK_PORT) 
           {
              port_status = PORT_UNLOCKED;
              KS_DEBUG(0, KS_INFO, "Port Unlocked");
           }
           else 
           {
              port_status = PORT_LOCKED;
              KS_DEBUG(0, KS_INFO, "Port Locked");
           }
        } /* EA_Status */
      } /* if lock or unlock */
      else
        port_status = PORT_AUTHORIZED;

      KS_DEBUG(issueCloseSession(), KS_INFO, "Closed Session AdminSP");
      break;
    } /* end case Response */


    case KS_CHALLENGE_AND_RESPONSE:{ /* 3 */
      /* if the port is already unlocked and they want to unlock we can skip this */
      if ((port_status == PORT_UNLOCKED) && (lock == UNLOCK_PORT)){
        KS_DEBUG(0,KS_INFO, "Challenge and Response skipped...Port is already unlocked");
        break;
      }
      /* if the port is already locked and they want to lock we can skip this */
      if ((port_status == PORT_LOCKED) && (lock == LOCK_PORT)){
        KS_DEBUG(0,KS_INFO, "Challenge and Response skipped...Port is already locked");
        break;
      }

      /* A little debug information here */
      if (lock == LOCK_PORT) {
        KS_DEBUG(0,KS_INFO, "TASK is to LOCK Diagnostics Port");
      }
      else {
        KS_DEBUG(0,KS_INFO, "TASK is to UNLOCK Diagnostics Port");
      }

      /* lets get the data store information and authenticate SID first */
      KS_DEBUG(issueStartSession("LockingSP"),KS_INFO,"Start Session Locking SP");

      /* Need to read the DataStore on the drive and get the SID PIN */
      KS_DEBUG(issueGet(tableNameToUID("DataStore"),NULL,&getParms),KS_ABORT,"Get Data Store info"); 

      if (countParameters(&getParms) != 1) {
         KS_DEBUG (1, KS_CLOSE, "Get on DataStore did not return 1");
      }

      /* get string into  Raw Data Store String*/
      KS_DEBUG(getByteValue(&getParms, &RawDataStore), KS_ABORT, "Get Byte Value ");

      /* to authenticate to SID we need to close the LockingSP session and reopen as AdminSP */
      KS_DEBUG(issueCloseSession(), KS_INFO, "Closed Session LockingSP");

      /* Load the data store structure with data received from the string*/
      KS_DEBUG(Load_DataStoreStructure(&DataStoreData,&RawDataStore), KS_ABORT, "Load Data Store Structure"); 
    
      /***************** unencrypt SID Pin pin loaded from data store table *******************/

      /* generate a master Key (SALT, IV, new_pin) using the Kinetic erase password passed in*/

      MasterKey.len = PIN_SIZE;
      /* Once the SID pin is no longer encrypted with a NULL password this code will need to change */
      KS_DEBUG((PBKDF( (uint8 *)PBKDF_NULL, 
                       (uint32 )0,
                       (uint8*)DataStoreData.SaltSID,
                       (uint32 )SALT_SIZE,
                       (uint32 )ITERATION_COUNT, \
                       (uint32 )MasterKey.len, 
                       (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated Master Key to decrypt SID Pin");

      GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.SIDPin,PIN_SIZE,
                                          (uint8 *)PBKDF_NULL,0,
                                          (uint8 *)DataStoreData.TagSID, TAG_SIZE,
                                          (uint8 *)DataStoreData.IV, IV_SIZE, 
                                          (uint8 *)MasterKey.data, MasterKey.len,
                                          (uint8 *)Retrieved_Pin.data, 
                                          (uint32 *) &Retrieved_Pin.len );
 
      if (GCM_Error > 0 )  {
        if (GCM_Error == 4) 
          sprintf(info_str,"DECRYPTGCM failure SIDPIN [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"DECRYPTGCM failure SIDPIN GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(0,KS_RETURN, info_str);
      }

      /* remove masterkey */
      memset(MasterKey.data, 0, MasterKey.len);
      MasterKey.len = PIN_SIZE;
   
      /* need to start an AdminSP session to do all the other vification and port setting */
      KS_DEBUG(issueStartSession("AdminSP"), KS_INFO, "Start Session ADminSP to Authenticate the SID"); 

      /* Authenticate with unecrypted SID pin */
      EA_Status = issueAuthenticate("SID", &Retrieved_Pin) ;

      /* remove generated pin data */
      memset(Retrieved_Pin.data, 0, Retrieved_Pin.len);
      Retrieved_Pin.len = 0;

      /*********** do Challenge first, wait for user input then do response *****************/

      /* authenticate with MakerSymK and NULL response to get Challenge */
      KS_DEBUG(issueAuthenticateCR("MakerSymK", &Chall_Resp, NULL), KS_CLOSE, "CHALLENGE_AND_RESPONSE: Challenge Authenticate Completed") ;

      /* Present Challenge and ask for Response */
	hexlify(Chall_Resp.data, temp, 32);
	printf("\nChallenge for TDCITool Key Authentication Drive Challenge:\n'%s'\n", temp);
	printf("\nEnter TDCITool Key Authentication Drive Challenge Response: ");
	unhexlify((unsigned char *)fgets((char *)temp,100,stdin), Chall_Resp.data);
	Chall_Resp.len = strlen((const char *)Chall_Resp.data);
	printf("\n");
    
      printf("Checking.........\n");

      /* Authenticate with MakerSymK and Response */
      KS_DEBUG(issueAuthenticateCR("MakerSymK", NULL, &Chall_Resp), KS_CLOSE, "CHALLENGE_AND_RESPONSE: RESPONSE Authenticate Completed") ;

      /* remove response data */
      memset(Chall_Resp.data, 0, Chall_Resp.len);
      Chall_Resp.len = 0;
     
      /* so far so good.....*/
      /* now lets actually lock or unlock the port */

      toSet = &lastToSet;
      resetParameters(toSet);
      /* Empty cell_block - cell_block does not apply to Set on Objects */
      setStartList(toSet);
      setEndList(toSet);
      /* Values - list of list of Column-name/value named-pairs */
      setStartList(toSet);
      setStartList(toSet);
      KS_DEBUG(setNamedIntValue(toSet, tcgTmpName("PortLocked"), (uint64_t) lock),KS_CLOSE,"Set Named Int Value PortLocked ");
      setStartName(toSet);
      KS_DEBUG(setByteValue(toSet, tcgTmpName("LockOnReset")),KS_CLOSE,"Set Byte Value LockOnReset ");
      setStartList(toSet);
      KS_DEBUG(setIntValue(toSet, LOR_POWERCYCLE),KS_CLOSE,"Set Named Int Value LOR_POWERCYCLE ");
      setEndList(toSet);
      setEndName(toSet);
      setEndList(toSet);
      setEndList(toSet);

      EA_Status = issueSet(portNameToUID("Diagnostics"), toSet);
      KS_DEBUG(EA_Status,KS_INFO, "Issue Set Diagnostics Port to Lock/Unlock Completed");

      if (EA_Status != SUCCESS) {
       /* return a port error, something happened */
       port_status = KS_PORT_ERROR;
       KS_DEBUG(port_status, KS_INFO, "Port Error:  Cannot Lock/unlock port ");
      }
      else {
        /* we have successfully done a challenge-response and SID authenticate return PORT_UNLOCKED or PORT_LOCKED */
        if (lock == UNLOCK_PORT) {
          port_status = PORT_UNLOCKED;
          KS_DEBUG(0, KS_INFO, "Port Unlocked");
        }
        else {
          port_status = PORT_LOCKED;
          KS_DEBUG(0, KS_INFO, "Port Locked");
        }
      } /* EA_Status */

      KS_DEBUG(issueCloseSession(), KS_INFO, "Closed Session AdminSP");
      break;
    } 
    default: {
       KS_DEBUG(KS_PARAMETER_ERROR,KS_RETURN, "Operation Specified is not supported ");
    }
  } /* end switch */
  
 return(port_status);

}

