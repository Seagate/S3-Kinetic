/*-----------------------------------------------------------------------------
*
*     Header: ks_erase.c
*     Date: 2014/02/28
*     Author: Chris N Allo
*
*     Description: Erase specified band
* 
* CHANGELIST:
*	7/2/14   changed Debug output handling for EAPI type calls to KS_EAPIABORT
*                instead of KS_ABORT  CA
*-----------------------------------------------------------------------------
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
 
*     
**
 * @file ks_erase.c
 *
 * Implementation Notes:
 * *To be implemented on SED drives but to abstract above the TCG security layer.
 * *This function is expecting an array of integers for the band numbers to be erased
 * *It will loop through each element of the "sizeof" the array or until a non-numeric value 
 * *is found in the array.
 * @param[in] array of band_numbers ie the number of the bands to be erased.
 * @param[in] optional Kinetic_Erase Pin.  (admin password for erase master )
 * @return SUCCESS if all goes well, otherwise the first error encountered.
  *
 */
/* CA 8-28-2014 Per the request of the Kinetic group the functionality of this is being changed slightly
 *   The bandmaster Pin will be changed on the erase and the erasemaster pin will then be reset to NULL
 *   if the erase and bandmaster pin change are successful
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
#include "ks_utilities.h"
#include "ks_erase.h"
#include "ks_setpin.h"
#include "ks_debug.h"

/* From EAPI functions */
#include "low_level.h"
#include "high_level.h"
#include "parameters.h"
#include "utilities.h"
#include "memory_helpers.h"
#include "debug.h"

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
/* THESE WERE TAKEN OUT OF high_level.c */
/** This is the value to be used in a lot of calls if a pin is not provided.
 *  It will be set during discovery and should not be changed by client code.
 */
tcgByteValue msidPin; 

/** The parameters for the last Set method call that was done by a high_level
 *  function.
 */
static parameters 	lastToSet;
  
ks_status ks_erase(int *Band_Number, int Band_Count, char * Kinetic_Erase_Pin) {
/*
 * transactions can only handle one band erase (Seagate Product Requirements limitation, Section 3.5 for Thin Provisioned drives.
1. Retrieve EraseMaster PIN
2. Erase Band(s)
*/


   tcgByteValue		MasterKey;
   int  		index,Band;
   SED_datastore_struct DataStoreData;
   tcgByteValue         RawDataStore;
   char 		indexMaster[16];
   parameters 		getParms;
   parameters 		*toSet;
   int			GCM_Error;
   tcgByteValue		Retrieved_Pin,Generated_Pin;
   uint16_t 		Tag_Length;  
   uint32_t 		Pin_Length;
   tcgByteValue		DataStoreString;
   char                 info_str[200];
   const uint8          PBKDF_NULL[] = "";
#ifdef DEBUGCRYPTO
   tcgByteValue		Temp_Val;
#endif

   /* We assume ks_setup command was called before this routine that will have done initial questioning
   *  of the drive.  If it hasnt been called the Device_Identifier string will be NULL and we should quit.
   */
   
   /* Device Identifier is set by ks_security, if that hasnt been called it will be NULL */   
   if (Device_Identifier == NULL) {
     /* we cant continue since ks_setup has not been run. it loads discovery data structure */
     if (Debug_Info > DEBUG_OFF) 
       KS_DEBUG (0, KS_INFO,"KS_ERASE:General Failure: Device Identifier = NULL  Need to run KS_SETUP ");
     /* Issue a stack reset command here to clean things up and close the session */
     issueStackReset(NULL);  /* we use NULL as comId per Doug P. */
     return KS_SYSTEM_SETUP_FAILURE;
   }

   /* check to see if the Band_Count is 0 or if the Band_Number array is Null  */
   /* If this is the case just return a parameter error to user.  */
   if ((Band_Number == NULL) || (Band_Count <= 0)) {
    KS_DEBUG (0, KS_INFO, "Band Number Array is NULL or Band Count is 0  [Parameter error] ");
    return KS_PARAMETER_ERROR;
   }

   if (Debug_Info == 2) { 
     /* if debug set to 2 we leave after we enter */
     KS_DEBUG (0, KS_INFO, "KS_ERASE:Debug Level 2 [Immediate Return] ");
     /* end transaction and close session */
     return KS_DEBUG_EXIT;
   }

   /* open a session */
   KS_DEBUG(issueStartSession("LockingSP"),KS_INFO,"Start Session Locking SP");
   KS_DEBUG(issueStartTransaction(), KS_CLOSE, "Start Transaction");

   /* Need to read the DataStore on the drive and get the BandMaster[Band_Number] PIN */
   KS_DEBUG(issueGet(tableNameToUID("DataStore"),NULL,&getParms),KS_EAPIABORT,"Get Data Store info"); 

   if (countParameters(&getParms) != 1) {
      KS_DEBUG (1, KS_ABORT, "Get on DataStore did not return 1");
   }

   /* get string into  Raw Data Store String*/
   KS_DEBUG(getByteValue(&getParms, &RawDataStore), KS_EAPIABORT, "Get Byte Value ");

   /* Load the data store structure with data received from the string*/
   KS_DEBUG(Load_DataStoreStructure(&DataStoreData,&RawDataStore), KS_ABORT, "Load Data Store Structure"); 
    
   /***************** unencrypt EraseMaster Pin pin loaded from data store table *******************/

   /* generate a master Key (SALT, IV, new_pin) using the Kinetic erase password passed in*/

   tcgByteValueFromString(&Generated_Pin, Kinetic_Erase_Pin);
   MasterKey.len = PIN_SIZE;

   KS_DEBUG((PBKDF( (uint8 *)Generated_Pin.data, 
                    (uint32 )Generated_Pin.len,
                    (uint8*)DataStoreData.SaltEM,
                    (uint32 )SALT_SIZE,
                    (uint32 )ITERATION_COUNT, \
                    (uint32 )MasterKey.len, 
                    (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated Master Key to decrypt Erasemaster");

   GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.EraseMasterPin,PIN_SIZE,
                                       (uint8 *)PBKDF_NULL,0,
                                       (uint8 *)DataStoreData.TagEM, TAG_SIZE,
                                       (uint8 *)DataStoreData.IV, IV_SIZE, 
                                       (uint8 *)MasterKey.data, MasterKey.len,
                                       (uint8 *)Retrieved_Pin.data, 
                                       (uint32 *) &Retrieved_Pin.len );

     /*typedef enum
     {
        NO_ERROR,                               // 0
        ERROR_PLAINTEXT_TOO_LARGE,              // 1
        ERROR_ASSOCIATED_DATA_TOO_LARGE,        // 2
        ERROR_CYPHERTEXT_TOO_LARGE,             // 3
        ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH, // 4
     } GCM_error;
     */
#ifdef DEBUGCRYPTO
    if (Debug_Info >= DEBUG_CRYPTO){
      printf("KS_ERASE:Debug [Decrypt Erasemaster PIN] completion\n ");

      printf("Retrieved:  Pin Length = %d GCM_ERROR = %d\n", Retrieved_Pin.len, GCM_Error);
      printf("  Master Key        = %s\n ",tcgByteValueDebugAllHexStr(&MasterKey)); 

      printf("  EM Retrieved PIN = %s\n ",tcgByteValueDebugAllHexStr(&Retrieved_Pin) );

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.EraseMasterPin, PIN_SIZE);
      printf("  EM Encrypted PIN = %s\n ",tcgByteValueDebugAllHexStr(&Temp_Val));

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.TagEM, TAG_SIZE);    
      printf("  TAG Value        = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.SaltEM, SALT_SIZE); 
      printf("  SALT Value       = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.IV, IV_SIZE); 
      printf("  IV Value         = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );
    }
#endif

    /* remove masterkey */
    memset(MasterKey.data, 0, MasterKey.len);
    MasterKey.len = PIN_SIZE;
    /* remove generated pin data */
    memset(Generated_Pin.data, 0, Generated_Pin.len);
    Generated_Pin.len = 0;

    /* authenticate once with unecrypted pin */
    KS_DEBUG(issueAuthenticate("EraseMaster", &Retrieved_Pin), KS_EAPIABORT,"Authenticate Band");

    /* Loop erasing all bands specified   */
    for (index = 0; index < Band_Count; index++) {
 
      if (Debug_Info == 3) { 
        /* if debug set to 3 we leave before we erase */
        KS_DEBUG (0, KS_ABORT, "KS_ERASE:Debug Level 3 [Before erase return] ");
        return (KS_DEBUG_EXIT);
      }
     Band = Band_Number[index];

     /* Setup and Call EAPI erase function */
     KS_DEBUG(issueErase(Band),KS_EAPIABORT,"Issue Erase Returned");

     /* Now BandMaster[i]Pin = MSID.  We need generate a new random pin and store a NULL encrypted value in the data store */

     sprintf(indexMaster, "BandMaster%d", Band);

     sprintf(info_str,"Processing %s",indexMaster);
     KS_DEBUG(0,KS_INFO,info_str);

     /* we are setting bandmaster authority for each band we deleted now   the TCG erase changes pins to msidpin */
     KS_DEBUG(issueAuthenticate(indexMaster, &msidPin ), KS_EAPIABORT, "Issue Authenticate BandMasterX Return");  /* Use MSID */  

     /********************  encrypt pin with NULL and store in data store structure  **********/
     /* generate a new SALT */
       KS_DEBUG(issueRandom((int)SALT_SIZE,(char *)DataStoreData.SaltBM[Band]), KS_EAPIABORT,"Created new SALT for BandMaster[X]");

       /* generate a new random pin */
       Generated_Pin.len = PIN_SIZE; /* Our pins are always 32 that we generate */
       KS_DEBUG(issueRandom((int)Generated_Pin.len, (char *)Generated_Pin.data), KS_EAPIABORT, "Issue Random New BandMaster[x]Pin  Return"); 

       /* generate a master Key (SALT, IV, new_pin) using a NULL password*/
       KS_DEBUG((PBKDF( (uint8 *)PBKDF_NULL, 
                        (uint32 )0,
                        (uint8* )DataStoreData.SaltBM[Band],
                        (uint32 )SALT_SIZE,
                        (uint32 )ITERATION_COUNT, 
                        (uint16 )MasterKey.len, 
                        (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated NEW Master Key BandMaster[X]"); 

       /* Now GCM_Encrypt with new_pin (NULL PW) (master key, IV, unencyrpted_pin) --> TAG, encrypted_pin */
       GCM_Error  = EncryptGCM_Mode( (uint8 *)Generated_Pin.data, PIN_SIZE, NULL, 0, 
                                     (uint8 *)DataStoreData.IV, IV_SIZE,
                                     (uint8 *)MasterKey.data, MasterKey.len, 
                                     (uint8 *)DataStoreData.BandMasterPin[Band], &Pin_Length, 
                                     (uint8 *)DataStoreData.TagBM[Band], &Tag_Length );

       if ((GCM_Error > 0 ) || (Pin_Length != PIN_SIZE) || (Tag_Length != TAG_SIZE)) {
        if (GCM_Error == 4) 
          sprintf(info_str,"ENCRYPTGCM failure BM %d [ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH]",Band);
        else
          sprintf(info_str,"ENCRYPTGCM failure BM %d GCM_Error = %d ", Band, GCM_Error);
        KS_DEBUG(GCM_Error,KS_ABORT, info_str);
      }

#ifdef DEBUGCRYPTO
    if (Debug_Info >= DEBUG_CRYPTO){
      printf (" BandMaster # %d index of BandCount = %d\n", Band, index+1);

      printf("  Master Key        = %s\n ",tcgByteValueDebugAllHexStr(&MasterKey)); 
      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.TagBM[Band], TAG_SIZE);    
      printf("  TAG Value        = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.SaltBM[Band], SALT_SIZE); 
      printf("  SALT Value       = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );

      // Print before and after PIN 
      tcgByteValueFromStringCount(&Temp_Val,(char *)DataStoreData.BandMasterPin[Band], (unsigned int) Pin_Length); 
      printf("  Raw BM PIN        = %s\n ",tcgByteValueDebugAllHexStr(&Generated_Pin) );
      printf("  Encrypted BM PIN  = %s\n ",tcgByteValueDebugAllHexStr(&Temp_Val) );
    }
#endif
       /* data store structure should have BandMasterPin, SaltBM, TagBM and IV loaded for BandMaster[X] */

       /* remove masterkey */
       memset(MasterKey.data, 0, MasterKey.len);
       MasterKey.len = PIN_SIZE;

     /**************  Encrypt BandMasterPin[X] Completed  ********************/


     /* clear the to set since we are going to do an issue set */
     toSet = &lastToSet;  
     resetParameters(toSet);

     /* Empty cell_block - cell_block does not apply to Set on Objects */
     setStartList(toSet);
     setEndList(toSet);

     /* Values - list of list of Column-name/value named-pairs */
     setStartList(toSet);
     setStartList(toSet);
     KS_DEBUG(setNamedByteValue(toSet, tcgTmpName("PIN"), &Generated_Pin), KS_EAPIABORT, "SetNamedByte BandMasterX Return");
     setEndList(toSet);
     setEndList(toSet);

     /* set new random pin for this BandMaster */
     KS_DEBUG(issueSet(pinNameToUID(indexMaster), toSet), KS_EAPIABORT, "Issue Set BandMasterX Return");   

     /* at this point we should have BandMaster[X] pin in the datastore back to a NULL encrypted random pin*/
     /* we will need to update the data store to reflect this */
     /* continue until we have erased all the requested bands */

     /* remove generated pin data */
     memset(Generated_Pin.data, 0, Generated_Pin.len);
     Generated_Pin.len = 0;

   } /* end for */ 

   /******************* Now Lets Reset the Erasemaster Pin to the Initialized value "" ************************************/
   
   /* get random pin */
   Generated_Pin.len = PIN_SIZE; /* Our pins are always 32 that we generate */
   KS_DEBUG(issueRandom((int)Generated_Pin.len, (char *)Generated_Pin.data), KS_EAPIABORT, "Issue Random EraseMaster Return");  

   /* We should still be authenticated to the Original Erasemaster Pin (the one it had when we came into this routine ) */
   /* we now want to generate a new pin and NULL encrypt it, then store it in the DataStore table */
   toSet = &lastToSet;  
   resetParameters(toSet);  /* get clean parameter list */
 
   /* old pin is MSID */
  // Dont Need KS_DEBUG(issueAuthenticate("EraseMaster", &msidPin ), KS_EAPIABORT, "Authenticate EraseMaster Return");  

   /* Empty cell_block - cell_block does not apply to Set on Objects */
   setStartList(toSet);
   setEndList(toSet);

   /* Values - list of list of Column-name/value named-pairs */
   setStartList(toSet);
   setStartList(toSet);
   KS_DEBUG(setNamedByteValue(toSet, tcgTmpName("PIN"), &Generated_Pin), KS_EAPIABORT, "Set NamedByte EraseMaster Return");
   setEndList(toSet);
   setEndList(toSet);
  
   /* make command and payload from toset and send */
   KS_DEBUG(issueSet(pinNameToUID("EraseMaster"), toSet), KS_EAPIABORT, "Issue Set EraseMaster Return"); 

 
   /***************  Encrypt EraseMaster Pin Start  *********************/

     /* Encrypt EraseMaster pin with NULL password and store encrypted pin, SALT and TAG in data store structure */

     /* generate a new SALT */
     KS_DEBUG(issueRandom((int)SALT_SIZE,(char *)DataStoreData.SaltEM), KS_EAPIABORT,"Created new SALT for EraseMaster");

     MasterKey.len = PIN_SIZE;

     KS_DEBUG((PBKDF( (uint8 *)PBKDF_NULL, 
                      (uint32 )0,
                      (uint8*)DataStoreData.SaltEM,
                      (uint32 )SALT_SIZE,
                      (uint32 )ITERATION_COUNT, 
                      (uint16 )MasterKey.len, 
                      (uint8 *)MasterKey.data )-1), KS_ABORT, "Generated NEW Master Key EM");

    KS_DEBUG(0, KS_INFO, "KS_INITIALIZE:EraseMaster [Master Key] completion");

    /* Now GCM_Encrypt with new_pin (master key, IV, unencyrpted_pin) --> TAG, encrypted_pin */
    GCM_Error  = EncryptGCM_Mode( (uint8 *)Generated_Pin.data, PIN_SIZE, 
                                  (uint8 *)PBKDF_NULL, 0, \
                                  (uint8 *)DataStoreData.IV, IV_SIZE,\
                                  (uint8 *)MasterKey.data, MasterKey.len, \
                                  (uint8 *)DataStoreData.EraseMasterPin, &Pin_Length, \
                                  (uint8 *)DataStoreData.TagEM, &Tag_Length );

     if ((GCM_Error > 0 ) || (Pin_Length != PIN_SIZE) || (Tag_Length != TAG_SIZE)) {
       KS_DEBUG(GCM_Error, KS_GCMABORT, "ENCRYPTGCM failure EraseMaster");
     }

     /* data store structure should have EraseMasterPin, SaltEM, TagEM and IV loaded */
#ifdef DEBUGCRYPTO
    if (Debug_Info >= DEBUG_CRYPTO){
      printf("KS_INITIALIZE:Debug [Set Erasemaster PIN] completion\n ");
     
      printf("  EraseMaster PIN = %s\n ",tcgByteValueDebugAllHexStr(&Generated_Pin) );

      printf("  Master Key       = %s\n ",tcgByteValueDebugAllHexStr(&MasterKey));

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.EraseMasterPin, (unsigned int) Pin_Length);
      printf("  EM Encrypted PIN = %s\n ",tcgByteValueDebugAllHexStr(&Temp_Val));

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.TagEM, TAG_SIZE);    
      printf("  TAG Value        = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.SaltEM, SALT_SIZE); 
      printf("  SALT Value       = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );

      tcgByteValueFromStringCount(&Temp_Val, (char *)DataStoreData.IV, IV_SIZE); 
      printf("  IV Value         = %s \n",tcgByteValueDebugAllHexStr(&Temp_Val) );
    }

#ifdef Decrypt_TEST_ON
       GCM_Error = DecryptGCM_Mode_Wrapper((uint8 *)DataStoreData.EraseMasterPin,PIN_SIZE,(uint8 *)NULL,0,\
                                           (uint8 *)DataStoreData.TagEM, TAG_SIZE,\
                                           (uint8 *)DataStoreData.IV, IV_SIZE, (uint8 *)MasterKey.data, MasterKey.len,\
                                           (uint8 *)Retrieved_Pin.data, &Retrieved_Pin.len );
    if (Debug_Info > DEBUG_OFF){
      printf("Retrieved:  Pin Length = %d  Tag_length = %d \n", Pin_Length, Tag_Length);
      printf("KS_INITIALIZE:Debug [Decrypt Erasemaster PIN] completion\n ");
      printf("  EM Retrieved PIN = %s\n ",tcgByteValueDebugAllHexStr(&Retrieved_Pin) );
      tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.EraseMasterPin, (unsigned int) Pin_Length); 
      printf("  EM Encrypted PIN = %s\n ",tcgByteValueDebugAllHexStr(&Temp_val)); 
      tcgByteValueFromStringCount(&Temp_val, (char *)DataStoreData.TagEM, (unsigned int) Tag_Length);   
      printf("  TAG Value        = %s \n",tcgByteValueDebugAllHexStr(&Temp_val) );
    }
#endif
#endif 

     /* remove masterkey, generated pin data */
     memset(MasterKey.data, 0, MasterKey.len);
     MasterKey.len = PIN_SIZE;
     memset(Generated_Pin.data, 0, Generated_Pin.len);
     Generated_Pin.len = 0;

   /**************  Encrypt EraseMaster Pin Completed  ********************/

   /******************* Reset EraseMaster PIN Completed *******************************************/


  /************************  Store DataStore Values Start *****************************************/

  /* Prepare Datastore structure */
  /* put datastore data into a serial byte stream */
  /* Write Datastore structure (Store new Bandmaster and Erasemaster Pins */
   
   KS_DEBUG(Load_DataStoreString(&DataStoreString, &DataStoreData), KS_ABORT, "Loading Data Store String ");
 
#ifdef DEBUGCRYPTO
     dumpBuffer(DataStoreString.data, DataStoreString.data+DataStoreString.len);
#endif

   toSet = &lastToSet;
   resetParameters(toSet);

   setStartList(toSet);
   setNamedIntValue(toSet, tcgTmpName("startRow"), 0);
   setEndList(toSet);

   setByteValue(toSet, &DataStoreString);  

   /* only write if at least 1 pin has changed */

   /* make call to load the datastore table */
   KS_DEBUG(issueSet(tableNameToUID("DataStore"), toSet), KS_EAPIABORT, "IssueSet DataStore Return"); 

   /* Initialize DataStoreData area to all 0's */
   memset(&DataStoreData, 0, sizeof(DataStoreData));
   memset(DataStoreString.data, 0, sizeof(DataStoreString.data ));

  /************************  Store DataStore Values Completed ********************************/

   KS_DEBUG(issueCommitTransaction(), KS_CLOSE, "Update DataStore Commit Transaction"); 
   /* Close session */
   KS_DEBUG(issueCloseSession(),KS_INFO,"Close ERASE Bands Session");


  return KS_SUCCESS;
}



