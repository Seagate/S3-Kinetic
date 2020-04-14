#ifndef LEVEL_0_DISCOVERY_H
#define LEVEL_0_DISCOVERY_H

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

/**
 * @file level_0_discovery.h
 *
 * Descriptions of the Level 0 Discovery Data Structures.
 * 
 */

#include <stdint.h>
#include "memory_helpers.h"

/*Secure Messaging Constants */
/**
 *  The number of SP's.
 *  Increase the number as needed.
 *  If the device returns more than this number of SPs, 
 *  then the rest will be discarded.
 *  The TCG Enterprise SSC requires at least two. (ADMIN and LOCKING)
 *  The TCG Opal SSC requires at least two. (ADMIN and LOCKING)
 *  The TCG Opal SSC v2.00 requires at least two. (ADMIN and LOCKING)
 *  If Enterprise diverges from Opal we will need to separate this into 2 values
 */
#define MAX_SPIDS  2

/**
 *  The number of CipherSuites Supported.
 *  Increase the number as needed.
 *  If the device returns more than this number of CipherSuites, 
 *  then the rest will be discarded. (Hopefully they will always add to the end of the established list)
 *  The TCG Enterprise SSC ,TCG Opal SSC ,TCG Opal SSC v2.00 
 *  currently support 20 from TCG Core Spec Addendum revision 1.05 (July 10, 2014)
 *  If TCG adds more and we support it, this number will need to be changed
 */
#define MAX_CIPHERSUITES  20

/** An arbitrary "big enough" value */
#define LEVEL_0_DISCOVERY_DATA_MAX_SIZE  4096
typedef unsigned char discoveryDataBuffer[LEVEL_0_DISCOVERY_DATA_MAX_SIZE];

typedef byteSource discoveryData;


/** NOTE: The structures are laid out in the order that the fields are
          described by the standards documents. However, structure and bit
          field packing are notoriously compiler and platform dependent.
          These structures and bit-fields are provided as documentation of
          the structures. The implementation of this API will parse the
          inputPtr data into these structures, the user(s) of this API should
          not assume how this done.
*/

/** The header fields are inline (we don't need to share them
    with another struct). The body has to be parsed
    incrementally because we don't know in advance which descriptors
    will be returned. */
typedef struct {
  uint32_t      parameterDataLength;
  uint32_t      dataStructureVersion;
  unsigned char reserved[8];
  unsigned char vendorSpecificHeader[32];
} discoveryDataHeader;

#define VENDOR_VERSION_OFFSET    0  /**< Index into vendorSpecificHeader for the Seagate Vendor Version */
#define VENDOR_DEVICE_SECURITY_LIFE_CYCLE_STATE_OFFSET    1  /**< Index into vendorSpecificHeader for the Seagate Vendor Version */


/* Will return new pointer that is past the parsed data, or NULL on error */
extern byteSource parseDiscoveryDataHeader(byteSource inputPtr, discoveryDataHeader *data);
extern void  printDiscoveryDataHeader(discoveryDataHeader *data);


typedef struct {
  uint16_t      featureCode;
  unsigned int  version :4;
  unsigned int  Reserved :4;
  unsigned char length;
} featureDescriptorHeader;

/* Will return new pointer that is past the parsed data, or NULL on error */
extern byteSource parseFeatureDescriptorHeader(byteSource inputPtr, featureDescriptorHeader *data);
extern void printFeatureDescriptorHeader(featureDescriptorHeader *data);

/* the following is used so we skip trailing 0's in a message */
#define EMPTY_FEATURE_CODE   0x0000

/* Code as defined in the TCG SSC Enterprise document, section 3.6.2.4.7 Required Values */
#define TPER_FEATURE_CODE     0x0001
typedef struct {
  unsigned int ReservedA: 1;
  unsigned int ComIDManagementSupported: 1;
  unsigned int ReservedB: 1;
  unsigned int StreamingSupported: 1;
  unsigned int BufferManagementSupported: 1;
  unsigned int ACKNACKSupported: 1;
  unsigned int AsynchSupported: 1;
  unsigned int SyncSupported: 1;
  unsigned char ReservedGroup[11];
} tperFeatureDescriptor;

/* 3.6.2.4.7 says Version 0x1 or any version that supports the features defined in this SSC */
/* 3.6.4.2.7 says Length = 0x0C and SyncSupported and StreamingSupported must both be 1 */
extern byteSource parseTPerFeatureDescriptor(byteSource inputPtr, tperFeatureDescriptor *data);
extern void  printTPerFeatureDescriptor(tperFeatureDescriptor *data);

/* Code as defined in the TCG SSC Enterprise document, section 3.6.2.5.7 Required Values */
#define LOCKING_FEATURE_CODE  0x0002
typedef struct {
  unsigned int Reserved :2;
  unsigned int MBRDone :1;
  unsigned int MBREnabled :1;
  unsigned int MediaEncryption :1;
  unsigned int Locked :1;
  unsigned int LockingEnabled :1;
  unsigned int LockingSupported :1;
  unsigned char reserved[11];
} lockingFeatureDescriptor;

/* 3.6.2.5.7 says Length = 0x0C, version (as for TPER Feature), LockingSupported and MediaEncryption must be 1 */
/* Will return new pointer that is past the parsed data, or NULL on error */
extern byteSource parseLockingFeatureDescriptor(byteSource inputPtr, lockingFeatureDescriptor *data);
extern void  printLockingFeatureDescriptor(lockingFeatureDescriptor *data);

/* Code as defined in the TCG Opal SSC document, section 3.1.1.4 Opal SSC Feature */
#define OPAL_SSC_FEATURE_CODE  0x0200
typedef struct {
  uint16_t baseComID;
  uint16_t comIDCount;
  unsigned int reservedA :7;
  unsigned int rangeCrossing :1;
  unsigned char reservedB[11];
} opalSSCFeatureDescriptor;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseOpalSSCFeatureDescriptor(byteSource inputPtr, opalSSCFeatureDescriptor *data);
void  printOpalSSCFeatureDescriptor(opalSSCFeatureDescriptor *data);

/* Code as defined in the TCG Opal SSC v2.00 document, section 3.1.1.5 Opal SSC V2.00 Feature */
#define OPAL_SSC_V2_FEATURE_CODE  0x0203
typedef struct {
  uint16_t baseComID;
  uint16_t comIDCount;
  unsigned int reservedA :7;
  unsigned int rangeCrossing :1;
  uint16_t numLockingSPAdminAuthoritiesSupported;
  uint16_t numLockingSPUserAuthoritiesSupported;
  unsigned char initialC_PIN_SIDPINIndicator;
  unsigned char behaviorOfC_PIN_SIDPINUponTperRevert;
  unsigned char reservedB[5];
} opalSSCv2FeatureDescriptor;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseOpalSSCv2FeatureDescriptor(byteSource inputPtr, opalSSCv2FeatureDescriptor *data);
void  printOpalSSCv2FeatureDescriptor(opalSSCv2FeatureDescriptor *data);

/* Code as defined in the TCG Opal SSC Feature Set: Single User Mode document, 
 * section 4.2.1 Single User Mode Feature Descriptor */
#define SINGLE_USER_MODE_FEATURE_CODE  0x0201
typedef struct {
  uint32_t numLockingObjectsSupported;
  unsigned int Reserved :5;
  unsigned int Policy :1;
  unsigned int All :1;
  unsigned int Any :1;
  unsigned char reserved[7];
} singleUserModeFeatureDescriptor;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseSingleUserModeFeatureDescriptor(byteSource inputPtr, singleUserModeFeatureDescriptor *data);
void  printSingleUserModeFeatureDescriptor(singleUserModeFeatureDescriptor *data);

/* Code as defined in the TCG Opal SSC Feature Set: Additional DataStore Tables document, 
 * section 4.1.1 Level 0 Discovery - DataStore Table Feature Descriptor */
#define DATASTORE_TABLE_FEATURE_CODE  0x0202
typedef struct {
  unsigned char reservedA[2];
  uint16_t maxNumDataStoreTables;
  uint32_t maxTotalSizeDataStoreTables;
  uint32_t dataStoreTableSizeAlignment;
} dataStoreTableFeatureDescriptor;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseDataStoreTableFeatureDescriptor(byteSource inputPtr, dataStoreTableFeatureDescriptor *data);
void  printDataStoreTableFeatureDescriptor(dataStoreTableFeatureDescriptor *data);

/* Code as defined in the TCG Enterprise SSC document, section 3.6.2.7 Enterprise SSC Feature */
#define ENTERPRISE_SSC_FEATURE_CODE  0x0100
typedef struct {
  uint16_t     baseComID;
  uint16_t     comIDCount;
  unsigned int reservedA :7;
  unsigned int rangeCrossing :1;
  unsigned char reservedB[11];
} enterpriseSSCFeatureDescriptor;

/* 3.6.2.7 says ComIDCount >= 0x02; rangeCrossing is Vendor Unique */
/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseEnterpriseSSCFeatureDescriptor(byteSource inputPtr, enterpriseSSCFeatureDescriptor *data);
void  printEnterpriseSSCFeatureDescriptor(enterpriseSSCFeatureDescriptor *data);

#ifdef SECURE_MESSAGING
/* Code as defined in the TCG Core Spec Addendum Secure Messaging Spec Version 1.00 4.1.1 Secure Messaging Feature Descriptor*/
#define SECURE_MESSAGING_FEATURE_CODE  0x0004
typedef struct {
  unsigned int Activated :1;
  /* Secure Messaging Features */
  unsigned int ReservedTLS :2;
  unsigned int Certificate_Request :1;
  unsigned int Server_Certificate :1;
  unsigned int Renegotiation :1;
  unsigned int Compression :1;
  unsigned int Session_Resumption :1;
  /* Unused area */
  unsigned char Reserved[3] ;
  /* SP assignment */
  uint16_t SPCount;
  uint64_t SP[MAX_SPIDS];
  /* Ciphersuites */  
  uint16_t CSCount;
  uint32_t CS[MAX_CIPHERSUITES];

} secureMessagingFeatureDescriptor;

/* 4.1.1.1 Activated 4.1.1.2 TLS Features */
/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseSecureMessagingFeatureDescriptor(byteSource inputPtr, secureMessagingFeatureDescriptor *data);
void  printSecureMessagingFeatureDescriptor(secureMessagingFeatureDescriptor *data);
#endif

/* Code as defined in the Seagate Product Requirements, section 4.2.3.1 Logical Port Feature Descriptor */
#define LOGICAL_PORT_FEATURE_CODE  0xC001
typedef struct {
  uint32_t      portIdentifier;
  unsigned char portLocked;
  unsigned char reserved[3];
} logicalPortDataStructure;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseLogicalPortDataStructure(byteSource inputPtr, logicalPortDataStructure *data);
void  printLogicalPortDataStructure(logicalPortDataStructure *data);

/* Code as defined in the TcgSedDataStructures, section 6.2.1.3.7 Activation Feature Descriptor */
#define ACTIVATION_FEATURE_CODE  0xC004
typedef struct {
  uint32_t      portIdentifier;
  unsigned char portLocked;
  unsigned char reserved[3];
} activationFeatureDescriptor;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseActivationFeatureDescriptor(byteSource inputPtr, activationFeatureDescriptor *data);
void  printActivationFeatureDescriptor(activationFeatureDescriptor *data);


/* Code as defined in the Seagate Product Requirements, section 4.2.3.2 Geometry Feature Descriptor */
#define GEOMETRY_FEATURE_CODE  0x0003
typedef struct {
  unsigned int  reservedA : 7;
  unsigned int  align : 1;
  unsigned char reservedB[7];
  uint32_t      logicalBlockSize;
  uint64_t      alignmentGranularity;
  uint64_t      lowestAlignedLBA;
} geometryFeatureDescriptor;

/* Will return new pointer that is past the parsed data, or NULL on error */
byteSource parseGeometryFeatureDescriptor(byteSource inputPtr, geometryFeatureDescriptor *data);
void  printGeometryFeatureDescriptor(geometryFeatureDescriptor *data);

#endif
