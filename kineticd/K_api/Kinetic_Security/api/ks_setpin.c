/*-----------------------------------------------------------------------------
*
*     Header: ks_setpin.c
*     Date: 2014/03/18
*     Author: Chris N Allo
*
*     Description: Encrypt Specified pin and store in Data Store table
* 
*-----------------------------------------------------------------------------*/
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
/*
 * @file ks_setpin.c
 *
 * Implementation 
 * * Description: setting Kinetic Locking/Erase PIN, changing PIN, or resetting PIN will 
 * * secure SED BandMaster/EraseMaster PIN values by wrapping them with a Kinetic PIN using 
 * * NIST SP800-132 key wrap algorithm.
 *
 * Notes:
 *    This API call will try to unwrap PIN value data structures when OldPIN is provided and 
 *    may return an error if unwrap is unsuccessful. 
 *    If NewPIN value is set to null then PINs will be unwrapped.
 *    When setting PIN refresh associated Bandmasters or EraseMaster credentials.
 *    SID is not to be wrapped by any Kinetic PIN. 
 * 
 * @param[in] OLD_Password
 * @param[in] NEW_Password
 * @param[in] Which_Pin  (enum )
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
/* CA 8-28-2014
 * Moved DataStoreData variable out to global level.  as local it was causing a seg fault in the Kinetic use of this function
 * added #ifdef to take out any crypto verbiage during debug modes
*/

/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* From PBKDF functions */
#include "types.h"
#include "sc_pbkdf.h"
#include "aes_gcm.h"
#include "sc_aes.h"
#include "sc_aescrypt.h"
#include "sc_hmac.h"
#include "sc_sha2.h"

/* From EAPI functions */
#include "low_level.h"
#include "high_level.h"
#include "parameters.h"
#include "utilities.h"
#include "debug.h"

/* Local functions */
#include "ks_globals.h"
#include "ks_setup.h"
#include "ks_setpin.h"
#include "ks_utilities.h"
#include "ks_debug.h"


/* NOTES:
 * Must run ks_setup function before this call or it will return an error
 *    
 */

static parameters 	lastToSet;
      tcgByteValue      RawDataStore;
   SED_datastore_struct	DataStoreData;
   
ks_status ks_setpin(char *NEW_Password, char *OLD_Password, ks_pin_types which_Pin) {

   int			GCM_Error;
   tcgByteValue		MasterKey;
   tcgByteValue		Retrieved_Pin;
   tcgByteValue		OLD_PW, NEW_PW;
   tcgByteValue		SALT;
   parameters 		getParms;
   parameters 		*toSet;
   char                 info_str[200];
   uint16_t 		Tag_Length;  
   uint32_t 		Pin_Length;
   const uint8          PBKDF_NULL[] = "";
//   tcgByteValue		Temp_val;


   /* We assume ks_setup command was called before this routine that will have done initial questioning
   *  of the drive.  If it hasnt been called the Device_Identifier string will be NULL and we should quit.
   */
   
   /* Device Identifier is set by ks_security, if that hasnt been called it will be NULL */   
   if (Device_Identifier == NULL) {
     /* we cant continue since ks_setup has not been run. it loads discovery data structure */
     KS_DEBUG(KS_SYSTEM_SETUP_FAILURE, KS_RETURN, "KS_SETPIN:General Failure: Device Identifier = NULL  Need to run KS_SETUP ");
   }

   if (Debug_Info == 2) { 
     /* if debug set to 2 we leave after we enter */
     KS_DEBUG(KS_DEBUG_EXIT, KS_RETURN, "KS_SETPIN:Debug Level 2 [Immediate Return]");
   }
 
   /* get DataStore information Load DataStoreData structure */
   /* we need to get the whole thing because once we get our new value we have to store the whole thing as a whole */

   /* open a session */
   KS_DEBUG(issueStartSession("LockingSP"),KS_INFO,"Start Session Locking SP");
   
   /* Start a transaction */
   KS_DEBUG(issueStartTransaction(), KS_CLOSE, "Start Transaction Completed");

   /* clear parameters */
   resetParameters(&getParms);

   /* Need to read the DataStore on the drive and get the BandMaster[Band_Number] PIN */
   KS_DEBUG(issueGet(tableNameToUID("DataStore"), NULL, &getParms),KS_CLOSE,"Get Data Store info Completed"); 

   if (countParameters(&getParms) != 1) {
      KS_DEBUG (KS_GENERAL_FAILURE, KS_ABORT, "Get Failed: DataStore did not return 1");
   }

   /* unload parameters into Raw Data Store String */
   KS_DEBUG(getByteValue(&getParms, &RawDataStore), KS_ABORT, "Get Byte Value ");

   KS_DEBUG(Load_DataStoreStructure(&DataStoreData,&RawDataStore), KS_ABORT, "Loaded Data Store Structure ");

   tcgByteValueFromString(&OLD_PW, OLD_Password);
   tcgByteValueFromString(&NEW_PW, NEW_Password);
 
   sprintf(info_str,"  OLD PW = %s\n ",tcgByteValueDebugAllHexStr(&OLD_PW) );
   KS_DEBUG (0, KS_INFO, info_str);
   sprintf(info_str,"  NEW_PW = %s\n ",tcgByteValueDebugAllHexStr(&NEW_PW) );
   KS_DEBUG (0, KS_INFO, info_str);

 
   /* Determine which type of pin we need to encrypt. We have currently 2 passwords the user can enter: Locking and Erase */
   /* Erase is used for the erasemaster and locking is used for BandMaster 1.  all other pins use a NULL password for this version */
   switch (which_Pin) {
     case  KS_ERASE_PIN: {
       KS_DEBUG(0, KS_INFO, "PIN Selected is Erasemaster ");
   
       /* Do Erasemaster Set Pin */
       MasterKey.len = PIN_SIZE;
       KS_DEBUG((PBKDF( (uint8 *)OLD_PW.data, 
                        (uint32 )OLD_PW.len,
                        (uint8*)DataStoreData.SaltEM,
                        (uint32 )SALT_SIZE,
                        (uint32 )ITERATION_COUNT, \
                        (uint32 )MasterKey.len, 
                        (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated Master Key to decrypt Erasemaster");

       /* now use Master Key with TAG to decrypt EraseMasterPin  */

       GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.EraseMasterPin,PIN_SIZE,
                                           (uint8 *)PBKDF_NULL,0,
                                           (uint8 *)DataStoreData.TagEM, TAG_SIZE,
                                           (uint8 *)DataStoreData.IV, IV_SIZE, 
                                           (uint8 *)MasterKey.data, MasterKey.len,
                                           (uint8 *)Retrieved_Pin.data,
                                           (uint32 *)&Retrieved_Pin.len );

       /* need to check error special to handle GCM errors differently */
      if ((GCM_Error > 0) || (Retrieved_Pin.len != PIN_SIZE)) {
        if (GCM_Error == 4) 
          sprintf(info_str,"DECRYPTGCM failure SIDPIN [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"DECRYPTGCM failure SIDPIN GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(GCM_Error,KS_ABORT, info_str);
      }

       /* remove masterkey */
       memset(MasterKey.data, 0, MasterKey.len);
       MasterKey.len = PIN_SIZE;

       /* if decryption is successful we can now encrypt the pin with the "NEW_PW" */

       /* first check to see if OLD_PW was NULL.  If it was we need to regenerate a new random pin
        * and put the new pin on the drive.  this new pin will then be encrypted through the code below */
       if (OLD_PW.len == 0)  {
         KS_DEBUG(0,KS_INFO,"OLD Password was NULL ");

         toSet = &lastToSet;  
         resetParameters(toSet);  /* get clean parameter list */

         KS_DEBUG(issueAuthenticate("EraseMaster", &Retrieved_Pin ), KS_ABORT, "Authenticate EraseMaster Return");  

         /* Empty cell_block - cell_block does not apply to Set on Objects */
         setStartList(toSet);
         setEndList(toSet);

         /* get new random pin */
         Retrieved_Pin.len = PIN_SIZE; /* Our pins are always 32 that we generate */
         KS_DEBUG(issueRandom((int)Retrieved_Pin.len,(char *)Retrieved_Pin.data), KS_ABORT,"Created new EM PIN since old PW was NULL"); 

         /* Values - list of list of Column-name/value named-pairs */
         setStartList(toSet);
         setStartList(toSet);
         KS_DEBUG(setNamedByteValue(toSet, tcgTmpName("PIN"), &Retrieved_Pin), KS_ABORT, "Set NamedByte EraseMaster Return");
         setEndList(toSet);
         setEndList(toSet);
  
         /* make command and payload from toset and send */
         KS_DEBUG(issueSet(pinNameToUID("EraseMaster"), toSet), KS_ABORT, "Issue Set EraseMaster Return");
         KS_DEBUG(0,KS_INFO,"Reset Erasemaster Pin on Drive to new random pin");  
       }
  
       /* Now lets Encrypt the Pin and store the information in the data store table */

       /* generate a new SALT */
       KS_DEBUG(issueRandom((int)SALT_SIZE,(char *)SALT.data), KS_ABORT,"Created new SALT");

       MasterKey.len = PIN_SIZE;
       /* generate a master Key (SALT, IV, NEW_PW) */
       KS_DEBUG((PBKDF( (uint8 *)NEW_PW.data, 
                        (uint32 )NEW_PW.len,
                        (uint8*)DataStoreData.SaltEM,
                        (uint32 )SALT_SIZE,
                        (uint32 )ITERATION_COUNT, \
                        (uint32 )MasterKey.len, 
                        (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated New Master Key to encrypt Erasemaster");
  
       /* Now GCM_Encrypt with NEW_PW (master key, IV, unencyrpted_pin) --> TAG, encrypted_pin */
       GCM_Error  = EncryptGCM_Mode( (uint8 *)Retrieved_Pin.data, PIN_SIZE, 
                                     (uint8 *)PBKDF_NULL, (uint32)0, 
                                     (uint8 *)DataStoreData.IV, (uint16)IV_SIZE,
                                     (uint8 *)MasterKey.data, (uint16)MasterKey.len, 
                                     (uint8 *)DataStoreData.EraseMasterPin, (uint32 *)&Pin_Length, 
                                     (uint8 *)DataStoreData.TagEM, (uint16 *)&Tag_Length );

       if ((GCM_Error > 0 ) || (Pin_Length != PIN_SIZE) || (Tag_Length != TAG_SIZE)) {
        if (GCM_Error == 4) 
          sprintf(info_str,"ENCRYPTGCM failure EM [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"ENCRYPTGCM failure EM GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(GCM_Error,KS_ABORT, info_str);
      }


      /* remove masterkey data */
      memset(MasterKey.data, 0, MasterKey.len);
      MasterKey.len = PIN_SIZE;
      memset(Retrieved_Pin.data, 0, Retrieved_Pin.len);
      Retrieved_Pin.len = 0;

      /* put newly encrypted pin on drive associated with Erasemaster */

      /* store SALT, TAG and encrypted value in data store structure */
      /* zero local SALT, TAG and encrypted pin */

      /* Load Data Store string */
      /* Convert data store structure to long string */
      KS_DEBUG(Load_DataStoreString(&RawDataStore, &DataStoreData), KS_ABORT, "Loading Data Store String ");

#ifdef DEBUGCRYPTO
        dumpBuffer(RawDataStore.data, RawDataStore.data+RawDataStore.len);
#endif

      MasterKey.len = PIN_SIZE;
      /* need to use a Bandmaster pin to authenticate to be able to store new entries in Data Store table*/
      /* we have the encrypted value so we need to decrypt the BandMaster0 pin to use for authentication */
      /* bandmaster 0 will always have a NULL password... as of 4/4/2014...                              */
      /* generate a master Key (SALT, IV, NEW_PW) */
      KS_DEBUG((PBKDF( (uint8 *)PBKDF_NULL, 
                       (uint32 )0,
                       (uint8*)DataStoreData.SaltBM[0],
                       (uint32 )SALT_SIZE,
                       (uint32 )ITERATION_COUNT, 
                       (uint32 )MasterKey.len, 
                       (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated New Master Key to Decrypt Bandmaster0");
  
       GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.BandMasterPin[0],PIN_SIZE,
                                           (uint8 *)PBKDF_NULL,0,
                                           (uint8 *)DataStoreData.TagBM[0], TAG_SIZE,
                                           (uint8 *)DataStoreData.IV, IV_SIZE, 
                                           (uint8 *)MasterKey.data, MasterKey.len,
                                           (uint8 *)Retrieved_Pin.data,
                                           (uint32 *)&Retrieved_Pin.len );

       if ((GCM_Error > 0 ) || (Pin_Length != PIN_SIZE) || (Tag_Length != TAG_SIZE)) {
        if (GCM_Error == 4) 
          sprintf(info_str,"DECRYPTGCM failure BM0PIN [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"DECRYPTGCM failure BM0PIN GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(GCM_Error,KS_ABORT, info_str);
      }

      /* remove masterkey data */
      memset(MasterKey.data, 0, MasterKey.len);
      MasterKey.len = PIN_SIZE;
#ifdef DEBUGCRYPTO
/*
    if (Debug_Info >= DEBUG_CRYPTO){
      printf("KS_SETPIN:Debug \n ");

      printf("  Master Key       = %s\n ",tcgByteValueDebugAllHexStr(&MasterKey));

      tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.BandMasterPin[0], (unsigned int) Pin_Length);
      printf(" BM0 Encrypted PIN = %s\n ",tcgByteValueDebugAllHexStr(&Temp_val));

      printf(" BM0 Retrieved PIN = %s\n ",tcgByteValueDebugAllHexStr(&Retrieved_Pin) );

      tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.TagBM[0], TAG_SIZE);    
      printf("  TAG Value        = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );

      tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.SaltBM[0], SALT_SIZE); 
      printf("  SALT Value       = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );

      tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.IV, IV_SIZE); 
      printf("  IV Value         = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );
    }
*/
#endif
      KS_DEBUG(issueAuthenticate("BandMaster0", &Retrieved_Pin ), KS_ABORT, "Authenticate With BandMaster0"); 

      /* Zero out old pin from memory */
      memset(&Retrieved_Pin.data, 0, Retrieved_Pin.len);
      Retrieved_Pin.len = 0;

      /* Put Data store string */
      toSet = &lastToSet;
      resetParameters(toSet);

      setStartList(toSet);
      setNamedIntValue(toSet, tcgTmpName("startRow"), 0);
      setEndList(toSet);

      setByteValue(toSet, &RawDataStore);  
 
      /* make call to load the datastore table */
      KS_DEBUG(issueSet(tableNameToUID("DataStore"), toSet), KS_ABORT, "Issue Set DataStore"); 

      /* zero data store data structure & string */
      memset(&RawDataStore.data, 0, RawDataStore.len);

      KS_DEBUG(issueCommitTransaction(), KS_CLOSE, "Update DataStore Commit Transaction"); 
      KS_DEBUG(issueCloseSession(), KS_INFO, "Update DataStore Close Session");

      break;
    }  /* end case KS_ERASE_PIN  (erasemaster) */
     case  KS_LOCKING_PIN : {
       /* Use LockingSP */
       KS_DEBUG(0, KS_INFO, "PIN Selected LOCKING PIN (BandMaster 1) ");
   
       /* Do Bandmaster1 Set Pin */

       tcgByteValueFromString(&OLD_PW, OLD_Password);

       tcgByteValueFromString(&NEW_PW, NEW_Password);

       MasterKey.len = PIN_SIZE;

       KS_DEBUG((PBKDF( (uint8 *)OLD_PW.data, 
                        (uint32 )OLD_PW.len,
                        (uint8*)DataStoreData.SaltBM[1],
                        (uint32 )SALT_SIZE,
                        (uint32 )ITERATION_COUNT, \
                        (uint32 )MasterKey.len, 
                        (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated Master Key to decrypt BandMaster1");

       /* now use Master Key with TAG to decrypt EraseMasterPin  */

       GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.BandMasterPin[1],PIN_SIZE,
                                           (uint8 *)PBKDF_NULL,0,
                                           (uint8 *)DataStoreData.TagBM[1], TAG_SIZE,
                                           (uint8 *)DataStoreData.IV, IV_SIZE, 
                                           (uint8 *)MasterKey.data, MasterKey.len,
                                           (uint8 *)Retrieved_Pin.data,
                                           (uint32 *)&Retrieved_Pin.len );

       /* need to check error special to handle GCM errors differently */
       if ((GCM_Error > 0) || (Retrieved_Pin.len != PIN_SIZE)) {
        if (GCM_Error == 4) 
          sprintf(info_str,"DECRYPTGCM failure BM1PIN [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"DECRYPTGCM failure BM1PIN GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(GCM_Error,KS_ABORT, info_str);
      }

       /* remove masterkey data */
       memset(MasterKey.data, 0, MasterKey.len);
       MasterKey.len = PIN_SIZE;

       /* if decryption is successful we can now encrypt the pin with the "NEW_PW" */

       /* first check to see if OLD_PW was NULL.  If it was we need to regenerate a new random pin
        * and put the new pin on the drive.  this new pin will then be encrypted through the code below */
       if (OLD_PW.len == 0)  { 

         KS_DEBUG(0,KS_INFO,"Old pin was a NULL we will generate a new random pin and put on drive");

         toSet = &lastToSet;  
         resetParameters(toSet);  /* get clean parameter list */

         KS_DEBUG(issueAuthenticate("BandMaster1", &Retrieved_Pin ), KS_ABORT, "Authenticate BandMaster1 Return");  

         /* Empty cell_block - cell_block does not apply to Set on Objects */
         setStartList(toSet);
         setEndList(toSet);

         /* get new random pin */
         Retrieved_Pin.len = PIN_SIZE; /* Our pins are always 32 that we generate */
         KS_DEBUG(issueRandom((int)Retrieved_Pin.len,(char *)Retrieved_Pin.data), KS_ABORT,"Created new BM1 PIN since old PW was NULL"); 

         /* Values - list of list of Column-name/value named-pairs */
         setStartList(toSet);
         setStartList(toSet);
         KS_DEBUG(setNamedByteValue(toSet, tcgTmpName("PIN"), &Retrieved_Pin), KS_ABORT, "Set NamedByte BandMaster1 Return");
         setEndList(toSet);
         setEndList(toSet);
  
         /* make command and payload from toset and send */
         KS_DEBUG(issueSet(pinNameToUID("BandMaster1"), toSet), KS_ABORT, "Issue Set BandMaster1 Return");
         KS_DEBUG(0,KS_INFO,"Reset BandMaster1 Pin on Drive to new random pin");  
       }
  
       /* Now lets Encrypt the Pin and store the information in the data store table */

       /* generate a new SALT */
       KS_DEBUG(issueRandom((int)SALT_SIZE,(char *)SALT.data), KS_ABORT,"Created new SALT");

       /* generate a master Key (SALT, IV, NEW_PW) */
       KS_DEBUG((PBKDF( (uint8 *)NEW_PW.data, 
                        (uint32 )NEW_PW.len,
                        (uint8*)DataStoreData.SaltBM[1],
                        (uint32 )SALT_SIZE,
                        (uint32 )ITERATION_COUNT, \
                        (uint32 )MasterKey.len, 
                        (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated New Master Key to encrypt Bandmaster1");
  
       /* Now GCM_Encrypt with NEW_PW (master key, IV, unencyrpted_pin) --> TAG, encrypted_pin */
       GCM_Error  = EncryptGCM_Mode( (uint8 *)Retrieved_Pin.data, PIN_SIZE, NULL, 0, 
                                     (uint8 *)DataStoreData.IV, IV_SIZE,
                                     (uint8 *)MasterKey.data, MasterKey.len, 
                                     (uint8 *)DataStoreData.BandMasterPin[1], &Pin_Length, 
                                     (uint8 *)DataStoreData.TagBM[1], &Tag_Length );

       if ((GCM_Error > 0 ) || (Pin_Length != PIN_SIZE) || (Tag_Length != TAG_SIZE)) {
        if (GCM_Error == 4) 
          sprintf(info_str,"ENCRYPTGCM failure BM 1 [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]");
        else
          sprintf(info_str,"ENCRYPTGCM failure BM 1 GCM_Error = %d \n", GCM_Error);
        KS_DEBUG(GCM_Error,KS_ABORT, info_str);
      }

      /* remove masterkey data */
      memset(MasterKey.data, 0, MasterKey.len);
      MasterKey.len = PIN_SIZE;

      /* put newly encrypted pin on drive associated with Erasemaster */

      /* store SALT, TAG and encrypted value in data store structure */
      /* zero local SALT, TAG and encrypted pin */

      /* Load Data Store string */
      /* Convert data store structure to long string */
      KS_DEBUG(Load_DataStoreString(&RawDataStore, &DataStoreData), KS_ABORT, "Loading Data Store String ");

      /* Clear Data Store structure data */
      memset(&DataStoreData, 0, sizeof(DataStoreData));
#ifdef DEBUGCRYPTO
        dumpBuffer(RawDataStore.data, RawDataStore.data+RawDataStore.len);
#endif

      KS_DEBUG(issueAuthenticate("BandMaster1", &Retrieved_Pin ), KS_ABORT, "Authenticate BandMaster1"); 

      /* Zero out old pin from memory */
      memset(&Retrieved_Pin.data, 0, Retrieved_Pin.len);
      Retrieved_Pin.len = 0;

      /* Put Data store string */
      toSet = &lastToSet;
      resetParameters(toSet);

      setStartList(toSet);
      setNamedIntValue(toSet, tcgTmpName("startRow"), 0);
      setEndList(toSet);

      setByteValue(toSet, &RawDataStore);  
 
      /* make call to load the datastore table */
      KS_DEBUG(issueSet(tableNameToUID("DataStore"), toSet), KS_ABORT, "Issue Set DataStore"); 

      /* zero data store data structure & string */
      memset(&RawDataStore.data, 0, RawDataStore.len);

      KS_DEBUG(issueCommitTransaction(), KS_CLOSE, "Update DataStore Commit Transaction"); 
      KS_DEBUG(issueCloseSession(), KS_INFO, "Update DataStore Close Session");

      break;
    }  /* end case KS_LOCKING_PIN  (bandmaster1) */

     default : {
       /* error case */
       KS_DEBUG(1, KS_RETURN, "PIN type entered is invalid ");
       break;
     }
  
   } /*end switch */


  return KS_SUCCESS;
}



