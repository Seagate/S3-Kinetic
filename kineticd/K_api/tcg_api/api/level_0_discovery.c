/*
 * Do NOT modify or remove this copyright and confidentiality notice.
 *
 * Copyright 2013 - 2014 Seagate Technology LLC.
 *
 * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 * and may be covered under one or more Non-Disclosure Agreements. All or
 * portions are also trade secret. Any use, modification, duplication,
 * derivation, distribution or disclosure of this code, for any reason,
 * not expressly authorized is prohibited. All other rights are expressly
 * reserved by Seagate Technology LLC.
 *
 */


/* CA 8/12/2014
   Need to add parse and print functions for return from level 0 discovery
   to display secure messaging feature descriptor
*/

/**
 * @file level_0_discovery.c
 * 
 */

#include <string.h>
#include <stdio.h>

#include "level_0_discovery.h"

byteSource parseDiscoveryDataHeader(byteSource inputPtr, discoveryDataHeader *data) {
  inputPtr = copyInt32s(inputPtr, 1, &(data->parameterDataLength));
  inputPtr = copyInt32s(inputPtr, 1, &(data->dataStructureVersion));
  inputPtr = copyBytes(inputPtr, sizeof(data->reserved), &(data->reserved[0]));
  inputPtr = copyBytes(inputPtr, sizeof(data->vendorSpecificHeader), (unsigned char *)&(data->vendorSpecificHeader));
  return inputPtr;
}

void printDiscoveryDataHeader(discoveryDataHeader *data) {
  unsigned int i;  
  printf("Discovery Header Data:\n");
  printf("parameterDataLlength: 0x%jx\nVersion: 0x%jx\n", (uintmax_t)data->parameterDataLength, (uintmax_t)data->dataStructureVersion);
  for (i = 0; i < sizeof(data->reserved); i++) {
    printf("reserved byte %u: 0x%02x\n", i, data->reserved[i]);
  }
  for (i = 0; i < sizeof(data->vendorSpecificHeader); i++) {
    printf("Vendor Specific Header byte: %u: 0x%02x\n", i, data->vendorSpecificHeader[i]);
  }
}

byteSource parseFeatureDescriptorHeader(byteSource inputPtr, featureDescriptorHeader *data) {
  unsigned char version, Reserved; /* temp copy since & operator doesn't work on bit fields */
  inputPtr = copyInt16s(inputPtr, 1, &(data->featureCode));
  inputPtr = copyNibbles(inputPtr, &version, &Reserved);
  if (inputPtr != NULL) {
    data->version = version;
    data->Reserved = Reserved;
  }
  inputPtr = copyBytes(inputPtr, 1, &(data->length));
  return inputPtr;
}

void printFeatureDescriptorHeader(featureDescriptorHeader *data) {
  /*lets skip all the unsed areas in the mesage */
  if (data->featureCode != EMPTY_FEATURE_CODE) {
    printf("Feature Description Header:\n");
    printf("...Code    : 0x%04x\n", data->featureCode);
    printf("...Version : 0x%x\n", data->version);
    printf("...Reserved: 0x%x\n", data->Reserved);
    printf("...Length  : 0x%02x(%d)\n", data->length, data->length);
  }
}

byteSource parseTPerFeatureDescriptor(byteSource inputPtr, tperFeatureDescriptor *data) {
  unsigned char tmpByte; /* temp copy since & operator doesn't work in bit fields */
  inputPtr = copyBytes(inputPtr, 1, &tmpByte);
  if (inputPtr != NULL) {
    data->ReservedA = BIT(tmpByte, 7);
    data->ComIDManagementSupported = BIT(tmpByte, 6);
    data->ReservedB = BIT(tmpByte, 5);
    data->StreamingSupported = BIT(tmpByte, 4);
    data->BufferManagementSupported = BIT(tmpByte, 3);
    data->ACKNACKSupported = BIT(tmpByte, 2);
    data->AsynchSupported = BIT(tmpByte, 1);
    data->SyncSupported = BIT(tmpByte, 0);
  }
  inputPtr = copyBytes(inputPtr, sizeof(data->ReservedGroup), &(data->ReservedGroup[0]));
  return inputPtr;
}

void printTPerFeatureDescriptor(tperFeatureDescriptor *data) {
  unsigned int i;
  printf("Reserved: 0x%x\n", data->ReservedA);
  printf("ComID Management Supported: 0x%x\n", data->ComIDManagementSupported);
  printf("Reserved: 0x%x\n", data->ReservedB);
  printf("Streaming Supported: 0x%x\n", data->StreamingSupported);
  printf("Buffer Management Supported: 0x%x\n", data->BufferManagementSupported);
  printf("ACK/NACK Supported: 0x%x\n", data->ACKNACKSupported);
  printf("Asynch Supported: 0x%x\n", data->AsynchSupported);
  printf("Synch Supported: 0x%x\n", data->SyncSupported);
  for (i = 0; i < sizeof(data->ReservedGroup); i++) {
    printf("Reserved Group Byte (%d): 0x%02x\n", i, data->ReservedGroup[i]);
  }
}

byteSource parseLockingFeatureDescriptor(byteSource inputPtr, lockingFeatureDescriptor *data) {
  unsigned char tmpByte; /* temp copy since & operator doesn't work in bit fields */
  inputPtr = copyBytes(inputPtr, 1, &tmpByte);
  if (inputPtr != NULL) {
    data->Reserved = (BIT(tmpByte, 7) << 1) | BIT(tmpByte, 6);
    data->MBRDone = BIT(tmpByte, 5);
    data->MBREnabled = BIT(tmpByte, 4);
    data->MediaEncryption = BIT(tmpByte, 3);
    data->Locked = BIT(tmpByte, 2);
    data->LockingEnabled = BIT(tmpByte, 1);
    data->LockingSupported = BIT(tmpByte, 0);
  }
  inputPtr = copyBytes(inputPtr, sizeof(data->reserved), &(data->reserved[0]));
  return inputPtr;
}


void printLockingFeatureDescriptor(lockingFeatureDescriptor *data) {
  unsigned int i;
  printf("Reserved: 0x%x\n", data->Reserved);
  printf("MBRDone: 0x%x\n",  data->MBRDone);
  printf("MBREnabled: 0x%x\n", data->MBREnabled);
  printf("MediaEncryption: 0x%x\n", data->MediaEncryption);
  printf("Locked: 0x%x\n", data->Locked);
  printf("LockingEnabled: 0x%x\n", data->LockingEnabled);
  printf("LockingSupported: 0x%x\n", data->LockingSupported);

  for (i = 0; i < sizeof(data->reserved); i++) {
    printf("Reserved Byte (%d): 0x%02x\n", i, data->reserved[i]);
  }
}

byteSource parseOpalSSCFeatureDescriptor(byteSource inputPtr, opalSSCFeatureDescriptor *data) {
  unsigned char rangeCrossingByte;
  inputPtr = copyInt16s(inputPtr, 1, &(data->baseComID));
  inputPtr = copyInt16s(inputPtr, 1, &(data->comIDCount));
  inputPtr = copyBytes(inputPtr, 1, &rangeCrossingByte);
  if (inputPtr != NULL) {
    data->reservedA = rangeCrossingByte >> 1;
    data->rangeCrossing = BIT(rangeCrossingByte, 0);
  }
  inputPtr = copyBytes(inputPtr, sizeof(data->reservedB), &(data->reservedB[0]));
  return inputPtr;
}

void printOpalSSCFeatureDescriptor(opalSSCFeatureDescriptor *data) {
  unsigned int i;
  printf("baseComID: 0x%x\n", data->baseComID);
  printf("comIDCount: 0x%x\n", data->comIDCount);
  printf("reservedA: 0x%x\n", data->reservedA);
  printf("rangeCrossing: 0x%x\n", data->rangeCrossing);

  for (i = 0; i < sizeof(data->reservedB); i++) {
    printf("ReservedB Byte (%d): 0x%02x\n", i, data->reservedB[i]);
  }
}

byteSource parseOpalSSCv2FeatureDescriptor(byteSource inputPtr, opalSSCv2FeatureDescriptor *data) {
  unsigned char rangeCrossingByte;
  inputPtr = copyInt16s(inputPtr, 1, &(data->baseComID));
  inputPtr = copyInt16s(inputPtr, 1, &(data->comIDCount));
  inputPtr = copyBytes(inputPtr, 1, &rangeCrossingByte);
  if (inputPtr != NULL) {
    data->reservedA = rangeCrossingByte >> 1;
    data->rangeCrossing = BIT(rangeCrossingByte, 0);
  }
  inputPtr = copyInt16s(inputPtr, 1, &(data->numLockingSPAdminAuthoritiesSupported));
  inputPtr = copyInt16s(inputPtr, 1, &(data->numLockingSPUserAuthoritiesSupported));
  inputPtr = copyBytes(inputPtr, 1, &(data->initialC_PIN_SIDPINIndicator));
  inputPtr = copyBytes(inputPtr, 1, &(data->behaviorOfC_PIN_SIDPINUponTperRevert));
  inputPtr = copyBytes(inputPtr, sizeof(data->reservedB), &(data->reservedB[0]));
  return inputPtr;
}

void printOpalSSCv2FeatureDescriptor(opalSSCv2FeatureDescriptor *data) {
  unsigned int i;
  printf("baseComID: 0x%x\n", data->baseComID);
  printf("comIDCount: 0x%x\n", data->comIDCount);
  printf("reservedA: 0x%x\n", data->reservedA);
  printf("rangeCrossing: 0x%x\n", data->rangeCrossing);
  printf("numLockingSPAdminAuthoritiesSupported: 0x%x\n", data->numLockingSPAdminAuthoritiesSupported);
  printf("numLockingSPUserAuthoritiesSupported: 0x%x\n", data->numLockingSPUserAuthoritiesSupported);
  printf("initialC_PIN_SIDPINIndicator: 0x%x\n", data->initialC_PIN_SIDPINIndicator);
  printf("behaviorOfC_PIN_SIDPINUponTperRevert: 0x%x\n", data->behaviorOfC_PIN_SIDPINUponTperRevert);

  for (i = 0; i < sizeof(data->reservedB); i++) {
    printf("ReservedB Byte (%d): 0x%02x\n", i, data->reservedB[i]);
  }
}

byteSource parseSingleUserModeFeatureDescriptor(byteSource inputPtr, singleUserModeFeatureDescriptor *data) {
  unsigned char tmpByte; /* temp copy since & operator doesn't work in bit fields */
  inputPtr = copyInt32s(inputPtr, 1, &(data->numLockingObjectsSupported));
  inputPtr = copyBytes(inputPtr, 1, &tmpByte);
  if (inputPtr != NULL) {
    data->Reserved = (BIT(tmpByte, 7) << 4) | 
                     (BIT(tmpByte, 6) << 3) | 
                     (BIT(tmpByte, 5) << 2) | 
                     (BIT(tmpByte, 4) << 1) | 
                      BIT(tmpByte, 3);
    data->Policy = BIT(tmpByte, 2);
    data->All = BIT(tmpByte, 1);
    data->Any = BIT(tmpByte, 0);
  }
  inputPtr = copyBytes(inputPtr, sizeof(data->reserved), &(data->reserved[0]));
  return inputPtr;
}

void printSingleUserModeFeatureDescriptor(singleUserModeFeatureDescriptor *data) {
  unsigned int i;
  printf("numLockingObjectsSupported: 0x%x\n", data->numLockingObjectsSupported);
  printf("Policy: 0x%x\n", data->Policy);
  printf("All: 0x%x\n", data->All);
  printf("Any: 0x%x\n", data->Any);

  for (i = 0; i < sizeof(data->reserved); i++) {
        printf("Reserved Byte (%d): 0x%02x\n", i, data->reserved[i]);
  }
}

byteSource parseDataStoreTableFeatureDescriptor(byteSource inputPtr, dataStoreTableFeatureDescriptor *data) {
  inputPtr = copyBytes(inputPtr, sizeof(data->reservedA), &(data->reservedA[0]));
  inputPtr = copyInt16s(inputPtr, 1, &(data->maxNumDataStoreTables));
  inputPtr = copyInt32s(inputPtr, 1, &(data->maxTotalSizeDataStoreTables));
  inputPtr = copyInt32s(inputPtr, 1, &(data->dataStoreTableSizeAlignment));
  return inputPtr;
}

void printDataStoreTableFeatureDescriptor(dataStoreTableFeatureDescriptor *data) {
  unsigned int i;
  for (i = 0; i < sizeof(data->reservedA); i++) {
    printf("reserved Byte 0x%x(%d): 0x%x(%d)\n", i, i, data->reservedA[i], data->reservedA[i]);
  }
  printf("maxNumDataStoreTables: 0x%x\n", data->maxNumDataStoreTables);
  printf("maxTotalSizeDataStoreTables: 0x%x\n", data->maxTotalSizeDataStoreTables);
  printf("dataStoreTableSizeAlignment: 0x%x\n", data->dataStoreTableSizeAlignment);
}

byteSource parseEnterpriseSSCFeatureDescriptor(byteSource inputPtr, enterpriseSSCFeatureDescriptor *data) {
  unsigned char rangeCrossingByte;
  inputPtr = copyInt16s(inputPtr, 1, &(data->baseComID));
  inputPtr = copyInt16s(inputPtr, 1, &(data->comIDCount));
  inputPtr = copyBytes(inputPtr, 1, &rangeCrossingByte);
  if (inputPtr != NULL) {
    data->reservedA = rangeCrossingByte >> 1;
    data->rangeCrossing = BIT(rangeCrossingByte, 0);
  }
  inputPtr = copyBytes(inputPtr, sizeof(data->reservedB), &(data->reservedB[0]));
  return inputPtr;
}

void printEnterpriseSSCFeatureDescriptor(enterpriseSSCFeatureDescriptor *data) {
  unsigned int i;
  printf("baseComID: 0x%x\n", data->baseComID);
  printf("comIDCount: 0x%x\n", data->comIDCount);
  printf("reservedA: 0x%x\n", data->reservedA);
  printf("rangeCrossing: 0x%x\n", data->rangeCrossing);

  for (i = 0; i < sizeof(data->reservedB); i++) {
    printf("ReservedB Byte (%d): 0x%02x\n", i, data->reservedB[i]);
  }
}

#ifdef SECURE_MESSAGING
/* Secure Messaging Additions */
byteSource parseSecureMessagingFeatureDescriptor(byteSource inputPtr, secureMessagingFeatureDescriptor *data) {
  unsigned char tmpByte; /* temp copy since & operator doesn't work in bit fields */
  int 		i;

  inputPtr = copyBytes(inputPtr, 1, &tmpByte);
  data->Activated = BIT(tmpByte, 7);
  data->Certificate_Request = BIT(tmpByte, 6);
  data->Server_Certificate = BIT(tmpByte, 5);
  data->Renegotiation = BIT(tmpByte, 4);
  data->Compression = BIT(tmpByte, 3);
  data->Session_Resumption = BIT(tmpByte, 2);
  inputPtr = copyBytes(inputPtr, sizeof(data->Reserved), &(data->Reserved[0]));
  inputPtr = copyInt16s(inputPtr, 1, &data->SPCount);

  /* Need to read all SP's that are in this sequence up to SPCount */
  for (i = 0; i < (int)data->SPCount; i++)
    {
      inputPtr = copyInt64s(inputPtr, 1, &data->SP[i]);
    }

  inputPtr = copyInt16s(inputPtr, 1, &data->CSCount);
  /* Need to read all CipherSuites that are in this sequence up to CSCount */
  for (i = 0; i < (int)data->CSCount; i++)
    {
      inputPtr = copyInt32s(inputPtr, 1, &data->CS[i]);
    }
  return inputPtr;
}

void printSecureMessagingFeatureDescriptor(secureMessagingFeatureDescriptor *data) {
  unsigned int i;
  printf("TLS Secure Messaging Found\n");
  printf("Activated: 0x%x\n", data->Activated);
  printf("Certificate_Request: 0x%x\n", data->Certificate_Request);
  printf("Server_Certificate: 0x%x\n", data->Server_Certificate);
  printf("Renegotiation: 0x%x\n", data->Renegotiation);
  printf("Compression: 0x%x\n", data->Compression);
  printf("Session_Resumption: 0x%x\n", data->Session_Resumption);
  for (i = 0; i < sizeof(data->Reserved); i++) 
    printf("Reserved Byte (%d): 0x%02x\n", i, data->Reserved[i]);
  printf("SP Count: 0x%02jx\n", (uintmax_t)data->SPCount);
  for (i=0; i<data->SPCount; i++)
    printf("SP[%d] = 0x%016jx\n", i, (uintmax_t)data->SP[i] );

  printf("CS Count: 0x%02jx\n", (uintmax_t)data->CSCount);
  for (i=0; i<data->CSCount; i++)
    printf("CS[%d] = 0x%08jx\n", i, (uintmax_t)data->CS[i] );
}
#endif

byteSource parseLogicalPortDataStructure(byteSource inputPtr, logicalPortDataStructure *data) {
  inputPtr = copyInt32s(inputPtr, 1, &(data->portIdentifier));
  inputPtr = copyBytes(inputPtr, 1, &(data->portLocked));
  inputPtr = copyBytes(inputPtr, sizeof(data->reserved), &(data->reserved[0]));
  return inputPtr;
}

void printLogicalPortDataStructure(logicalPortDataStructure *data) {
  unsigned int i;
  printf("portIdentifier: 0x%jx\n", (uintmax_t)data->portIdentifier);
  printf("portLocked: 0x%x\n", data->portLocked);
  for (i = 0; i < sizeof(data->reserved); i++) {
    printf("Reserved Byte (%d): 0x%02x\n", i, data->reserved[i]);
  }
}

byteSource parseActivationFeatureDescriptor(byteSource inputPtr, activationFeatureDescriptor *data) {
  inputPtr = copyInt32s(inputPtr, 1, &(data->portIdentifier));
  inputPtr = copyBytes(inputPtr, 1, &(data->portLocked));
  inputPtr = copyBytes(inputPtr, sizeof(data->reserved), &(data->reserved[0]));
  return inputPtr;
}

#ifdef SECURE_MESSAGING
void printActivationFeatureDescriptor(activationFeatureDescriptor *data) {
  unsigned int i;
  printf("Activation Feature Present: \n");
//  printf("portIdentifier: 0x%jx\n", (uintmax_t)data->portIdentifier);
 // printf("portLocked: 0x%x\n", data->portLocked);
 // for (i = 0; i < sizeof(data->reserved); i++) {
 //   printf("Reserved Byte (%d): 0x%02x\n", i, data->reserved[i]);
//  }
}

#endif

byteSource parseGeometryFeatureDescriptor(byteSource inputPtr, geometryFeatureDescriptor *data) {
  unsigned char tempByte;
  inputPtr = copyBytes(inputPtr, 1, &tempByte);
  if (inputPtr != NULL) {
    data->reservedA = tempByte >> 1;
    data->align = BIT(tempByte, 0);
  }
  inputPtr = copyBytes(inputPtr, sizeof(data->reservedB), &(data->reservedB[0]));
  inputPtr = copyInt32s(inputPtr, 1, &data->logicalBlockSize);
  inputPtr = copyInt64s(inputPtr, 1, &data->alignmentGranularity);
  inputPtr = copyInt64s(inputPtr, 1, &data->lowestAlignedLBA);
  return inputPtr;
}

void printGeometryFeatureDescriptor(geometryFeatureDescriptor *data) {
  unsigned int i;

  printf("reservedA: 0x%x\n", data->reservedA);
  printf("align: 0x%x\n", data->align);

  for (i = 0; i < sizeof(data->reservedB); i++) {
    printf("ReservedB Byte (%d): 0x%02x\n", i, data->reservedB[i]);
  }

  printf("logicalBlockSize: 0x%jx\n", (uintmax_t)data->logicalBlockSize);
  printf("alignmentGranularity: 0x%jx\n", (uintmax_t)data->alignmentGranularity);
  printf("lowestAlignedLBA: 0x%jx\n", (uintmax_t)data->lowestAlignedLBA);
}
