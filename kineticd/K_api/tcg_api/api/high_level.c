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

/* High-level functions */

/*! @file
 *  There are several static variables defined here in order
 *  to simplify memory management. In a more robust implementation
 *  these variables mig live on the call stack or, in
 *  dynamically allocated memory, or ...
 */

#include <string.h>
#include <stdio.h>
#include "tcg_constants.h"
#include "high_level.h"
#include "low_level.h"
#include "parameters.h"
#include "debug.h"

#include "memory_helpers.h"

#include "parameters.h"

#include "utilities.h"

#include "transport.h"

/** Maximum size of a C_PIN table credential */
#define MAX_C_PIN_SIZE    32

/** This is the value to be used in a lot of calls if a pin is not provided.
 *  It will be set during discovery and should not be changed by client code.
 */
tcgByteValue msidPin;

/** The results from the last Get method call that was done by a high_level
 *  function.
 */
static parameters lastResults;

/** The parameters for the last Set method call that was done by a high_level
 *  function.
 */
static parameters lastToSet;

/** The parameters for the last Activate method call that was done by a 
 *  high_level function.
 */
static parameters lastToActivate;

/** The parameters for the last Reactivate method call that was done by a 
 *  high_level function.
 */
static parameters lastToReactivate;

/** The parameters for the last RevertSP method call that was done by a 
 *  high_level function.
 */
static parameters lastToRevertSP;

static tcgByteValue effectivePin;

/** Internal helper function. */
static tcgByteValue *msidDefaultPin(char *pin) {
  if (pin == NULL) {
    tcgByteValueCopy(&effectivePin, &msidPin);
  } else {
    tcgByteValueFromString(&effectivePin, pin);
  }
  return &effectivePin;
  }

/** OPTIONAL_STACK_RESET is used in each high_level function before
 *  any drive operations are started. By default this is defined
 *  to do nothing, but if you want every operation to first reset
 *  the TCG Stack, change the definition to:
 *  #define OPTIONAL_STACK_RESET issueStackReset(NULL)
 */
#define OPTIONAL_STACK_RESET    /*NOOP*/

static discoveryDataBuffer discoveryBuffer;
static parameters propertiesResults;
static parameters msidGetResults;

static discoveryDataHeader discoveryHeader;

/** parse the results of Level 0 Discovery and copy the commonly used info
    to the discovered_data structure.
    NOTE: This function does NOT enforce any ordering on the descriptors.
    NOTE: This function does NOT detect duplicate or "missing" descriptors.
    Both of those are extensions left to the reader.
*/
status parseThroughDiscoveryData(discoveryData blob, discovered_data *d) {
  byteSource  logicalPortEnd;
  byteSource  endOfData;
  CHECK_DEBUG;


  if (blob == NULL) {
    return API_ERROR("parseThroughDiscoveryData: NULL discoveryData pointer");
  }
  if (d == NULL) {
    return API_ERROR("parseThroughDiscoveryData: NULL discovered_data pointer");
  }

  memset(d, 0x00, sizeof(discovered_data));
  blob = parseDiscoveryDataHeader(blob, &discoveryHeader);
  if (blob == NULL) {
    return ERROR_("parseThroughDiscoveryData: Cannot parse discoveryHeader");
  }
  DEBUG_S(printDiscoveryDataHeader(&discoveryHeader));
  d->dataStructureVersion = discoveryHeader.dataStructureVersion;
  if (d->dataStructureVersion != 1) {
    return ERROR_("parseThroughDiscoveryData: Version must be 1!");
  }
  if (discoveryHeader.vendorSpecificHeader[VENDOR_VERSION_OFFSET] == 0x01) {
    d->driveSecurityLifeCycleState = discoveryHeader.vendorSpecificHeader[VENDOR_DEVICE_SECURITY_LIFE_CYCLE_STATE_OFFSET];
  }

  /* The Discovery Header Length counts everything except itself */
  endOfData = blob + sizeof(discoveryHeader.parameterDataLength) + discoveryHeader.parameterDataLength;

  while (blob < endOfData) {
    featureDescriptorHeader fDH;
    tperFeatureDescriptor TPFD;
    lockingFeatureDescriptor LFD;
    geometryFeatureDescriptor GFD;
    opalSSCFeatureDescriptor OpalSSCFD;
    unsigned int comIDCount;  /* CA  I dont see why this variable is necessary */

    opalSSCv2FeatureDescriptor OpalSSCv2FD;
    singleUserModeFeatureDescriptor SingleUserModeFD;
    dataStoreTableFeatureDescriptor DataStoreTableFD;
    enterpriseSSCFeatureDescriptor EntSSCFD;
#ifdef SECURE_MESSAGING
    secureMessagingFeatureDescriptor SecMsgFD;
#endif
#ifdef ADDACTIVATIONCODEHERE
    activationFeatureDescriptor ActMsgFD;
#endif
    unsigned int idx;

    DEBUG(("==================\nChecking for Feature Descriptor Header at %p \n ====================\n", blob)); /* blob has address for feature descriptor structure */
    blob = parseFeatureDescriptorHeader(blob, &fDH);
    if (blob == NULL) {
      return ERROR_("parseThroughDiscoveryData: Cannot parse Feature Descriptor Header");
    }
    DEBUG_S(printFeatureDescriptorHeader(&fDH));
    
/* CA 8/12/2014
   Added section to handle secure messaging feature descriptor
*/
    switch (fDH.featureCode) {
    case EMPTY_FEATURE_CODE:
      blob = copyBytes(blob, fDH.length, NULL);
      break;
    case TPER_FEATURE_CODE:
      DEBUG(("TPer Feature!\n"));
      blob = parseTPerFeatureDescriptor(blob, &TPFD);
      DEBUG_S(printTPerFeatureDescriptor(&TPFD));
      d->streamingSupported = TPFD.StreamingSupported;
      d->syncSupported = TPFD.SyncSupported;
      d->fdTPerSeen = 1;
      break;
    case LOCKING_FEATURE_CODE:
      DEBUG(("Locking Feature!\n"));
      blob = parseLockingFeatureDescriptor(blob, &LFD);
      DEBUG_S(printLockingFeatureDescriptor(&LFD));
      d->mbrDone = LFD.MBRDone;
      d->mbrEnabled = LFD.MBREnabled;
      d->lockingSupported = LFD.LockingSupported;
      d->lockingEnabled = LFD.LockingEnabled;
      d->mediaEncryption = LFD.MediaEncryption;
      d->fdLockingSeen = 1;
      break;
    case OPAL_SSC_FEATURE_CODE:
      DEBUG(("Opal SSC Feature!\n"));
      blob = parseOpalSSCFeatureDescriptor(blob, &OpalSSCFD);
      DEBUG_S(printOpalSSCFeatureDescriptor(&OpalSSCFD));
      d->comIDCount = comIDCount = OpalSSCFD.comIDCount;
      if (comIDCount > MAX_COMIDS) {
        DEBUG(("Warning, device has %d ComIDs, but we support only %d\n", comIDCount, MAX_COMIDS));
        comIDCount = MAX_COMIDS;
      }
      for (idx = 0; idx < comIDCount ; idx++) {
        d->comIDs[idx] = OpalSSCFD.baseComID + idx;
      }
      for (; idx < MAX_COMIDS; idx++) {
        d->comIDs[idx] = 0;
      }
      d->rangeCrossing = OpalSSCFD.rangeCrossing;
      d->fdOpalSSCSeen = 1;
      break;
    case OPAL_SSC_V2_FEATURE_CODE:
      DEBUG(("Opal SSC v2.00 Feature!\n"));
      blob = parseOpalSSCv2FeatureDescriptor(blob, &OpalSSCv2FD);
      DEBUG_S(printOpalSSCv2FeatureDescriptor(&OpalSSCv2FD));
      d->comIDCount = comIDCount = OpalSSCv2FD.comIDCount;
      if (comIDCount > MAX_COMIDS) {
        DEBUG(("Warning, device has %d ComIDs, but we support only %d\n", comIDCount, MAX_COMIDS));
        comIDCount = MAX_COMIDS;
      }
      for (idx = 0; idx < comIDCount ; idx++) {
        d->comIDs[idx] = OpalSSCv2FD.baseComID + idx;
      }
      for (; idx < MAX_COMIDS; idx++) {
        d->comIDs[idx] = 0;
      }
      d->rangeCrossing = OpalSSCv2FD.rangeCrossing;
      d->numLockingSPAdminAuthoritiesSupported = OpalSSCv2FD.numLockingSPAdminAuthoritiesSupported;
      d->numLockingSPUserAuthoritiesSupported = OpalSSCv2FD.numLockingSPUserAuthoritiesSupported;
      d->initialC_PIN_SIDPINIndicator = OpalSSCv2FD.initialC_PIN_SIDPINIndicator;
      d->behaviorOfC_PIN_SIDPINUponTperRevert = OpalSSCv2FD.behaviorOfC_PIN_SIDPINUponTperRevert;
      d->fdOpalSSCv2Seen = 1;
      break;
    case SINGLE_USER_MODE_FEATURE_CODE:
      DEBUG(("Single User Mode Feature!\n"));
      blob = parseSingleUserModeFeatureDescriptor(blob, &SingleUserModeFD);
      DEBUG_S(printSingleUserModeFeatureDescriptor(&SingleUserModeFD));
      d->numLockingObjectsSupported = SingleUserModeFD.numLockingObjectsSupported;
      d->policy = SingleUserModeFD.Policy;
      d->all = SingleUserModeFD.All;
      d->any = SingleUserModeFD.Any;
      d->fdSingleUserModeSeen = 1;
      break;
    case DATASTORE_TABLE_FEATURE_CODE:
      DEBUG(("DataStore Table Feature!\n"));
      blob = parseDataStoreTableFeatureDescriptor(blob, &DataStoreTableFD);
      DEBUG_S(printDataStoreTableFeatureDescriptor(&DataStoreTableFD));
      d->maxNumDataStoreTables = DataStoreTableFD.maxNumDataStoreTables;
      d->maxTotalSizeDataStoreTables = DataStoreTableFD.maxTotalSizeDataStoreTables;
      d->dataStoreTableSizeAlignment = DataStoreTableFD.dataStoreTableSizeAlignment;
      d->fdDataStoreTableSeen = 1;
      break;
    case ENTERPRISE_SSC_FEATURE_CODE:
      DEBUG(("Enterprise SSC Feature!\n"));
      blob = parseEnterpriseSSCFeatureDescriptor(blob, &EntSSCFD);
      DEBUG_S(printEnterpriseSSCFeatureDescriptor(&EntSSCFD));
      d->comIDCount = comIDCount = EntSSCFD.comIDCount;
      if (comIDCount > MAX_COMIDS) {
        DEBUG(("Warning, device has %d ComIDs, but we support only %d\n", comIDCount, MAX_COMIDS));
        comIDCount = MAX_COMIDS;
      }
      for (idx = 0; idx < comIDCount ; idx++) {
        d->comIDs[idx] = EntSSCFD.baseComID + idx;
      }
      for (; idx < MAX_COMIDS; idx++) {
        d->comIDs[idx] = 0;
      }
      d->rangeCrossing = EntSSCFD.rangeCrossing;
      d->fdEnterpriseSSCSeen = 1;
      break;

#ifdef SECURE_MESSAGING
    case SECURE_MESSAGING_FEATURE_CODE:
      DEBUG(("Secure Messaging Feature!\n"));
      blob = parseSecureMessagingFeatureDescriptor(blob, &SecMsgFD);
      DEBUG_S(printSecureMessagingFeatureDescriptor(&SecMsgFD));
      /* Get number of SP's specified */
      d->SPCount = SecMsgFD.SPCount;
      /* should we have this check in the code at all? */
      if (d->SPCount > MAX_SPIDS) {
        DEBUG(("Warning, device has %d SPs, but we support only %d ..truncating\n", d->SPCount, MAX_SPIDS));
        d->SPCount = MAX_SPIDS;
      }
      /* Walk through all SP ids contained according to incoming SPCount */
      for (idx = 0; idx < (int)d->SPCount; idx++)
       {
         d->SP[idx] = SecMsgFD.SP[idx];
       }

      /* Get number of Cipher Suites specified */
      d->CSCount = SecMsgFD.CSCount;
      /* should we have this check in the code at all? */
      if (d->CSCount > MAX_CIPHERSUITES) {
        DEBUG(("Warning, device has %d Ciphersuites, but we support only %d ..truncating\n", d->CSCount, MAX_CIPHERSUITES));
        d->CSCount = MAX_CIPHERSUITES;
      }
      /* Walk through all Ciphersuites contained according to incoming CSCount */
      for (idx = 0; idx < (int)d->CSCount; idx++)
       {
         d->CS[idx] = SecMsgFD.CS[idx];
       }

      /* these are values we would like to see dsiplayed on a level 0 Discovery dump */
      d->Activated = SecMsgFD.Activated;
      d->Certificate_Request = SecMsgFD.Certificate_Request;
      d->Server_Certificate = SecMsgFD.Server_Certificate;
      d->Renegotiation = SecMsgFD.Renegotiation;
      d->Compression = SecMsgFD.Compression;
      d->Session_Resumption = SecMsgFD.Session_Resumption;
      d->fdSecureMessagingSeen = 1;  /* set flag for Secure messaging */
      break;
#endif

    case LOGICAL_PORT_FEATURE_CODE:
      DEBUG(("Logical Port Feature!\n"));
      logicalPortEnd = blob + fDH.length;
       /* We need to parse/consume all the descriptors, even
         if we have to throw-away/ignore some of them, which
         is why we test idx in the body of the loop instead
         of in the loop's test expression.
      */
      for (idx = 0; blob < logicalPortEnd; idx++) {
        logicalPortDataStructure LPDS;
        blob = parseLogicalPortDataStructure(blob, &LPDS);
        DEBUG_S(printLogicalPortDataStructure(&LPDS));
        if (idx < MAX_LOGICAL_PORTS) {
          d->logicalPortIDs[idx] = LPDS.portIdentifier;
          d->logicalPortLocked[idx] = LPDS.portLocked;
        }
      }
      if (idx > MAX_LOGICAL_PORTS) {
        DEBUG(("Warning, got %d logical ports, but only had room for the first %d\n", idx, MAX_LOGICAL_PORTS));
        idx = MAX_LOGICAL_PORTS;
      }
      d->logicalPortCount = idx;
      d->fdLogicalPortSeen = 1;
      
      DEBUG(("Logical Port parsing stopped at: %p\n", blob));
      DEBUG(("calculated end                 : %p\n", logicalPortEnd));
      break;

    case ACTIVATION_FEATURE_CODE:
      DEBUG(("Activation Feature!\n"));
#ifdef ADDACTIVATIONCODEHERE
      logicalPortEnd = blob + fDH.length;
       /* We need to parse/consume all the descriptors, even
         if we have to throw-away/ignore some of them, which
         is why we test idx in the body of the loop instead
         of in the loop's test expression.
      */
      for (idx = 0; blob < logicalPortEnd; idx++) {
        logicalPortDataStructure LPDS;
        blob = parseLogicalPortDataStructure(blob, &LPDS);
        DEBUG_S(printLogicalPortDataStructure(&LPDS));
        if (idx < MAX_LOGICAL_PORTS) {
          d->logicalPortIDs[idx] = LPDS.portIdentifier;
          d->logicalPortLocked[idx] = LPDS.portLocked;
        }
      }
      if (idx > MAX_LOGICAL_PORTS) {
        DEBUG(("Warning, got %d logical ports, but only had room for the first %d\n", idx, MAX_LOGICAL_PORTS));
        idx = MAX_LOGICAL_PORTS;
      }
      d->logicalPortCount = idx;
      d->fdActivationSeen = 1;
      
      DEBUG(("Logical Port parsing stopped at: %p\n", blob));
      DEBUG(("calculated end                 : %p\n", logicalPortEnd));
#endif
      d->fdActivationSeen = 1;
      break;
    case GEOMETRY_FEATURE_CODE:
      DEBUG(("Geometry Feature!\n"));
      blob = parseGeometryFeatureDescriptor(blob, &GFD);
      DEBUG_S(printGeometryFeatureDescriptor(&GFD));
      d->logicalBlockSize = GFD.logicalBlockSize;
      d->alignmentGranularity = GFD.alignmentGranularity;
      d->lowestAlignedLBA = GFD.lowestAlignedLBA;
      d->alignmentRequired = GFD.align;
      d->fdGeometrySeen = 1;
      break;
    default:
      DEBUG(("Unknown feature descriptor: 0x%x, skipping 0x%x(%d) bytes.\n\n", fDH.featureCode, fDH.length, fDH.length));
      blob = copyBytes(blob, fDH.length, NULL);
      break;
    }
  }
  DEBUG(("\n\n\n"));
  DEBUG(("Parsing stopped at: %p\ncalculated end    : %p\n\n", blob, endOfData));
  return SUCCESS;
}

int isOpalDrive(discovered_data *d) {
  if (d == NULL) {
    return 0;
  }
  if ((d->dataStructureVersion == 1) &&
      (d->fdTPerSeen == 1) &&
      (d->fdOpalSSCSeen == 1) && 
      (d->fdLockingSeen == 1) &&
      (d->syncSupported == 1) &&
      (d->streamingSupported == 1) &&
      (d->lockingSupported == 1) &&
      (d->mediaEncryption == 1) &&
      (d->comIDCount >= 1)) {
    return 1;
  }
  
  return 0;
      
}

int isOpalv2Drive(discovered_data *d) {
  if (d == NULL) {
    return 0;
  }
  if ((d->dataStructureVersion == 1) &&
      (d->fdTPerSeen == 1) &&
      (d->fdOpalSSCv2Seen == 1) &&
      (d->fdLockingSeen == 1) &&
      (d->syncSupported == 1) &&
      (d->streamingSupported == 1) &&
      (d->lockingSupported == 1) &&
      (d->mediaEncryption == 1) &&
      (d->comIDCount >= 1) &&
      (d->numLockingSPAdminAuthoritiesSupported >= 4) &&
      (d->numLockingSPUserAuthoritiesSupported >= 8)) {
    return 1;
  }
 /* CNA  For testing of TLS we force this to return 1 */
#ifdef SECURE_MESSAGING
return 1;
#else
  return 0;
#endif
      
}

int isEnterpriseDrive(discovered_data *d) {
  if (d == NULL) {
    return 0;
  }
  if ((d->dataStructureVersion == 1) &&
      (d->fdTPerSeen == 1) &&
      (d->fdEnterpriseSSCSeen == 1) &&
      (d->fdLockingSeen == 1) &&
      (d->syncSupported == 1) &&
      (d->streamingSupported == 1) &&
      (d->lockingSupported == 1) &&
      (d->mediaEncryption == 1) &&
      (d->comIDCount >= 2)) {
    return 1;
  }
  
  return 0;
      
}

int singleUserModeSupported(discovered_data *d) {
  if (d == NULL) {
    return 0;
  }
  if ((isOpalDrive(d) ||
       isOpalv2Drive(d)) &&
      (d->fdSingleUserModeSeen == 1)) {
    return 1;
  }
  
  return 0;
      
}

status discover(transport *t, discovered_data *d) {
  CHECK_DEBUG;
  if (t == NULL) {
    return API_ERROR("discover: NULL transport");
  }
  if (d == NULL) {
    return API_ERROR("discover: NULL discovered_data");
  }
  TA_CHECK(discoverNoMSID(t, d));
  TA_CHECK(issueStartSession("AdminSP"));
  TA_CHECK(issueGet(pinNameToUID("MSID"), NULL, &msidGetResults));
  if (t->ssc == OPAL_SSC) {
    TA_CHECK(getIdByteValue(&msidGetResults, C_PIN_TABLE_PIN_COLUMN_ID, &msidPin));
  } else {
    TA_CHECK(getNamedByteValue(&msidGetResults, tcgTmpName("PIN"), &msidPin));
  }

  DEBUG(("MSID Pin: %s\n", tcgByteValueDebugStr(&msidPin)));
  TA_CHECK(issueCloseSession());

  return SUCCESS;
}

status discoverNoMSID(transport *t, discovered_data *d) {
  CHECK_DEBUG;
  if (t == NULL) {
    return API_ERROR("discoverNoMSID: NULL transport");
  }
  if (d == NULL) {
    return API_ERROR("discoverNoMSID: NULL discovered_data");
  }
  setTransport(t);
  OPTIONAL_STACK_RESET;
  DEBUG(("About to get Level 0 discovery data\n"));
  memset(discoveryBuffer, 0x00, sizeof(discoveryBuffer));
  TA_CHECK(issueLevel0Discovery(discoveryBuffer));
  DEBUG(("Parsing Level 0 discovery data\n"));
  TA_CHECK(parseThroughDiscoveryData(discoveryBuffer, d));
  DEBUG(("Level 0 Parsed OK\n"));

  if (!d->fdEnterpriseSSCSeen && !d->fdOpalSSCSeen && !d->fdOpalSSCv2Seen) {
    /* If not an Enterprise SSC, Opal SSC, or Opal SSC v2.00 drive 
     * we don't know how to talk to it:
     * No ComIDs, no ability to get MSID, etc.
     */
    return ERROR_("Drive is not Enterprise SSC, Opal SSC, or Opal SSC v2.00, discovery failure");
  }
  t->comID = ((d->comIDs[0]) << 16);  /* TODO: FIX THIS WITH A MACRO!! */
  DEBUG(("Picked ComID: 0x%jx\n", (uintmax_t)t->comID));
  if (isOpalDrive(d) || isOpalv2Drive(d)) {
    t->ssc = OPAL_SSC;
  } else {
    /* assume it is enterprise?   What about SDD drives? */
    t->ssc = ENTERPRISE_SSC;
  }

  DEBUG(("Checking Properties\n"));
  TA_CHECK(issueProperties(&propertiesResults));
  DEBUG(("GOT PARAMETERS REPLY!:\n"));
  DEBUG_S(dumpParameters(&propertiesResults));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("MaxPacketSize"), &(d->maxPacketSize)));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("MaxComPacketSize"), &(d->maxComPacketSize)));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("MaxResponseComPacketSize"), &(d->maxResponseComPacketSize)));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("MaxAuthentications"), &(d->maxAuthentications)));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("DefSessionTimeout"), &(d->defaultSessionTimeout)));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("MinSessionTimeout"), &(d->minSessionTimeout)));
  TA_CHECK(getNamedIntValue(&propertiesResults, tcgTmpName("MaxSessionTimeout"), &(d->maxSessionTimeout)));
  
  DEBUG_S(printDiscoveryData(d));

  return SUCCESS;
}

void printDiscoveryData(discovered_data *d) {
  uint64_t idx;
  if (d == NULL) {
    printf("printDiscoveryData: NULL pointer, nothing to print.\n");
    return;
  }
  printf("Discover data from %p\n", (void *)d);
  printf("fdTPerSeen: 0x%x\n", d->fdTPerSeen);
  printf("fdLockingSeen: 0x%x\n", d->fdLockingSeen);
  if (isOpalDrive(d) || isOpalv2Drive(d)) {
    printf("fdOpalSSCSeen: 0x%x\n", d->fdOpalSSCSeen);
    printf("fdOpalSSCv2Seen: 0x%x\n", d->fdOpalSSCv2Seen);
    printf("fdSingleUserModeSeen: 0x%x\n", d->fdSingleUserModeSeen);
    printf("fdDataStoreTableSeen: 0x%x\n", d->fdDataStoreTableSeen);
  } else {
    printf("fdEnterpriseSSCSeen: 0x%x\n", d->fdEnterpriseSSCSeen);
  }
  printf("fdLogicalPortSeen: 0x%x\n", d->fdLogicalPortSeen);
  printf("fdGeometrySeen: 0x%x\n", d->fdGeometrySeen);
  printf("fdSecureMessagingSeen: 0x%x\n", d->fdSecureMessagingSeen);
  if (d->fdSecureMessagingSeen) {
    printf("SPCount: 0x%x\n", d->SPCount);
    printf("CSCount: 0x%x\n", d->CSCount);
  }
  printf("fdActivationSeen: 0x%x\n", d->fdActivationSeen);
  printf("dataStructureVersion: 0x%jx\n", (uintmax_t)d->dataStructureVersion);
  printf("driveSecurityLifeCycleState: 0x%x\n", d->driveSecurityLifeCycleState);
  printf("StreamingSupported: 0x%x\n", d->streamingSupported);
  printf("SyncSupported: 0x%x\n", d->syncSupported);
  if (isOpalDrive(d) || isOpalv2Drive(d)) {
    printf("MBRDone: 0x%x\n", d->mbrDone);
    printf("MBREnabled: 0x%x\n", d->mbrEnabled);
  } 
  printf("LockingEnabled: 0x%x\n", d->lockingEnabled);
  printf("MediaEncryption: 0x%x\n", d->mediaEncryption);
  printf("LockingSupported: 0x%x\n", d->lockingSupported);
  printf("rangeCrossing: 0x%x\n", d->rangeCrossing);
  printf("comIDCount: 0x%x\n", d->comIDCount);
  printf("ComIDS:");
  for (idx = 0; idx < MAX_COMIDS; idx++) {
    printf(" 0x%jx", (uintmax_t)d->comIDs[idx]);
  }
  printf("\n");
  printf("maxPacketSize: 0x%jx\n", (uintmax_t)d->maxPacketSize);
  printf("maxComPacketSize: 0x%jx\n", (uintmax_t)d->maxComPacketSize);
  printf("maxResponseComPacketSize: 0x%jx\n", (uintmax_t)d->maxResponseComPacketSize);
  printf("maxAuthentications: 0x%jx\n", (uintmax_t)d->maxAuthentications);
  printf("defaultSessionTimeout: 0x%jx\n", (uintmax_t)d->defaultSessionTimeout);
  printf("maxSessionTimeout: 0x%jx\n", (uintmax_t)d->maxSessionTimeout);
  printf("minSessionTimeout: 0x%jx\n", (uintmax_t)d->minSessionTimeout);
  
  printf("alignmentRequired: 0x%x\n", d->alignmentRequired);
  printf("logicalBlockSize: 0x%jx\n", (uintmax_t)d->logicalBlockSize);
  printf("alignmentGranularity: 0x%jx\n", (uintmax_t)d->alignmentGranularity);
  printf("lowestAlignedLBA: 0x%jx\n", (uintmax_t)d->lowestAlignedLBA);
  
  printf("logicalPortCount: 0x%jx\n", (uintmax_t)d->logicalPortCount);
  printf("logicalPorts id(locked):");
  for (idx = 0; idx < d->logicalPortCount; idx++) {
    printf(" 0x%08x(%d)", d->logicalPortIDs[idx], d->logicalPortLocked[idx]);
  }
  printf("\n");
  printf("isOpalDrive: 0x%x\n", isOpalDrive(d));
  printf("isOpalv2Drive: 0x%x\n", isOpalv2Drive(d));
  printf("isEnterpriseDrive: 0x%x\n", isEnterpriseDrive(d));
  if (isOpalDrive(d) || isOpalv2Drive(d)) {
    printf("singleUserModeSupported(all, any, policy): 0x%x(0x%x, 0x%x, 0x%x)\n", singleUserModeSupported(d), d->all, d->any, d->policy);
  }
}

status activateBandLocking(char *pin, sumBands_t sumBands,
                      rangeStartRangeLengthPolicy_t policy,
                      uint32_t numDataStoreTables) {
  parameters *toActivate = &lastToActivate;
  tcgByteValue *tcgPin;
  tcgByteValue tempByteValue;
  unsigned int idx;
  CHECK_TRANSPORT;
  resetParameters(toActivate);
  if (sumBands == ALL_BANDS) {
    TA_CHECK(setStartName(toActivate));
    TA_CHECK(setIntValue(toActivate, ACTIVATE_METHOD_SUMSELECTIONLIST_PARAMETER_ID));
    tcgByteValueFromStringCount(&tempByteValue, (char *)LOCKING_TABLE_UID, TCG_UID_BYTE_COUNT);
    TA_CHECK(setByteValue(toActivate, &tempByteValue));
    TA_CHECK(setEndName(toActivate));
    TA_CHECK(setStartName(toActivate));
    TA_CHECK(setIntValue(toActivate, ACTIVATE_METHOD_POLICY_PARAMETER_ID));
    if (policy == USER_POLICY) {
      TA_CHECK(setIntValue(toActivate, USER_POLICY));
    } else {
      TA_CHECK(setIntValue(toActivate, ADMINS_POLICY));
    }
    TA_CHECK(setEndName(toActivate));
  }
  if (numDataStoreTables > 16) {
    return API_ERROR("activateBandLocking: numDataStoreTables too large");
  } else {
    if (numDataStoreTables > 0) {
      TA_CHECK(setStartName(toActivate));
      TA_CHECK(setIntValue(toActivate, ACTIVATE_METHOD_DATASTORETABLESIZES_PARAMETER_ID));
      TA_CHECK(setStartList(toActivate));
      for (idx = 0; idx < numDataStoreTables; idx++) {
        TA_CHECK(setIntValue(toActivate, 0x4000));
      }
      TA_CHECK(setEndList(toActivate));
      TA_CHECK(setEndName(toActivate));
    }
  }
  tcgPin = msidDefaultPin(pin);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("AdminSP"));
  TA_CHECK(issueAuthenticate("SID", tcgPin));
  TA_CHECK(issueActivate(toActivate));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status reactivateBandLocking(char *pin, sumBands_t sumBands,
                      rangeStartRangeLengthPolicy_t policy,
                       char *newAdmin1Pin, uint32_t numDataStoreTables) {
  parameters *toReactivate = &lastToReactivate;
  tcgByteValue *tcgPin;
  tcgByteValue tcgNewAdmin1Pin;
  tcgByteValue tempByteValue;
  unsigned int idx;
  CHECK_TRANSPORT;
  resetParameters(toReactivate);
  if (sumBands == ALL_BANDS) {
    TA_CHECK(setStartName(toReactivate));
    TA_CHECK(setIntValue(toReactivate, REACTIVATE_METHOD_SUMSELECTIONLIST_PARAMETER_ID));
    tcgByteValueFromStringCount(&tempByteValue, (char *)LOCKING_TABLE_UID, TCG_UID_BYTE_COUNT);
    TA_CHECK(setByteValue(toReactivate, &tempByteValue));
    TA_CHECK(setEndName(toReactivate));
    TA_CHECK(setStartName(toReactivate));
    TA_CHECK(setIntValue(toReactivate, REACTIVATE_METHOD_POLICY_PARAMETER_ID));
    if (policy == USER_POLICY) {
      TA_CHECK(setIntValue(toReactivate, USER_POLICY));
    } else {
      TA_CHECK(setIntValue(toReactivate, ADMINS_POLICY));
    }
    TA_CHECK(setEndName(toReactivate));
  }
  tcgByteValueFromString(&tcgNewAdmin1Pin, newAdmin1Pin);
  if (newAdmin1Pin != NULL) {
    TA_CHECK(setStartName(toReactivate));
    TA_CHECK(setIdByteValue(toReactivate, REACTIVATE_METHOD_ADMIN1PIN_PARAMETER_ID, &tcgNewAdmin1Pin));
    TA_CHECK(setEndName(toReactivate));
  }
  if (numDataStoreTables > 16) {
    return API_ERROR("reactivateBandLocking: numDataStoreTables too large");
  } else {
    if (numDataStoreTables > 0) {
      TA_CHECK(setStartName(toReactivate));
      TA_CHECK(setIntValue(toReactivate, REACTIVATE_METHOD_DATASTORETABLESIZES_PARAMETER_ID));
      TA_CHECK(setStartList(toReactivate));
      for (idx = 0; idx < numDataStoreTables; idx++) {
        TA_CHECK(setIntValue(toReactivate, 0x4000));
      }
      TA_CHECK(setEndList(toReactivate));
      TA_CHECK(setEndName(toReactivate));
    }
  }
  tcgPin = msidDefaultPin(pin);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  TA_CHECK(issueReactivate(toReactivate));
  return SUCCESS;
}

status getAuthority(char *authority, char *pin, uint64_t *enabled) {
  parameters *results = &lastResults;
  tcgByteValue *tcgPin;
  CHECK_TRANSPORT;
  if (authority == NULL) {
    return API_ERROR("getAuthority: NULL Authority");
  }
  tcgPin = msidDefaultPin(pin);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  } else {
    TA_CHECK(issueAuthenticate(authority, tcgPin));
  }
  TA_CHECK(issueGet(authorityNameToUID(authority), NULL, results));
  TA_CHECK(issueCloseSession());
  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(getIdIntValue(results, AUTHORITY_TABLE_ENABLED_COLUMN_ID, enabled));
  } else { 
    TA_CHECK(getNamedIntValue(results, tcgTmpName("Enabled"), enabled));
  }
  return SUCCESS;
}

status setAuthority(char *authority, char *pin, uint64_t enabled) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  if (authority == NULL) {
    return API_ERROR("setAuthority: NULL Authority");
  }
  resetParameters(toSet);
  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setIdIntValue(toSet, AUTHORITY_TABLE_ENABLED_COLUMN_ID, enabled));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
    OPTIONAL_STACK_RESET;
    TA_CHECK(issueStartSession("LockingSP"));
    TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  } else {
    /* Empty cell block - cell_block does not apply to Set on Objects */
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setEndList(toSet));
    /* Values - list of list of Column-name/value named-pairs */
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("Enabled"), enabled));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndList(toSet));
    OPTIONAL_STACK_RESET;
    TA_CHECK(issueStartSession("LockingSP"));
    TA_CHECK(issueAuthenticate("EraseMaster", tcgPin));
  }
  TA_CHECK(issueSet(authorityNameToUID(authority), toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status setPin(char *authority, char *SP, char *newPin, char *oldPin) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgOldPin;
  tcgByteValue tcgNewPin;
  CHECK_TRANSPORT;
  if (authority == NULL) {
    return API_ERROR("setPin: NULL Authority");
  }
  if (SP == NULL) {
    return API_ERROR("setPin: NULL SP");
  }
  if (newPin == NULL) {
    return API_ERROR("setPin: NULL newPin");
  }
  resetParameters(toSet);

  tcgOldPin = msidDefaultPin(oldPin);
  tcgByteValueFromString(&tcgNewPin, newPin);

  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setIdByteValue(toSet, C_PIN_TABLE_PIN_COLUMN_ID, &tcgNewPin));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
  } else {
    /* Empty cell_block - cell_block does not apply to Set on Objects */
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setEndList(toSet));

    /* Values - list of list of Column-name/value named-pairs */
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setNamedByteValue(toSet, tcgTmpName("PIN"), &tcgNewPin));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndList(toSet));
  }

  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession(SP));
  TA_CHECK(issueAuthenticate(authority, tcgOldPin));
  TA_CHECK(issueSet(pinNameToUID(authority), toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status getBandOpal(int bandNumber, uint64_t *startingLBA, uint64_t *length, char *pin) {
  parameters *results = &lastResults;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  if (startingLBA == NULL) {
    return API_ERROR("getBandOpal: NULL startingLBA");
  }
  if (length == NULL) {
    return API_ERROR("getBandOpal: NULL length");
  }
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  TA_CHECK(issueGet(bandNumberToUID(bandNumber), NULL, results));
  TA_CHECK(issueCloseSession());
  TA_CHECK(getIdIntValue(results, LOCKING_TABLE_RANGESTART_COLUMN_ID, startingLBA));
  TA_CHECK(getIdIntValue(results, LOCKING_TABLE_RANGELENGTH_COLUMN_ID, length));
  return SUCCESS;
}

status getBandEnterprise(int bandNumber, uint64_t *startingLBA, uint64_t *length) {
  parameters *results = &lastResults;
  CHECK_TRANSPORT;
  if (startingLBA == NULL) {
    return API_ERROR("getBandEnterprise: NULL startingLBA");
  }
  if (length == NULL) {
    return API_ERROR("getBandEnterprise: NULL length");
  }
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueGet(bandNumberToUID(bandNumber), NULL, results));
  TA_CHECK(issueCloseSession());
  TA_CHECK(getNamedIntValue(results, tcgTmpName("RangeStart"), startingLBA));
  TA_CHECK(getNamedIntValue(results, tcgTmpName("RangeLength"), length));
  return SUCCESS;
}

status setBand(int bandNumber, uint64_t startingLBA, uint64_t length, char *pin) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  resetParameters(toSet);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(issueAuthenticate("Admin1", tcgPin));
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setIdIntValue(toSet, LOCKING_TABLE_RANGESTART_COLUMN_ID, startingLBA));
    TA_CHECK(setIdIntValue(toSet, LOCKING_TABLE_RANGELENGTH_COLUMN_ID, length));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
  } else {
    TA_CHECK(issueAuthenticate(bandNumberToAuthority(bandNumber), tcgPin));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("RangeStart"), startingLBA));
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("RangeLength"), length));
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndList(toSet));
  }
  TA_CHECK(issueSet(bandNumberToUID(bandNumber), toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status getBandLockingOpal(int bandNumber,
               uint64_t *readLockEnabled, uint64_t *writeLockEnabled,
               uint64_t *readLocked, uint64_t *writeLocked, 
               uint64_t *lockOnPowerCycle, uint64_t *lockOnProgrammatic,
               char *pin) {
  parameters *results = &lastResults;
  status s;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  if (writeLockEnabled == NULL) {
    return API_ERROR("getBandLockingOpal: NULL writeLockEnabled");
  }
  if (readLockEnabled == NULL) {
    return API_ERROR("getBandLockingOpal: NULL readLockEnabled");
  }
  if (writeLocked == NULL) {
    return API_ERROR("getBandLockingOpal: NULL writeLocked");
  }
  if (readLocked == NULL) {
    return API_ERROR("getBandLockingOpal: NULL readLocked");
  }
  if (lockOnPowerCycle == NULL) {
    return API_ERROR("getBandLockingOpal: NULL lockOnPowerCycle");
  }
  if (lockOnProgrammatic == NULL) {
    return API_ERROR("getBandLockingOpal: NULL lockOnProgrammatic");
  }
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  TA_CHECK(issueGet(bandNumberToUID(bandNumber), NULL, results));
  TA_CHECK(issueCloseSession());
  TA_CHECK(getIdIntValue(results, LOCKING_TABLE_READLOCKENABLED_COLUMN_ID, readLockEnabled));
  TA_CHECK(getIdIntValue(results, LOCKING_TABLE_WRITELOCKENABLED_COLUMN_ID, writeLockEnabled));
  TA_CHECK(getIdIntValue(results, LOCKING_TABLE_READLOCKED_COLUMN_ID, readLocked));
  TA_CHECK(getIdIntValue(results, LOCKING_TABLE_WRITELOCKED_COLUMN_ID, writeLocked));
  if (lockOnPowerCycle != NULL) {
    status found = setSlotForId(results, LOCKING_TABLE_LOCKONRESET_COLUMN_ID);
    if (hasError(found)) {
      return ERROR_("Cannot read LockOnReset column");
    }
    if (hasError(getStartList(results))) {
      return ERROR_("LockOnReset value found, but is not a list");
    }
    *lockOnPowerCycle = 0; /* Presume false until shown otherwise */
    while (!peekEndList(results)) {
      uint64_t intValue;
      s = getIntValue(results, &intValue);
      if (hasError(s)) {
        return ERROR_("LockOnReset should be list of integers.");
      }
      if (intValue == LOR_POWERCYCLE) {
        *lockOnPowerCycle = 1;
      }
    }
    TA_CHECK(getEndList(results));
  }
  if (lockOnProgrammatic != NULL) {
    status found = setSlotForId(results, LOCKING_TABLE_LOCKONRESET_COLUMN_ID);
    if (hasError(found)) {
      return ERROR_("Cannot read LockOnReset column");
    }
    if (hasError(getStartList(results))) {
      return ERROR_("LockOnReset value found, but is not a list");
    }
    *lockOnProgrammatic = 0; /* Presume false until shown otherwise */
    while (!peekEndList(results)) {
      uint64_t intValue;
      s = getIntValue(results, &intValue);
      if (hasError(s)) {
        return ERROR_("LockOnReset should be list of integers.");
      }
      if (intValue == LOR_PROGRAMMATIC) {
        *lockOnProgrammatic = 1;
      }
    }
    TA_CHECK(getEndList(results));
  }
  return SUCCESS;
}

status getBandLockingEnterprise(int bandNumber,
                      uint64_t *readLockEnabled, uint64_t *writeLockEnabled,
                      uint64_t *readLocked, uint64_t *writeLocked, uint64_t *lockOnPowerCycle) {
  parameters *results = &lastResults;
  status found;
  status s;
  CHECK_TRANSPORT;
  if (writeLockEnabled == NULL) {
    return API_ERROR("getBandLockingEnterprise: NULL writeLockEnabled");
  }
  if (readLockEnabled == NULL) {
    return API_ERROR("getBandLockingEnterprise: NULL readLockEnabled");
  }
  if (writeLocked == NULL) {
    return API_ERROR("getBandLockingEnterprise: NULL writeLocked");
  }
  if (readLocked == NULL) {
    return API_ERROR("getBandLockingEnterprise: NULL readLocked");
  }
  if (lockOnPowerCycle == NULL) {
    return API_ERROR("getBandLockingEnterprise: NULL lockOnPowerCycle");
  }
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueGet(bandNumberToUID(bandNumber), NULL, results));
  TA_CHECK(issueCloseSession());
  TA_CHECK(getNamedIntValue(results, tcgTmpName("ReadLockEnabled"), readLockEnabled));
  TA_CHECK(getNamedIntValue(results, tcgTmpName("WriteLockEnabled"), writeLockEnabled));
  TA_CHECK(getNamedIntValue(results, tcgTmpName("ReadLocked"), readLocked));
  TA_CHECK(getNamedIntValue(results, tcgTmpName("WriteLocked"), writeLocked));
  found = setSlotForName(results, tcgTmpName("LockOnReset"));
  if (hasError(found)) {
    return ERROR_("Cannot read LockOnReset column");
  }
  if (hasError(getStartList(results))) {
    return ERROR_("LockOnReset value found, but is not a list");
  }
  *lockOnPowerCycle = 0; /* Presume false until shown otherwise */
  while (!peekEndList(results)) {
    uint64_t intValue;
    s = getIntValue(results, &intValue);
    if (hasError(s)) {
      return ERROR_("LockOnReset should be list of integers.");
    }
    if (intValue == LOR_POWERCYCLE) {
      *lockOnPowerCycle = 1;
    }
  }
  TA_CHECK(getEndList(results));
  return SUCCESS;
}

status setBandLockingOpal(int bandNumber, char *pin,
                      uint64_t *readLockEnabled, uint64_t *writeLockEnabled,
                      uint64_t *readLocked, uint64_t *writeLocked,
                      resetTypes_t *lockOnReset) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  resetParameters(toSet);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(setStartName(toSet));
  TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
  TA_CHECK(setStartList(toSet));
  /* We must pass the column's values to the Set method in the order they
     occur in the table in order to set all of them in one method call.
  */
  if (readLockEnabled != NULL) {
    TA_CHECK(setIdIntValue(toSet, LOCKING_TABLE_READLOCKENABLED_COLUMN_ID, *readLockEnabled));
  }
  if (writeLockEnabled != NULL) {
    TA_CHECK(setIdIntValue(toSet, LOCKING_TABLE_WRITELOCKENABLED_COLUMN_ID, *writeLockEnabled));
  }
  if (readLocked != NULL) {
    TA_CHECK(setIdIntValue(toSet, LOCKING_TABLE_READLOCKED_COLUMN_ID, *readLocked));
  }
  if (writeLocked != NULL) {
    TA_CHECK(setIdIntValue(toSet, LOCKING_TABLE_WRITELOCKED_COLUMN_ID, *writeLocked));
  }
  if (lockOnReset != NULL) {
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setIntValue(toSet, LOCKING_TABLE_LOCKONRESET_COLUMN_ID));
    TA_CHECK(setStartList(toSet));
    if ((*lockOnReset == POWERCYCLE_RESET) || (*lockOnReset == ALL_RESET)) {
      TA_CHECK(setIntValue(toSet, LOR_POWERCYCLE));
    } 
    if ((*lockOnReset == PROGRAMMATIC_RESET) || (*lockOnReset == ALL_RESET)) {
      TA_CHECK(setIntValue(toSet, LOR_PROGRAMMATIC));
    }
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
  }
  TA_CHECK(setEndList(toSet));
  TA_CHECK(setEndName(toSet));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  TA_CHECK(issueSet(bandNumberToUID(bandNumber), toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status setBandLockingEnterprise(int bandNumber, char *pin,
                      uint64_t *readLockEnabled, uint64_t *writeLockEnabled,
                      uint64_t *readLocked, uint64_t *writeLocked, resetTypes_t *lockOnReset) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  resetParameters(toSet);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  /* Empty cell_block - cell_block does not apply to Set on Objects */
  TA_CHECK(setStartList(toSet));
  TA_CHECK(setEndList(toSet));

  /* Values - list of list of Column-name/value named-pairs */
  TA_CHECK(setStartList(toSet));
  TA_CHECK(setStartList(toSet));
  /* We must pass the column's values to the Set method in the order they
     occur in the table in order to set all of them in one method call.
  */
  if (readLockEnabled != NULL) {
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("ReadLockEnabled"), *readLockEnabled));
  }
  if (writeLockEnabled != NULL) {
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("WriteLockEnabled"), *writeLockEnabled));
  }
  if (readLocked != NULL) {
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("ReadLocked"), *readLocked));
  }
  if (writeLocked != NULL) {
    TA_CHECK(setNamedIntValue(toSet, tcgTmpName("WriteLocked"), *writeLocked));
  }
  if (countParameters(toSet) <= 4) {
    return API_ERROR("setBandLocking: All locking parameters are null. At least one value to set must be specified.");
  }

  if (lockOnReset != NULL) {
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setByteValue(toSet, tcgTmpName("LockOnReset")));
    TA_CHECK(setStartList(toSet));
    if (*lockOnReset == POWERCYCLE_RESET) {
      TA_CHECK(setIntValue(toSet, LOR_POWERCYCLE));
    } 
    if ((*lockOnReset == PROGRAMMATIC_RESET) || (*lockOnReset == ALL_RESET)) {
      return API_ERROR("setBandLockingEnterprise: Programmatic Reset and All Reset are not supported on Enterprise.");
    }
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
  }

  TA_CHECK(setEndList(toSet));
  TA_CHECK(setEndList(toSet));
  TA_CHECK(issueAuthenticate(bandNumberToAuthority(bandNumber), tcgPin));
  TA_CHECK(issueSet(bandNumberToUID(bandNumber), toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status getMBRControl(uint64_t *enable, uint64_t *done, uint64_t *doneOnPowerCycle, uint64_t *doneOnProgrammatic) {
  parameters *results = &lastResults;
  status s;
  CHECK_TRANSPORT;
  if ((enable == NULL) && (done == NULL) && (doneOnPowerCycle == NULL) && (doneOnProgrammatic == NULL)) {
    return API_ERROR("getMBRControl: All parameters are NULL");
  }
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueGet(MBRCONTROL_TABLE_UID, NULL, results));
  TA_CHECK(issueCloseSession());
  if (enable != NULL) {
    s = getIdIntValue(results, MBRCONTROL_TABLE_ENABLE_COLUMN_ID, enable);
    if (hasError(s)) {
      return ERROR_("Enable column data not available.");
    }
  }
  if (done != NULL) {
    s = getIdIntValue(results, MBRCONTROL_TABLE_DONE_COLUMN_ID, done);
    if (hasError(s)) {
      return ERROR_("Done column data not available.");
    }
  }
  if (doneOnPowerCycle != NULL) {
    status found = setSlotForId(results, MBRCONTROL_TABLE_DONEONRESET_COLUMN_ID);
    if (hasError(found)) {
      return ERROR_("Cannot read DoneOnReset column");
    }
    if (hasError(getStartList(results))) {
      return ERROR_("DoneOnReset value found, but is not a list");
    }
    *doneOnPowerCycle = 0; /* Presume false until shown otherwise */
    while (!peekEndList(results)) {
      uint64_t intValue;
      s = getIntValue(results, &intValue);
      if (hasError(s)) {
        return ERROR_("DoneOnReset should be list of integers.");
      }
      if (intValue == LOR_POWERCYCLE) {
        *doneOnPowerCycle = 1;
      }
    }
    TA_CHECK(getEndList(results));
  }
  if (doneOnProgrammatic != NULL) {
    status found = setSlotForId(results, MBRCONTROL_TABLE_DONEONRESET_COLUMN_ID);
    if (hasError(found)) {
      return ERROR_("Cannot read DoneOnReset column");
    }
    if (hasError(getStartList(results))) {
      return ERROR_("DoneOnReset value found, but is not a list");
    }
    *doneOnProgrammatic = 0; /* Presume false until shown otherwise */
    while (!peekEndList(results)) {
      uint64_t intValue;
      s = getIntValue(results, &intValue);
      if (hasError(s)) {
        return ERROR_("DoneOnReset should be list of integers.");
      }
      if (intValue == LOR_PROGRAMMATIC) {
        *doneOnProgrammatic = 1;
      }
    }
    TA_CHECK(getEndList(results));
  }
  return SUCCESS;
}

status setMBRControl(char *pin, uint64_t *enable, uint64_t *done, resetTypes_t *doneOnReset) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin;
  CHECK_TRANSPORT;

  resetParameters(toSet);
  TA_CHECK(setStartName(toSet));
  TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
  TA_CHECK(setStartList(toSet));
  if (enable != NULL) {
    TA_CHECK(setIdIntValue(toSet, MBRCONTROL_TABLE_ENABLE_COLUMN_ID, *enable));
  }
  if (done != NULL) {
    TA_CHECK(setIdIntValue(toSet, MBRCONTROL_TABLE_DONE_COLUMN_ID, *done));
  }
  if (doneOnReset != NULL) {
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setIntValue(toSet, MBRCONTROL_TABLE_DONEONRESET_COLUMN_ID));
    TA_CHECK(setStartList(toSet));
    if ((*doneOnReset == POWERCYCLE_RESET) || (*doneOnReset == ALL_RESET)) {
      TA_CHECK(setIntValue(toSet, LOR_POWERCYCLE));
    } 
    if ((*doneOnReset == PROGRAMMATIC_RESET) || (*doneOnReset == ALL_RESET)) {
      TA_CHECK(setIntValue(toSet, LOR_PROGRAMMATIC));
    }
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
  }
  TA_CHECK(setEndList(toSet));
  TA_CHECK(setEndName(toSet));
  tcgPin = msidDefaultPin(pin);

  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  TA_CHECK(issueSet(MBRCONTROL_TABLE_UID, toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status genKey(int bandNumber, char *admin1pin) {
  tcgByteValue *tcgPin = msidDefaultPin(admin1pin);
  CHECK_TRANSPORT;
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin));
  TA_CHECK(issueGenKey(bandNumber));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status erase(int bandNumber, char *eraseMasterPin) {
  tcgByteValue *tcgPin = msidDefaultPin(eraseMasterPin);
  CHECK_TRANSPORT;
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(issueAuthenticate("Admin1", tcgPin)); /* Can be Admins or UserN */
  } else {
    TA_CHECK(issueAuthenticate("EraseMaster", tcgPin));
  }
  TA_CHECK(issueErase(bandNumber));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status revertLockingSP(char *pin, uint64_t keepGlobalRangeKey) {
  parameters *toRevertSP = &lastToRevertSP;
  tcgByteValue *tcgPin;
  CHECK_TRANSPORT;
  resetParameters(toRevertSP);
  TA_CHECK(setIdIntValue(toRevertSP, REVERTSP_METHOD_KEEPGLOBALRANGEKEY_PARAMETER_ID, keepGlobalRangeKey));
  tcgPin = msidDefaultPin(pin);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("LockingSP"));
  TA_CHECK(issueAuthenticate("Admin1", tcgPin)); /* Can be Admins or UserN */
  TA_CHECK(issueRevertSP(toRevertSP));
  return SUCCESS;
}

status restoreToFactory(char *pin) {
  parameters *toRevertSP = &lastToRevertSP;
  tcgByteValue tcgPin;
  CHECK_TRANSPORT;
  resetParameters(toRevertSP);
  OPTIONAL_STACK_RESET;
  if (pin == NULL) {
    pin = "VUTSRQPONMLKJIHGFEDCBA9876543210";
  }
  tcgByteValueFromString(&tcgPin, pin);
  TA_CHECK(issueStartSession("AdminSP"));
  TA_CHECK(issueAuthenticate("PSID", &tcgPin));
  TA_CHECK(issueRevertSP(toRevertSP));
  return SUCCESS;
}

status generateRandom(char *spName, uint64_t *sizeInBytes, char *result) {
  int byteCount = (sizeInBytes != NULL) ? *sizeInBytes : 32;
  CHECK_TRANSPORT;
  if (spName == NULL ) {
    spName = "AdminSP";
  }
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession(spName));
  TA_CHECK(issueRandom(byteCount, result));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status getPortLocking(char *port, char *pin, uint64_t *locked, uint64_t *lockOnPowerCycle) {
  parameters *results = &lastResults;
  tcgByteValue *tcgPin;
  status s;
  status found;
  CHECK_TRANSPORT;
  if (port == NULL) {
    return API_ERROR("getPortLocking: NULL port");
  }
  if ((locked == NULL) && (lockOnPowerCycle == NULL)) {
    return API_ERROR("getPortLocking: Both the locked and lockOnPowerCycle parameters are NULL");
  }
  tcgPin = msidDefaultPin(pin);
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("AdminSP"));
  TA_CHECK(issueAuthenticate(portNameToAuthority(port), tcgPin));
  TA_CHECK(issueGet(portNameToUID(port), NULL, results));
  TA_CHECK(issueCloseSession());
  if (locked != NULL) {
    if (ourTransport->ssc == OPAL_SSC) {
      s = getIdIntValue(results, PORTLOCKING_TABLE_PORTLOCKED_COLUMN_ID, locked);
    } else {
      s = getNamedIntValue(results, tcgTmpName("PortLocked"), locked);
    }
    if (hasError(s)) {
      return ERROR_("PortLocked column data not available.");
    }
  }
  if (lockOnPowerCycle != NULL) {
    if (ourTransport->ssc == OPAL_SSC) {
      found = setSlotForId(results, PORTLOCKING_TABLE_LOCKONRESET_COLUMN_ID);
    } else {
      found = setSlotForName(results, tcgTmpName("LockOnReset"));
    }
    if (hasError(found)) {
      return ERROR_("Cannot read LockOnReset column");
    }
    if (hasError(getStartList(results))) {
      return ERROR_("LockOnReset value found, but is not a list");
    }
    *lockOnPowerCycle = 0; /* Presume false until shown otherwise */
    while (!peekEndList(results)) {
      uint64_t intValue;
      s = getIntValue(results, &intValue);
      if (hasError(s)) {
        return ERROR_("LockOnReset should be list of integers.");
      }
      if (intValue == LOR_POWERCYCLE) {
        *lockOnPowerCycle = 1;
      }
    }
    TA_CHECK(getEndList(results));
  }
  return SUCCESS;
}

status setPortLocking(char *port, char *pin, uint64_t *locked, uint64_t *lockOnPowerCycle) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin;
  CHECK_TRANSPORT;

  if (port == NULL) {
    return API_ERROR("setPortLocking: NULL port");
  }
  resetParameters(toSet);
  if (ourTransport->ssc == OPAL_SSC) {
    TA_CHECK(setStartName(toSet));
    TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
    TA_CHECK(setStartList(toSet));
    if (locked != NULL) {
      TA_CHECK(setIdIntValue(toSet, PORTLOCKING_TABLE_PORTLOCKED_COLUMN_ID, *locked));
    }
    if (lockOnPowerCycle != NULL) {
      TA_CHECK(setStartName(toSet));
      TA_CHECK(setIntValue(toSet, PORTLOCKING_TABLE_LOCKONRESET_COLUMN_ID));
      TA_CHECK(setStartList(toSet));
      if (*lockOnPowerCycle) {
        TA_CHECK(setIntValue(toSet, LOR_POWERCYCLE));
      }
      TA_CHECK(setEndList(toSet));
      TA_CHECK(setEndName(toSet));
    }
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndName(toSet));
  } else {
    /* Empty cell_block - cell_block does not apply to Set on Objects */
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setEndList(toSet));
    /* Values - list of list of Column-name/value named-pairs */
    TA_CHECK(setStartList(toSet));
    TA_CHECK(setStartList(toSet));
    if (locked != NULL) {
      TA_CHECK(setNamedIntValue(toSet, tcgTmpName("PortLocked"), *locked));
    }
    if (lockOnPowerCycle != NULL) {
      TA_CHECK(setStartName(toSet));
      TA_CHECK(setByteValue(toSet, tcgTmpName("LockOnReset")));
      TA_CHECK(setStartList(toSet));
      if (*lockOnPowerCycle) {
        TA_CHECK(setIntValue(toSet, LOR_POWERCYCLE));
      }
      TA_CHECK(setEndList(toSet));
      TA_CHECK(setEndName(toSet));
    }
    if (countParameters(toSet) <= 4) {
      return API_ERROR("setPortLocking: All parameters are null. At least one parameter must be non-NULL.");
    }
    TA_CHECK(setEndList(toSet));
    TA_CHECK(setEndList(toSet));
  }

  tcgPin = msidDefaultPin(pin);

  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("AdminSP"));
  TA_CHECK(issueAuthenticate(portNameToAuthority(port), tcgPin));
  TA_CHECK(issueSet(portNameToUID(port), toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}

status setTperReset(char *pin, uint64_t enable) {
  parameters *toSet = &lastToSet;
  tcgByteValue *tcgPin = msidDefaultPin(pin);
  CHECK_TRANSPORT;
  resetParameters(toSet);
  TA_CHECK(setStartName(toSet));
  TA_CHECK(setIntValue(toSet, SET_METHOD_VALUES_PARAMETER_ID));
  TA_CHECK(setStartList(toSet));
  TA_CHECK(setIdIntValue(toSet, TPERINFO_TABLE_PROGRAMMATICRESETENABLE_COLUMN_ID, enable));
  TA_CHECK(setEndList(toSet));
  TA_CHECK(setEndName(toSet));
  OPTIONAL_STACK_RESET;
  TA_CHECK(issueStartSession("AdminSP"));
  TA_CHECK(issueAuthenticate("SID", tcgPin));
  TA_CHECK(issueSet(TPERINFO_TABLE_UID, toSet));
  TA_CHECK(issueCloseSession());
  return SUCCESS;
}
