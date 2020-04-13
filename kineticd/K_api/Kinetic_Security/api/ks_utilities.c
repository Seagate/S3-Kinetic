/*-----------------------------------------------------------------------------
*
*     Header: ks_utilities.c
*     Date: 2014/03/28
*     Author: Chris N Allo
*
*     Description: Utility functions used in libksapi
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
 * @file ks_utilities.c
 *
 * Implementation Notes:
 *
 */

/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local functions */
#include "ks_globals.h"

/* From EAPI functions */
#include "parameters.h"


/* Load data store structure with values from the data store string */ 
ks_status Load_DataStoreStructure(SED_datastore_struct *DataStoreData, tcgByteValue *RawDataStore) {

   int  		BandNumber;
   int			stroffset;

   /* Initialize DataStoreData area to all 0's */
   memset(DataStoreData, 0, sizeof(SED_datastore_struct));

   /* Load the data store structure from rawdatastring */
   
   stroffset = 0;
   memcpy(DataStoreData->Header, RawDataStore->data, HEADER_SIZE);
   stroffset += HEADER_SIZE;

   memmove(DataStoreData->IV,(RawDataStore->data)+stroffset, IV_SIZE); 
   stroffset += IV_SIZE;

   memmove(DataStoreData->SaltSID, (RawDataStore->data)+stroffset, SALT_SIZE);
   stroffset += SALT_SIZE;

   memmove(DataStoreData->TagSID,RawDataStore->data+stroffset, TAG_SIZE); 
   stroffset += TAG_SIZE;

   memmove(DataStoreData->SIDPin, RawDataStore->data+stroffset, PIN_SIZE);
   stroffset += PIN_SIZE;

   memmove(DataStoreData->SaltEM, RawDataStore->data+stroffset, SALT_SIZE);
   stroffset += SALT_SIZE;

   memmove(DataStoreData->TagEM,RawDataStore->data+stroffset, TAG_SIZE); 
   stroffset += TAG_SIZE;

   memmove(DataStoreData->EraseMasterPin, RawDataStore->data+stroffset, PIN_SIZE);
   stroffset += PIN_SIZE;

   for (BandNumber = 0; BandNumber < MAX_SETBANDS; BandNumber++) {

     memmove(DataStoreData->SaltBM[BandNumber], RawDataStore->data+stroffset, SALT_SIZE);
     stroffset += SALT_SIZE;

     memmove(DataStoreData->TagBM[BandNumber],RawDataStore->data+stroffset, TAG_SIZE); 
     stroffset += TAG_SIZE;

     memmove(DataStoreData->BandMasterPin[BandNumber],RawDataStore->data+stroffset, PIN_SIZE );
     stroffset += PIN_SIZE;
   }

   memmove(DataStoreData->SaltBM915, RawDataStore->data+stroffset, SALT_SIZE);
   stroffset += SALT_SIZE;

   memmove(DataStoreData->TagBM915,RawDataStore->data+stroffset, TAG_SIZE); 
   stroffset += TAG_SIZE;

   /* finish BandMasters 9 through 15 */
   for (BandNumber=MAX_SETBANDS; BandNumber < MAX_BAND_SIZE; BandNumber++)
    {
      memmove(DataStoreData->BandMasterPin915[BandNumber],RawDataStore->data+stroffset, PIN_SIZE );
      stroffset += PIN_SIZE;
    }

  RawDataStore->len = stroffset;

  return KS_SUCCESS;
}


/* Load the data store string with values from the data store structure  */ 
ks_status Load_DataStoreString(tcgByteValue *DataStoreString, SED_datastore_struct *DataStoreData) {

   int  		BandNumber;
   int			stroffset;

   stroffset=0;

   /* Initialize DataStoreString area to all 8's  we only use 1004 but lets clear the whole area */
   memset(DataStoreString->data, 8, MAX_DATASTORE_SIZE);
   
   /* need to Store header info */
   memcpy(DataStoreData->Header, CURRENT_VERSION, HEADER_SIZE );
   
   memmove(DataStoreString->data, DataStoreData->Header, HEADER_SIZE );
   stroffset += HEADER_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->IV, IV_SIZE );
   stroffset += IV_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->SaltSID, SALT_SIZE );
   stroffset += SALT_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->TagSID, TAG_SIZE );
   stroffset += TAG_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->SIDPin, PIN_SIZE );
   stroffset += PIN_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->SaltEM, SALT_SIZE );
   stroffset += SALT_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->TagEM, TAG_SIZE );
   stroffset += TAG_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->EraseMasterPin, PIN_SIZE );
   stroffset += PIN_SIZE;

   for (BandNumber = 0; BandNumber < MAX_SETBANDS; BandNumber++) {
    
     memmove(DataStoreString->data+stroffset, DataStoreData->SaltBM[BandNumber], SALT_SIZE );
     stroffset += SALT_SIZE;
   
     memmove(DataStoreString->data+stroffset, DataStoreData->TagBM[BandNumber], TAG_SIZE );
     stroffset += TAG_SIZE;

     memmove(DataStoreString->data+stroffset, DataStoreData->BandMasterPin[BandNumber], PIN_SIZE );
     stroffset += PIN_SIZE;
   } /* end for */

   memmove(DataStoreString->data+stroffset, DataStoreData->SaltBM915, SALT_SIZE );
   stroffset += SALT_SIZE;

   memmove(DataStoreString->data+stroffset, DataStoreData->TagBM915, TAG_SIZE );
   stroffset += TAG_SIZE;

   for (BandNumber=MAX_SETBANDS; BandNumber < MAX_BAND_SIZE; BandNumber++)
    {
      memmove(DataStoreString->data+stroffset, DataStoreData->BandMasterPin915[BandNumber], PIN_SIZE );
      stroffset += PIN_SIZE;
    }
   
   DataStoreString->len = stroffset;

   return (KS_SUCCESS);

}  /* end Load_DataStoreString  */

