#ifndef TCG_CONSTANTS_H
#define TCG_CONSTANTS_H

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
/* CA 9-3-2014 Added TLS Start and Sync method ID's */
/* Do we add TLS Alert definitions in here?  are they defined in a TCG spec yet for value ? */

#define LEVEL_0_DISCOVERY_COM_ID                  0x0001 /**< As defined in section 4.3 of the TCG Core Spec document */

#define STACK_RESET_REQUEST_CODE              0x00000002 /**< As defined in section 3.3.4.7.5 of the TCG Core Spec document */
#define STACK_RESET_NO_RESPONSE_AVAILABLE         0x0000 /**< As defined in section 3.3.4.7.3 of the TCG Core Spec document */

#define STACK_RESET_STATUS_SUCCESS            0x00000000 /**< As defined in section 4.5.2 of the TCG Enterprise SSC document */
#define STACK_RESET_STATUS_FAILURE            0x00000001 /**< As defined in section 4.5.2 of the TCG Enterprise SSC document */

#define STACK_RESET_PROTOCOL_ID                   0x0002 /**< As defined in section 3.4.7.5 of the TCG Enterprise SSC document */
#define LEVEL_0_DISCOVERY_SECURITY_PROTOCOL_ID      0x01 /**< As defined in section 3.6.2.2 of the TCG Enterprise SSC document */
#define LEVEL_0_DISCOVERY_SECURITY_PROTOCOL_COMID 0x0001 /**< As defined in section 3.6.2.2 of the TCG Enterprise SSC document */

#define TPER_RESET_PROTOCOL_ID                      0x02 /**< As defined in section 3.2.3 of the TCG Opal SSC v2.00 document */
#define TPER_RESET_COMID                          0x0004 /**< As defined in section 3.2.3 of the TCG Opal SSC v2.00 document */

#define TCG_METHOD_CALL_PROTOCOL_ID               0x0001 /**< As defined in ... of the TCG ...document */

#define DATA_SUBPACKET_KIND                         0x00 /**<As defined by section 3.2.3.4.1.1.2 of the TCG Core Specification document */


/** As defined in Table 178 of the TCG Core Specification document */
#define SPUID                    ((unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x01")   /* "This SP" */

/** As defined in Table 178 of the TCG Core Specification document */
#define SMUID                    ((unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\xFF")   /* "The Session Manager" */

/**< As defined in Table 180 of the TCG Core Specification document */
#define PROPERTIES_METHOD_UID    ((unsigned char *)"\x00\x00\x00\x00\x00\x00\xFF\x01")

/**< As defined in Table 180 of the TCG Core Specification document */
#define START_SESSION_METHOD_UID ((unsigned char *)"\x00\x00\x00\x00\x00\x00\xFF\x02")

/**< As defined in Table 180 of the TCG Core Specification document */
#define SYNC_SESSION_METHOD_UID  ((unsigned char *)"\x00\x00\x00\x00\x00\x00\xFF\x03")

#ifdef SECURE_MESSAGING
/**< As defined in Sect 3.1.1 of the TCG Core Spec Addendum: Secure Messaging document */
#define START_TLS_SESSION_METHOD_UID ((unsigned char *)"\x00\x00\x00\x00\x00\x00\xFF\x12")

/**< As defined in Sect 3.1.2 of the TCG Core Spec Addendum: Secure Messaging  document */
#define SYNC_TLS_SESSION_METHOD_UID  ((unsigned char *)"\x00\x00\x00\x00\x00\x00\xFF\x13")
#endif

/**< As defined in Table 180 of the TCG Core Specification document */
#define CLOSE_SESSION_METHOD_UID ((unsigned char *)"\x00\x00\x00\x00\x00\x00\xFF\x06")

/**< As defined in Section 5.2.1 of the TCG Opal SSC Specification document */
#define ACTIVATE_METHOD_UID      ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x02\x03")

/**< As defined in Section 3.1.1.1 of the TCG Opal SSC Single User Mode Feature Set document */
#define REACTIVATE_METHOD_UID      ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x08\x01")

/**< As defined in Table 242 of the TCG Core Specification v2.00 document */
#define GET_METHOD_UID_OPAL           ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x16")

/**< As defined in Table 242 of the TCG Core Specification v2.00 document */
#define SET_METHOD_UID_OPAL           ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x17")

/**< As defined in Table 242 of the TCG Core Specification v2.00 document */
#define AUTHENTICATE_METHOD_UID_OPAL  ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x1C")

/**< As defined in Table 181 of the TCG Core Specification document */
#define GET_METHOD_UID_ENTERPRISE           ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x06")

/**< As defined in Table 181 of the TCG Core Specification document */
#define SET_METHOD_UID_ENTERPRISE           ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x07")

/**< As defined in Table 181 of the TCG Core Specification document */
#define AUTHENTICATE_METHOD_UID_ENTERPRISE  ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x0C")

/**< As defined in Table 242 of the TCG Core Specification v2.00 document */
#define GENKEY_METHOD_UID        ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x10")

/**< As defined in Table 28 of the TCG Opal SSC Specification document (referenced from the Seagate Product Requirements documentation) */
#define REVERTSP_METHOD_UID  ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x00\x11")

/**< As defined in Table 181 of the TCG Core Specification document */
#define RANDOM_METHOD_UID  ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x06\x01")

/**< As defined in Section 10.5.4.1 of the TCG Enterprise SSC Specification document */
#define ERASE_METHOD_UID  ((unsigned char *)"\x00\x00\x00\x06\x00\x00\x08\x03")

/**< As defined in Section 6, Table 12 of the TCG Enterprise SSC document */
#define STATUS_CODE_AUTHORITY_LOCKED_OUT            0x12


/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define START_LIST_TOKEN          0xF0

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define END_LIST_TOKEN            0xF1

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define START_NAME_TOKEN          0xF2

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define END_NAME_TOKEN            0xF3

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define CALL_TOKEN                0xF8

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define END_OF_DATA_TOKEN         0xF9

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define END_OF_SESSION_TOKEN      0xFA

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define START_TRANSACTION_TOKEN   0xFB

/** As defined in The Token Types Table (Table 04) of the TCG Core Specification document */
#define END_TRANSACTION_TOKEN     0xFC

#define TRANSACTION_COMMIT_STATUS_CODE        0x00

/**< As defined in Section 3.2.3.2.5 of the TCG Core Specification document */
#define TRANSACTION_ABORT_STATUS_CODE 0x01

/**< As defined in Table 111 reset_types of the TCG Opal SSC v2.00 document */
#define LOR_POWERCYCLE          0

/**< As defined in Table 111 reset_types of the TCG Opal SSC v2.00 document */
#define LOR_PROGRAMMATIC        3

/** As defined in Table 04 Token Types of the TCG Core Specification document */
#define METHOD_STATUS_SUCCESS       0

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_SUCCESS 0

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_NOT_AUTHORIZED 1

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_READ_ONLY 2

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_SP_BUSY 3

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_SP_FAILED 4

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_SP_DISABLED 5

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_SP_FROZEN 6

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_NO_SESSIONS_AVAILABLE 7

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INDEX_CONFLICT 8

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INSUFFICIENT_SPACE 9

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INSUFFICIENT_ROWS 10

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INVALID_COMMAND 11

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INVALID_PARAMETER 12

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INVALID_REFERENCE 13

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_INVALID_SECMSG_PROPERTIES 14

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_TPER_MALFUNCTION 15

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_TRANSACTION_FAILURE 16

/** As defined in Table 31 Status Codes of the TCG Core Specification document */
#define TCG_STATUS_RESPONSE_OVERFLOW 17

/** As defined in Table 12 Status Codes of the TCG SSC Enterprise document */
#define TCG_STATUS_AUTHORITY_LOCKED_OUT 18

/** As defined in Table 37 of the TCG Opal SSC v2.00 document */
#define MBRCONTROL_TABLE_UID  ((unsigned char *)"\x00\x00\x08\x03\x00\x00\x00\x01")

/** As defined in Table 37 of the TCG Opal SSC v2.00 document */
#define MBRCONTROL_TABLE_ENABLE_COLUMN_ID 1

/** As defined in Table 37 of the TCG Opal SSC v2.00 document */
#define MBRCONTROL_TABLE_DONE_COLUMN_ID 2

/** As defined in Table 37 of the TCG Opal SSC v2.00 document */
#define MBRCONTROL_TABLE_DONEONRESET_COLUMN_ID  3

/** As defined in Table 22 of the TCG Opal SSC v2.00 document */
#define TPERINFO_TABLE_UID  ((unsigned char *)"\x00\x00\x02\x01\x00\x03\x00\x01")

/** As defined in Table 21 of the TCG Opal SSC v2.00 document */
#define TPERINFO_TABLE_PROGRAMMATICRESETENABLE_COLUMN_ID 0x08

/** As defined in Table 20 of the TCG Opal SSC v2.00 document */
#define C_PIN_TABLE_PIN_COLUMN_ID 3

/** As defined in Table 240 of the TCG Core Specification v2.00 document */
#define LOCKING_TABLE_UID    ((unsigned char *)"\x00\x00\x08\x02\x00\x00\x00\x00")

/** As defined in Table 31 of the TCG Opal SSC v2.00 document */
#define AUTHORITY_TABLE_ENABLED_COLUMN_ID 5

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_RANGESTART_COLUMN_ID 3

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_RANGELENGTH_COLUMN_ID 4

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_READLOCKENABLED_COLUMN_ID 5

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_WRITELOCKENABLED_COLUMN_ID 6

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_READLOCKED_COLUMN_ID 7

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_WRITELOCKED_COLUMN_ID 8

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define LOCKING_TABLE_LOCKONRESET_COLUMN_ID 9

/** As defined in Section 5.1.4.2.3 of the TCG Core Specification v2.00 document */
#define CELL_BLOCK_STARTROW_ID 0x01

/** As defined in Section 5.1.4.2.3 of the TCG Core Specification v2.00 document */
#define CELL_BLOCK_ENDROW_ID 0x02

/** As defined in Section 5.3.3.12 of the TCG Core Specification v2.00 document */
#define AUTHENTICATE_METHOD_PROOF_PARAMETER_ID 0

/** As defined in Section 5.3.3.7 of the TCG Core Specification v2.00 document */
#define SET_METHOD_WHERE_PARAMETER_ID 0

/** As defined in Section 5.3.3.7 of the TCG Core Specification v2.00 document */
#define SET_METHOD_VALUES_PARAMETER_ID 1

/** As defined in Section 2.15 of the Seagate TCG Opal Data Structures document */
#define PORTLOCKING_TABLE_LOCKONRESET_COLUMN_ID 2

/** As defined in Section 2.15 of the Seagate TCG Opal Data Structures document */
#define PORTLOCKING_TABLE_PORTLOCKED_COLUMN_ID 3

/** As defined in Section 3.1.2.1 of the TCG Opal SSC Single User Mode Feature Set document */
#define ACTIVATE_METHOD_SUMSELECTIONLIST_PARAMETER_ID 0x060000

/** As defined in Section 3.1.2.1 of the TCG Opal SSC Single User Mode Feature Set document */
#define ACTIVATE_METHOD_POLICY_PARAMETER_ID 0x060001

/** As defined in Section 3.1.1 of the TCG Opal SSC Additional DataStore Tables Feature Set document */
#define ACTIVATE_METHOD_DATASTORETABLESIZES_PARAMETER_ID 0x060002

/** As defined in Section 3.1.1.1 of the TCG Opal SSC Single User Mode Feature Set document */
#define REACTIVATE_METHOD_SUMSELECTIONLIST_PARAMETER_ID 0x060000

/** As defined in Section 3.1.1.1 of the TCG Opal SSC Single User Mode Feature Set document */
#define REACTIVATE_METHOD_POLICY_PARAMETER_ID 0x060001

/** As defined in Section 3.1.1.1 of the TCG Opal SSC Single User Mode Feature Set document */
#define REACTIVATE_METHOD_ADMIN1PIN_PARAMETER_ID 0x060002

/** As defined in Section 3.1.2 of the TCG Opal SSC Additional DataStore Tables Feature Set document */
#define REACTIVATE_METHOD_DATASTORETABLESIZES_PARAMETER_ID 0x060003

/** As defined in Table 36 of the TCG Opal SSC v2.00 document */
#define REVERTSP_METHOD_KEEPGLOBALRANGEKEY_PARAMETER_ID 0x060000

#define TCG_UID_BYTE_COUNT 8

#endif
