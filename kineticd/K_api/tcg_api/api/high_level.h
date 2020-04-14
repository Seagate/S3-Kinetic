#ifndef HIGH_LEVEL_H
#define HIGH_LEVEL_H

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
   Will need to add specifics for secure messaging feature descriptor
*/

/**
 * @file high_level.h
 *
 * High-level functions map to common and recommended Enterprise SSC, 
 * Opal SSC, and Opal SSC v2.00 use cases. These functions are implemented 
 * with utility functions and low_level.h functions, and are intended for 
 * "down the fairway" uses and not for testing or exploration.
 *
 */

#include <stdint.h>
#include "status_codes.h"
#include "transport.h"
#include "level_0_discovery.h"
#include "parameters.h"
#include "utilities.h"


/**
 * An arbitrary but "big enough" value to capture the expected
 * number of logical ports from the Logical Port Descriptor.
 * If more ports than this number are returned, only the first
 * MAX_LOGICAL_PORTS of them will be saved.
 */
#define MAX_LOGICAL_PORTS  20

/**
 *  The number of available COMIDs.
 *  Increase the number as needed.
 *  If the device returns more than this number of ComIDs, 
 *  then the rest will be discarded.
 *  The TCG Enterprise SSC requires at least two.
 *  The TCG Opal SSC requires at least one.
 *  The TCG Opal SSC v2.00 requires at least one.
 */
#define MAX_COMIDS  2

/**
 * This is a small amount of the most useful information that we've
 * culled from TCG commands during discovery.
 */


typedef struct {
  unsigned int fdTPerSeen :1;     /**< Set to 1 when the TPer Feature Descriptor is parsed */
  unsigned int fdLockingSeen :1;  /**< Set to 1 when the Locking Feature Descriptor is parsed */
  unsigned int fdOpalSSCSeen :1; /**< Set to 1 when the Opal SSC Feature Descriptor is parsed */
  unsigned int fdOpalSSCv2Seen :1; /**< Set to 1 when the Opal SSC v2.00 Feature Descriptor is parsed */
  unsigned int fdSingleUserModeSeen :1; /**< Set to 1 when the Single User Mode Feature Descriptor is parsed */
  unsigned int fdDataStoreTableSeen :1; /**< Set to 1 when the DataStore Table Feature Descriptor is parsed */
  unsigned int fdEnterpriseSSCSeen :1; /**< Set to 1 when the Enterprise SSC Feature Descriptor is parsed */
  unsigned int fdLogicalPortSeen :1;   /**< Set to 1 when the Logical Port Feature Descriptor is parsed */
  unsigned int fdGeometrySeen :1;      /**< Set to 1 when the Geometry Feature Descriptor is parsed */
  unsigned int fdSecureMessagingSeen :1;      /**< Set to 1 when the Secure messaging Feature Descriptor is parsed */
  unsigned int fdActivationSeen :1;      /**< Set to 1 when the Activation Feature Descriptor is parsed */

  uint32_t      dataStructureVersion; /**< From the Level 0 Discovery response header */
  unsigned char driveSecurityLifeCycleState; /**< Only valid if this is a Seagate Enterprise or Opal Drive */

  unsigned int streamingSupported : 1;  /**< From the TPer Feature Descriptor */
  unsigned int syncSupported : 1;     /**< From the TPer Feature Descriptor */
  unsigned int mbrDone : 1;           /**< From the Locking Feature Descriptor */
  unsigned int mbrEnabled : 1;        /**< From the Locking Feature Descriptor */
  unsigned int lockingEnabled : 1;    /**< From the Locking Feature Descriptor */
  unsigned int lockingSupported : 1;  /**< From the Locking Feature Descriptor */
  unsigned int mediaEncryption : 1;   /**< From the Locking Feature Descriptor */
  unsigned int rangeCrossing : 1;     /**< From the Enterprise SSC Feature Descriptor */
  uint16_t comIDCount;        /**< From the Enterprise SSC Feature Descriptor */
  uint16_t comIDs[MAX_COMIDS]; /**< derived/calculated from the Enterprise SSC Feature Descriptor. Unused slots will be set to 0. */
  /* Secure Messaging */
  uint16_t SPCount;                	/**< From the Secure Messaging Feature Descriptor  Number of SP's supported*/
  uint64_t SP[MAX_SPIDS];		/**< From the Secure Messaging Feature Descriptor  array of SP's read*/
  uint16_t CSCount;         		/**< From the Secure Messaging Feature Descriptor  Number of CipherSuites available*/
  uint32_t CS[MAX_CIPHERSUITES];   	/**< From the Secure Messaging Feature Descriptor  array of CipherSuites read*/
  unsigned int Certificate_Request :1;	/**< From the Secure Messaging Feature Descriptor */ 
  unsigned int Server_Certificate :1;	/**< From the Secure Messaging Feature Descriptor */ 
  unsigned int Renegotiation :1;	/**< From the Secure Messaging Feature Descriptor */ 
  unsigned int Compression :1;		/**< From the Secure Messaging Feature Descriptor */ 
  unsigned int Session_Resumption :1;	/**< From the Secure Messaging Feature Descriptor */ 
  unsigned int Activated :1;		/**< From the Secure Messaging Feature Descriptor */ 
  /* end Secure Messaging */

  uint16_t numLockingSPAdminAuthoritiesSupported; /**< From the Opal SSC v2.00 Feature Descriptor */
  uint16_t numLockingSPUserAuthoritiesSupported;    /**< From the Opal SSC v2.00 Feature Descriptor */
  unsigned char initialC_PIN_SIDPINIndicator;           /**< From the Opal SSC v2.00 Feature Descriptor */
  unsigned char behaviorOfC_PIN_SIDPINUponTperRevert;   /**< From the Opal SSC v2.00 Feature Descriptor */
  uint32_t numLockingObjectsSupported;    /**< From the Single User Mode Feature Descriptor */
  unsigned int policy : 1;                /**< From the Single User Mode Feature Descriptor */
  unsigned int all : 1;                   /**< From the Single User Mode Feature Descriptor */
  unsigned int any : 1;                   /**< From the Single User Mode Feature Descriptor */
  uint16_t maxNumDataStoreTables;         /**< From the DataStore Table Feature Descriptor */
  uint32_t maxTotalSizeDataStoreTables;   /**< From the DataStore Table Feature Descriptor */
  uint32_t dataStoreTableSizeAlignment;   /**< From the DataStore Table Feature Descriptor */
  uint64_t      maxPacketSize;             /**< From the Properties method results */
  uint64_t      maxComPacketSize;          /**< From the Properties method results */
  uint64_t      maxResponseComPacketSize;  /**< From the Properties method results */
  uint64_t      maxAuthentications;        /**< From the Properties method results */
  uint64_t      defaultSessionTimeout;     /**< From the Properties method results */
  uint64_t      maxSessionTimeout;         /**< From the Properties method results */
  uint64_t      minSessionTimeout;         /**< From the Properties method results */

  unsigned int alignmentRequired : 1;     /**< From the Geometry Feature Descriptor - Seagate or Opal SSC v2.00 unique */
  uint32_t logicalBlockSize;          /**< From the Geometry Feature Descriptor - Seagate or Opal SSC v2.00 unique */
  uint64_t alignmentGranularity;      /**< From the Geometry Feature Descriptor - Seagate or Opal SSC v2.00 unique */
  uint64_t lowestAlignedLBA;          /**< From the Geometry Feature Descriptor - Seagate or Opal SSC v2.00 unique */
  
  uint64_t      logicalPortCount;                      /**< How many entries in the following arrays are valid */
  uint32_t      logicalPortIDs[MAX_LOGICAL_PORTS];     /**< From the Logical Port Feature Descriptor - Seagate unique */
  unsigned char logicalPortLocked[MAX_LOGICAL_PORTS];  /**< From the Logical Port Feature Descriptor - Seagate unique */
} discovered_data;

/** The bands that will be activated in Single User Mode.
 *  For Opal SSC or Opal SSC v2.00 with Single User Mode only.
 */
typedef enum {NONE_BANDS,
              ALL_BANDS
              } sumBands_t;

/** The different values for the RangeStartRangeLengthPolicy
 *  parameter of the Activate method in Single User Mode.
 *  For Opal SSC or Opal SSC v2.00 with Single User Mode only.
 */
typedef enum {USER_POLICY,
              ADMINS_POLICY
              } rangeStartRangeLengthPolicy_t;

/** The different values for the DoneOnReset parameter of
 *  the setMBRControl function.
 *  For Opal SSC or Opal SSC v2.00 only.
 */
typedef enum {NONE_RESET,
              POWERCYCLE_RESET,
              PROGRAMMATIC_RESET,
              ALL_RESET
              } resetTypes_t;

/**
 * This function takes a pointer to discovered data and determines if it describes 
 * an Opal SSC drive.
 * Returns 0 if 'd' is NULL or if the data does not describe an Opal SSC drive.
 *
 * @param[in] d The discovered_data to analyze.
 * @return value is a boolean (non-zero is true, zero is false).
 */
extern int isOpalDrive(discovered_data *d);

/**
 * This function takes a pointer to discovered data and determines if it describes 
 * an Opal SSC v2.00 drive.
 * Returns 0 if 'd' is NULL or if the data does not describe an Opal SSC v2.00 drive.
 *
 * @param[in] d The discovered_data to analyze.
 * @return value is a boolean (non-zero is true, zero is false).
 */
extern int isOpalv2Drive(discovered_data *d);

/**
 * This function takes a pointer to discovered data and determines if it describes 
 * an Enterprise SSC drive.
 * Returns 0 if 'd' is NULL or if the data does not describe an Enterprise SSC drive.
 *
 * @param[in] d The discovered_data to analyze.
 * @return value is a boolean (non-zero is true, zero is false).
 */
extern int isEnterpriseDrive(discovered_data *d);

/**
 * This function takes a pointer to discovered data and determines if it describes 
 * a drive that supports the Opal SSC or Opal SSC v2.00 Single User Mode feature set.
 * Returns 0 if 'd' is NULL or if the data does not describe a drive that supports
 * Single User Mode.
 *
 * @param[in] d The discovered_data to analyze.
 * @return value is a boolean (non-zero is true, zero is false).
 */
extern int singleUserModeSupported(discovered_data *d);

/**
 * This function is used to discover whether a device supports Enterprise SSC,
 * Opal SSC, or Opal SSC v2.00
 * and to retrieve device communications capabilities. It issues a Security
 * Protocol 0x01 discovery command (Level 0 Discovery) and parses the results
 * to determine device characteristics. It also invokes the Properties method and
 * parses the Properties response. It also attempts to retrieve MSID for use
 * as a default PIN in the other functions, as described there.
 *
 * NOTE: It is beyond the scope of this function to determine if the drive is
 *       capable of responding to TCG Security Protocol commands.
 * NOTE: Failure to invoke this function before calling other functions in this
 *       file may result in failures of those functions, as they rely
 *       on the values retrieved by this function for many of their default
 *       parameter values.
 * NOTE: The values from drive are cached at this level, changing the
 *       drive and/or transport requires that this function be called again
 *       or stale cached data will likely result in failures of the other
 *       functions in this module.
 *
 * @param[in] t The transport to use (calls setTransport in low_level and is
 *              thus shared with those functions).
 * @param[out] d The discovered_data to fill in.
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status discover(transport *t, discovered_data *d);

/**
 * Same as discover() but does not attempt to retrieve MSID.
 * This exists so that it can be used when a session is already open and either
 * MSID is already known, or is not needed.
 *
 * @param[in] t The transport to use (calls setTransport in low_level and is
 *              thus shared with those functions).
 * @param[out] d The discovered_data to fill in.
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status discoverNoMSID(transport *t, discovered_data *d);

/**
 * This function is used to parse through a buffer holding the results of
 * level 0 discovery. Data relevant to the discovered_data structure will
 * be filled in there (some fields of discovered_data are filled in elsewhere).
 *
 * @param[in] blob (required) pointer to the raw level 0 discovery data.
 * @param[out] d (required) pointer to the discovered_data to be filled in.
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status parseThroughDiscoveryData(discoveryData blob, discovered_data *d);

/**
 * Debugging function to print the contents of a discovered_data structure.
 *
 * @param[in] d (required) Pointer to the discovered_data to be printed.
 */
extern void printDiscoveryData(discovered_data *d);

/**
 * This function activates data (user LBA) locking on the device. Only valid
 * for Opal SSC or Opal SSC v2.00.
 *
 * @param[in] pin (optional) The PIN of the SID authority (if NULL, MSID is used)
 * @param[in] sumBands (required) Whether all bands will be activated in
 * Single User Mode or no bands will be activated in Single User Mode
 * @param[in] policy (required) The ownership policy for the
 * RangeStart, RangeLength, and CommonName columns of all Single User Mode bands
 * (this parameter is ignored if sumBands is set to NONE)
 * @param[in] numDataStoreTables (required) The number of 16KB (0x4000-byte)
 * DataStore tables to be created (if 0, one DataStore Table of the maximum
 * size the TPer supports will be created)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status activateBandLocking(char *pin, sumBands_t sumBands,
                      rangeStartRangeLengthPolicy_t policy,
                      uint32_t numDataStoreTables);

/**
 * This function reactivates the Locking SP. Only valid for Opal SSC or 
 * Opal SSC v2.00 drives that support the Single User Mode feature set.
 *
 * @param[in] pin (optional) The current PIN of the Admin1 authority (if NULL, 
 * MSID is used)
 * @param[in] sumBands (required) Whether all bands will be reactivated in
 * Single User Mode or no bands will be reactivated in Single User Mode
 * @param[in] policy (required) The ownership policy for the
 * RangeStart, RangeLength, and CommonName columns of all Single User Mode bands
 * (this parameter is ignored if sumBands is set to NONE)
 * @param[in] newAdmin1Pin (optional) The new Admin1 PIN to be set (if NULL,
 * Admin1 PIN is not changed)
 * @param[in] numDataStoreTables (required) The number of 16KB (0x4000-byte)
 * DataStore tables to be created (if 0, one DataStore Table of the maximum
 * size the TPer supports will be created)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status reactivateBandLocking(char *pin, sumBands_t sumBands,
                      rangeStartRangeLengthPolicy_t policy,
                      char *newAdmin1Pin, uint32_t numDataStoreTables);

/**
 * This function retrieves the enable/disable of the specified authority
 * in the Locking SP (Security Provider).
 *
 * @param[in] authority (required) The specified authority
 * @param[in] pin (optional) The PIN of the specified authority (if NULL, MSID is used)
 * @param[out] enabled (required) Set to the value of the Enabled column
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status getAuthority(char *authority, char *pin, uint64_t *enabled);

/**
 * This function enables or disables the specified authority in the Locking
 * SP.
 *
 * @param[in] authority (required) The authority to set
 * @param[in] pin (optional) The PIN of the EraseMaster authority (if NULL, MSID is used)
 * @param[in] enabled (required)The value that will be set for the Enabled column
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setAuthority(char *authority, char *pin, uint64_t enabled);

/**
 * This function sets the PIN of the specified authority to a user-defined PIN.
 *
 * @param[in] authority (required) The authority whose PIN is to be set
 * @param[in] SP (required) The SP of the authority whose PIN is to be set
 * @param[in] newPin (required) The new PIN to set
 * @param[in] oldPin (optional) The current PIN of the authority (if NULL, MSID is used)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setPin(char *authority, char *SP, char *newPin, char *oldPin);

/**
 * This function retrieves the starting LBA number and the length (in LBAs)
 * of the specified band on an Opal SSC or Opal SSC v2.00 drive.
 *
 * @param[in] bandNumber (required) The band number (0-15) of interest
 * @param[out] startingLBA (required) Set to the value of the StartingLBA column
 * @param[out] length (required) Set to the value of the Length column
 * @param[in] pin (optional) The PIN of the Admin1 authority (if NULL, MSID is used)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status getBandOpal(int bandNumber, uint64_t *startingLBA, uint64_t *length, char *pin);

/**
 * This function retrieves the starting LBA number and the length (in LBAs)
 * of the specified band on an Enterprise SSC drive.
 *
 * @param[in] bandNumber (required) The band number (0-15) of interest
 * @param[out] startingLBA (required) Set to the value of the StartingLBA column
 * @param[out] length (required) Set to the value of the Length column
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status getBandEnterprise(int bandNumber, uint64_t *startingLBA, uint64_t *length);

/**
 * This function configures the starting LBA number and the length (in LBAs)
 * of the specified band.
 *
 * @param[in] bandNumber (required) The band number (0-15) to set
 * @param[in] startingLBA (required) The starting LBA number to set
 * @param[in] length (required) The length to set, in LBAs
 * @param[in] pin (optional) The PIN of the associated authority (if NULL, MSID is used)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setBand(int bandNumber, uint64_t startingLBA, uint64_t length, char *pin);

/**
 * This function retrieves values for the specified band on an Opal SSC or Opal SSC v2.00 drive.
 *
 * @param[in] bandNumber (required) The band number to get
 * @param[out] writeLockEnabled (required) Set to the WriteLockEnabled column of the band
 * @param[out] readLockEnabled (required) Set to the ReadLockEnabled column of the band
 * @param[out] writeLocked (required) Set to the WriteLocked column of the band
 * @param[out] readLocked (required) Set to the ReadLocked column of the band
 * @param[out] lockOnPowerCycle (required) Set to non-zero if PowerCycle is in the
 * LockOnReset column
 * @param[out] lockOnProgrammatic (required) Set to non-zero if Programmatic is in the
 * LockOnReset column
 * @param[in] pin (optional) The pin of the Admin1 authority. If NULL, MSID is used
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status getBandLockingOpal(int bandNumber,
                      uint64_t *writeLockEnabled, uint64_t *readLockEnabled,
                      uint64_t *writeLocked, uint64_t *readLocked, 
                      uint64_t *lockOnPowerCycle, uint64_t *lockOnProgrammatic,
                      char *pin);

/**
 * This function retrieves values for the specified band on an Enterprise SSC drive.
 *
 * @param[in] bandNumber (required) The band number to get
 * @param[out] readLockEnabled (required) Set to the ReadLockEnabled column of the band
 * @param[out] writeLockEnabled (required) Set to the WriteLockEnabled column of the band
 * @param[out] readLocked (required) Set to the ReadLocked column of the band
 * @param[out] writeLocked (required) Set to the WriteLocked column of the band
 * @param[out] lockOnPowerCycle (required) Set to non-zero if PowerCycle is in the
 * LockOnReset column
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status getBandLockingEnterprise(int bandNumber,
                      uint64_t *readLockEnabled,
                      uint64_t *writeLockEnabled,
                      uint64_t *readLocked,
                      uint64_t *writeLocked,
                      uint64_t *lockOnPowerCycle);

/**
 * This function sets values for the specified band on an Opal SSC or Opal SSC v2.00 drive.
 *
 * @param[in] bandNumber (required) The band number of the band whose columns are to be set
 * @param[in] pin (optional) The pin to use to authenticate the corresponding BandMaster authority. If NULL, MSID is used
 * @param[in] readLockEnabled (optional) If not NULL, used to set the ReadLockEnabled column of the band
 * @param[in] writeLockEnabled (optional) If not NULL, used to set the WriteLockEnabled column of the band
 * @param[in] readLocked (optional) If not NULL, used to set the ReadLocked column of the band
 * @param[in] writeLocked (optional) If not NULL, used to set the WriteLocked column of the band
 * @param[in] lockOnReset (optional) If not NULL, the LockOnReset column will be set to the
 * empty list, Power Cycle, Programmatic, or all (based on the value pointed to)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setBandLockingOpal(int bandNumber, char *pin,
                      uint64_t *readLockEnabled, uint64_t *writeLockEnabled,
                      uint64_t *readLocked, uint64_t *writeLocked,
                      resetTypes_t *lockOnReset);

/**
 * This function sets values for the specified band on an Enterprise SSC drive.
 *
 * @param[in] bandNumber (required) The band number of the band whose columns are to be set
 * @param[in] pin (optional) The pin to use to authenticate the corresponding BandMaster authority. If NULL, MSID is used
 * @param[in] readLockEnabled (optional) If not NULL, used to set the ReadLockEnabled column of the band
 * @param[in] writeLockEnabled (optional) If not NULL, used to set the WriteLockEnabled column of the band
 * @param[in] readLocked (optional) If not NULL, used to set the ReadLocked column of the band
 * @param[in] writeLocked (optional) If not NULL, used to set the WriteLocked column of the band
 * @param[in] lockOnReset (optional) If not NULL, the LockOnReset column will be set to the
 * empty list, Power Cycle, or Programmatic  (based on the value pointed to)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setBandLockingEnterprise(int bandNumber, char *pin,
                      uint64_t *readLockEnabled, uint64_t *writeLockEnabled,
                      uint64_t *readLocked, uint64_t *writeLocked,
                      resetTypes_t *lockOnReset);

/**
 * This function gets values of the MBRControl Table. For Opal SSC or 
 * Opal SSC v2.00 only.
 *
 * @param[out] enable (optional) If not NULL, set to the value of the Enable column.
 * @param[out] done (optional) If not NULL, set to the value of the Done column.
 * @param[out] doneOnPowerCycle (optional) If not NULL, set to non-zero if 
 * PowerCycle is in the DoneOnReset column value.
 * @param[out] doneOnProgrammatic (optional) If not NULL, set to non-zero 
 * if Programmatic is in the DoneOnReset column value.
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 *
 * NOTE: At least one of locked or lockOnPowerCycle must not be NULL.
 */
extern status getMBRControl(uint64_t *enable, uint64_t *done, uint64_t *doneOnPowerCycle, uint64_t *doneOnProgrammatic);

/**
 * This function sets values of the MBRControl Table to the specified states.
 * For Opal SSC or Opal SSC v2.00 only.
 * The the Enable value, the Done value, the DoneOnPowerCycle value, the 
 * DoneOnProgrammatic value, and the PIN of the Admin1 authority are optional 
 * arguments to this function. If the PIN is not specified, MSID is used.
 *
 * @param[in] pin (optional) The pin of the Admin1 authority (If NULL, MSID is used)
 * @param[in] enable (optional) If not NULL the value to be set for the Enable column
 * @param[in] done (optional) If not NULL the value to be set for the Done column
 * @param[in] doneOnReset (optional) if not NULL, the DoneOnReset column will be
 * set to the empty list, Power Cycle, Programmatic, or all (based on the value
 * pointed to)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setMBRControl(char *pin, uint64_t *enable, uint64_t *done, resetTypes_t *doneOnReset);

/**
 * This function securely erases the
 * specified band. It securely erases the band by eradicating the current
 * data encryption key for the band and generating a new one. For Opal SSC or 
 * Opal SSC v2.00 only
 *
 * @param[in] bandNumber (required) The band number (0-15) to be erased
 * @param[in] admin1Pin (optional) The PIN of the Admin1 authority (if NULL, MSID is used)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status genKey(int bandNumber, char *admin1Pin);


/**
 * This function securely erases and resets the access control on the
 * specified band. It securely erases the band by eradicating the current
 * data encryption key for the band and generating a new one. It resets
 * the access control by unlocking and disabling locking on the band. It
 * sets the PIN of the associated BandMaster authority back to MSID. Note
 * that this function does not disable locking on reset for the band.
 * For Enterprise SSC or for Opal SSC or Opal SSC v2.00 with Single User 
 * Mode only.
 *
 * @param[in] bandNumber (required) The band number (0-15) to be erased
 * @param[in] eraseMasterPin (optional) The PIN of the EraseMaster authority (if NULL, MSID is used)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status erase(int bandNumber, char *eraseMasterPin);

/**
 * This function reverts the LockingSP back to its original factory state. All
 * LBAs will return to Band0. All bands will be unlocked. Locking will
 * be disabled on all bands. Locking on reset will be disabled on all
 * bands. The PINs of the SID, EraseMaster, BandMaster0, and BandMaster1
 * authorities will be set to the MSID. The BandMaster2-15 authorities will
 * be disabled. The encryption key for Band0 will be eradicated and
 * regenerated if keepGlobalRangeKey is true. For Opal SSC or Opal SSC v2.00 only.
 *
 * @param[in] pin (optional) The PIN of the Admin1 authority (if NULL, MSID is used)
 * @param[in] keepGlobalRangeKey (required) Whether we want to keep the encryption
 * key for Band0 or not
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */

extern status revertLockingSP(char *pin, uint64_t keepGlobalRangeKey);

/**
 * This function restores the device back to its original factory state. All
 * LBAs will return to Band0. All bands will be unlocked. Locking will
 * be disabled on all bands. Locking on reset will be disabled on all
 * bands. The PINs of the SID, EraseMaster, BandMaster0, and BandMaster1
 * authorities will be set to the MSID. The BandMaster2-15 authorities will
 * be disabled. All encryption keys will be eradicated and regenerated.
 *
 * @param[in] pin (optional) The PIN of the PSID authority (if NULL, VUTSRQPONMLKJIHGFEDCBA9876543210 is used)
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status restoreToFactory(char *pin);

/**
 * This function uses the device to generate a random number.
 *
 * @param[in] spName (optional) The SP to use (if NULL, Admin SP is used)
 * @param[in] sizeInBytes (optional) The size of the random number (if NULL, 32 is used)
 * @param[out] result (required) The place to store the result. Must point to space to hold at least sizeInBytes bytes
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status generateRandom(char *spName, uint64_t *sizeInBytes, char *result);

/**
 * This function gets values of the specified port.
 *
 * @param[in] port (required) The name of the port whose locking info is being requested
 * @param[in] pin (optional) The pin to use (If NULL, MSID is used)
 * @param[out] locked (optional) It not NULL, set to the value of the Locked column
 * @param[out] lockOnPowerCycle (optional) If not NULL, set to non-zero if PowerCycle is in the LockOnReset column value.
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 *
 * NOTE: At least one of locked or lockOnPowerCycle must not be NULL.
 */
extern status getPortLocking(char *port, char *pin, uint64_t *locked, uint64_t *lockOnPowerCycle);

/**
 * This function sets values of the specified port to the specified states.
 * The port, the Locked value, the LockOnReset
 * value, and the PIN of the SID authority are optional arguments to this
 * function. If the PIN is not specified, MSID is used.
 *
 * @param[in] port (required) The name of the port whose locking info is being set
 * @param[in] pin (optional) The pin of the SID authority (If NULL, MSID is used)
 * @param[in] locked (optional) If not NULL the value to be set for the Locked column
 * @param[in] lockOnPowerCycle (optional) if not NULL, the LockOnReset column will be set to either the empty list or just the list containing PowerCycle (based on the value pointed to).
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setPortLocking(char *port, char *pin, uint64_t *locked, uint64_t *lockOnPowerCycle);

/**
 * This function enables or disables TPER_RESET by setting the 
 * ProgrammaticResetEnable column of the TPerInfo Table.
 * For Opal SSC or Opal SSC v2.00 only.
 *
 * @param[in] pin (optional) The pin of the SID authority (If NULL, MSID is used)
 * @param[in] enable (required) The value that will be set for the
 * ProgrammaticResetEnable column
 * @return SUCCESS if all goes well, otherwise the first error code encountered.
 */
extern status setTperReset(char *pin, uint64_t enable);

extern tcgByteValue msidPin;

#endif
