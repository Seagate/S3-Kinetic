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


/*! @file
 *  Misc. utilities, converstion functions, etc.
 */

#include <stdio.h>
#include "utilities.h"
#include "low_level.h"

#define BAND_NUMBER_RANGE_CHECK(x) {            \
  if ((x < 0) || (x > 1023)) {                  \
    return NULL;                                \
  }                                             \
}

static unsigned char _bandNumberUID[8];
/**< As defined in Table 36 of the TCG Opal SSC v2.00 specification */
#define BAND_NUMBER_GLOBAL_UID_OPAL "\x00\x00\x08\x02\x00\x00\x00\x01" /* for global range */
#define BAND_NUMBER_UID_PREFIX_OPAL "\x00\x00\x08\x02\x00\x03" /* for all other locking ranges */
#define BAND_NUMBER_UID_PREFIX_ENTERPRISE "\x00\x00\x08\x02\x00\x00"
#define BAND_NUMBER_PREFIX_LEN_OPAL (sizeof(BAND_NUMBER_UID_PREFIX_OPAL)-1)  /* account for null termination on strings */
#define BAND_NUMBER_PREFIX_LEN_ENTERPRISE (sizeof(BAND_NUMBER_UID_PREFIX_ENTERPRISE)-1)  /* account for null termination on strings */

/** Return the UID for the given bandNumber.
 *  If the bandNumber is out of range (to large or small), then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 *  Implementation Note:
 *  BandNumber UIDs cannot start at 0, because a UID ending in four zero bytes
 *  is always the UID for an entire table. Hence we check the bandNumber is valid
 *  but then we increment it by 1 because Band 0 is ....00 00 00 01 NOT ...00 00 00 00
 */
uid bandNumberToUID(int bandNumber) {
  BAND_NUMBER_RANGE_CHECK(bandNumber);
  if (ourTransport->ssc == OPAL_SSC) {
    if (bandNumber == 0) {
      memcpy(_bandNumberUID, BAND_NUMBER_GLOBAL_UID_OPAL, sizeof(BAND_NUMBER_GLOBAL_UID_OPAL));
    } else {
      memcpy(_bandNumberUID, BAND_NUMBER_UID_PREFIX_OPAL, BAND_NUMBER_PREFIX_LEN_OPAL);
      _bandNumberUID[BAND_NUMBER_PREFIX_LEN_OPAL] =   (char)((bandNumber >> 8) & 0xFF);
      _bandNumberUID[BAND_NUMBER_PREFIX_LEN_OPAL+1] = (char) (bandNumber       & 0xFF);
    }
  } else {
    bandNumber++;
    memcpy(_bandNumberUID, BAND_NUMBER_UID_PREFIX_ENTERPRISE, BAND_NUMBER_PREFIX_LEN_ENTERPRISE);
    _bandNumberUID[BAND_NUMBER_PREFIX_LEN_ENTERPRISE] =   (char)((bandNumber >> 8) & 0xFF);
    _bandNumberUID[BAND_NUMBER_PREFIX_LEN_ENTERPRISE+1] = (char) (bandNumber       & 0xFF);
  }
  return _bandNumberUID;
}

static unsigned char _bandMEKUID[8];
/**< As defined in Table 36 of the TCG Opal SSC v2.00 specification */
#define BAND_MEK_GLOBAL_UID "\x00\x00\x08\x06\x00\x00\x00\x01" /* for global range */
#define BAND_MEK_UID_PREFIX "\x00\x00\x08\x06\x00\x03" /* for all other locking ranges */
#define BAND_MEK_PREFIX_LEN (sizeof(BAND_MEK_UID_PREFIX)-1)  /* account for null termination on strings */

/** Return the UID of the associated K_AES_256 object for the given bandNumber.
 *  If the bandNumber is out of range (to large or small), then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 *  Implementation Note:
 *  BandNumber UIDs cannot start at 0, because a UID ending in four zero bytes
 *  is always the UID for an entire table. Hence we check the bandNumber is valid
 *  but then we increment it by 1 because Band 0 is ....00 00 00 01 NOT ...00 00 00 00
 */
uid bandNumberToMEKUID(int bandNumber) {
  BAND_NUMBER_RANGE_CHECK(bandNumber);
  if (bandNumber == 0) {
    memcpy(_bandMEKUID, BAND_MEK_GLOBAL_UID, sizeof(BAND_MEK_GLOBAL_UID));
  } else {
    memcpy(_bandMEKUID, BAND_MEK_UID_PREFIX, BAND_MEK_PREFIX_LEN);
    _bandMEKUID[BAND_MEK_PREFIX_LEN] =   (char)((bandNumber >> 8) & 0xFF);
    _bandMEKUID[BAND_MEK_PREFIX_LEN+1] = (char) (bandNumber       & 0xFF);
  }
  return _bandMEKUID;
}

static char _bandNameForNumber[sizeof("BandMasterXXXX")];

/** Return the BandMaster Authority name for the given bandNumber.
 *  If the bandNumber is out of range (to large or small), then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 */
char *bandNumberToAuthority(int bandNumber) {
  BAND_NUMBER_RANGE_CHECK(bandNumber);
  sprintf(_bandNameForNumber, "BandMaster%d", bandNumber);
  return _bandNameForNumber;
}


typedef struct { char *portName; char *portAuthority; char *portUID; } _portInfoStruct;
static _portInfoStruct _portInfoMap[] = {
  { "Diagnostics",        "SID", "\x00\x01\x00\x02\x00\x01\x00\x01" }, /* Also need to authenticate as MakerSymK */
  { "FWDownload",         "SID", "\x00\x01\x00\x02\x00\x01\x00\x02" },
  { "UDS",                "SID", "\x00\x01\x00\x02\x00\x01\x00\x03" },
  { "Cross_Segment_FW_Download", "SID", "\x00\x01\x00\x02\x00\x01\x00\x0E" }, /* Also need to authenticate as MakerSymK */
  { "ActivationIEEE1667", "SID", "\x00\x01\x00\x02\x00\x01\x00\x0F" },  /* OPAL */
};

#define _PORT_INFO_COUNT (sizeof(_portInfoMap)/sizeof(_portInfoStruct))

static _portInfoStruct *_findPort(char *portName) {
  unsigned int i;
  for (i = 0; i < _PORT_INFO_COUNT; i++) {
    if (strcmp(portName, _portInfoMap[i].portName) == 0) {
      return &(_portInfoMap[i]);
    }
  }
  return NULL;
}

/** Return the name of the Authority controlling the given portName.
 *  If the portName is unknown, then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 */
char *portNameToAuthority(char *portName) {
  _portInfoStruct *pIS = _findPort(portName);
  if (pIS == NULL) {
    return NULL;
  }
  return pIS->portAuthority;
}

/** Return the UID associated with the given portName
 *  If the portName is unknown, then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 */
uid portNameToUID(char *portName) {
  _portInfoStruct *pIS = _findPort(portName);
  if (pIS == NULL) {
    return NULL;
  }
  return (uid)pIS->portUID;
}

typedef struct { char *name; char *UID; } _pinInfoStruct;
static _pinInfoStruct _pinInfoMap[] = {
    { "SID",             "\x00\x00\x00\x0B\x00\x00\x00\x01" },

    { "MSID",            "\x00\x00\x00\x0B\x00\x00\x84\x02" },

    { "BandMaster0",     "\x00\x00\x00\x0B\x00\x00\x80\x01" },
    { "BandMaster1",     "\x00\x00\x00\x0B\x00\x00\x80\x02" },
    { "BandMaster2",     "\x00\x00\x00\x0B\x00\x00\x80\x03" },
    { "BandMaster3",     "\x00\x00\x00\x0B\x00\x00\x80\x04" },
    { "BandMaster4",     "\x00\x00\x00\x0B\x00\x00\x80\x05" },
    { "BandMaster5",     "\x00\x00\x00\x0B\x00\x00\x80\x06" },
    { "BandMaster6",     "\x00\x00\x00\x0B\x00\x00\x80\x07" },
    { "BandMaster7",     "\x00\x00\x00\x0B\x00\x00\x80\x08" },
    { "BandMaster8",     "\x00\x00\x00\x0B\x00\x00\x80\x09" },
    { "BandMaster9",     "\x00\x00\x00\x0B\x00\x00\x80\x0A" },
    { "BandMaster10",    "\x00\x00\x00\x0B\x00\x00\x80\x0B" },
    { "BandMaster11",    "\x00\x00\x00\x0B\x00\x00\x80\x0C" },
    { "BandMaster12",    "\x00\x00\x00\x0B\x00\x00\x80\x0D" },
    { "BandMaster13",    "\x00\x00\x00\x0B\x00\x00\x80\x0E" },
    { "BandMaster14",    "\x00\x00\x00\x0B\x00\x00\x80\x0F" },
    { "BandMaster15",    "\x00\x00\x00\x0B\x00\x00\x80\x10" },

    { "EraseMaster",     "\x00\x00\x00\x0B\x00\x00\x84\x01" },

    { "User1",           "\x00\x00\x00\x0B\x00\x03\x00\x01" },
    { "User2",           "\x00\x00\x00\x0B\x00\x03\x00\x02" },
    { "User3",           "\x00\x00\x00\x0B\x00\x03\x00\x03" },
    { "User4",           "\x00\x00\x00\x0B\x00\x03\x00\x04" },
    { "User5",           "\x00\x00\x00\x0B\x00\x03\x00\x05" },
    { "User6",           "\x00\x00\x00\x0B\x00\x03\x00\x06" },
    { "User7",           "\x00\x00\x00\x0B\x00\x03\x00\x07" },
    { "User8",           "\x00\x00\x00\x0B\x00\x03\x00\x08" },
    { "User9",           "\x00\x00\x00\x0B\x00\x03\x00\x09" },
    { "User10",          "\x00\x00\x00\x0B\x00\x03\x00\x0A" },
    { "User11",          "\x00\x00\x00\x0B\x00\x03\x00\x0B" },
    { "User12",          "\x00\x00\x00\x0B\x00\x03\x00\x0C" },
    { "User13",          "\x00\x00\x00\x0B\x00\x03\x00\x0D" },
    { "User14",          "\x00\x00\x00\x0B\x00\x03\x00\x0E" },
    { "User15",          "\x00\x00\x00\x0B\x00\x03\x00\x0F" },
    { "User16",          "\x00\x00\x00\x0B\x00\x03\x00\x10" },

    { "Admin1",          "\x00\x00\x00\x0B\x00\x01\x00\x01" },
    { "Admin2",          "\x00\x00\x00\x0B\x00\x01\x00\x02" },
    { "Admin3",          "\x00\x00\x00\x0B\x00\x01\x00\x03" },
    { "Admin4",          "\x00\x00\x00\x0B\x00\x01\x00\x04" },
  };

#define _PIN_INFO_COUNT (sizeof(_pinInfoMap)/sizeof(_pinInfoStruct))

static _pinInfoStruct *_findPIN(char *pinName) {
  unsigned int i;
  for (i = 0; i < _PIN_INFO_COUNT; i++) {
    if (strcmp(pinName, _pinInfoMap[i].name) == 0) {
      return &(_pinInfoMap[i]);
    }
  }
  return NULL;
}
/** Return the UID associated with the given pinName.
 *  If the pinName is unknown, then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 */
uid pinNameToUID(char *pinName) {
  _pinInfoStruct *pIS = _findPIN(pinName);
  if (pIS == NULL) {
    return NULL;
  }
  return (uid)pIS->UID;
}


typedef struct { char *name; char *UID; } _authorityInfoStruct;
static _authorityInfoStruct _authorityInfoMap[] = {
    { "Anybody",         "\x00\x00\x00\x09\x00\x00\x00\x01" },
    { "MakerSymK",       "\x00\x00\x00\x09\x00\x00\x00\x04" },
    { "SID",             "\x00\x00\x00\x09\x00\x00\x00\x06" },

    { "MSID",            "\x00\x00\x00\x09\x00\x00\x84\x02" },

    { "PSID",            "\x00\x00\x00\x09\x00\x01\xFF\x01" },

    { "BandMaster0",     "\x00\x00\x00\x09\x00\x00\x80\x01" },
    { "BandMaster1",     "\x00\x00\x00\x09\x00\x00\x80\x02" },
    { "BandMaster2",     "\x00\x00\x00\x09\x00\x00\x80\x03" },
    { "BandMaster3",     "\x00\x00\x00\x09\x00\x00\x80\x04" },
    { "BandMaster4",     "\x00\x00\x00\x09\x00\x00\x80\x05" },
    { "BandMaster5",     "\x00\x00\x00\x09\x00\x00\x80\x06" },
    { "BandMaster6",     "\x00\x00\x00\x09\x00\x00\x80\x07" },
    { "BandMaster7",     "\x00\x00\x00\x09\x00\x00\x80\x08" },
    { "BandMaster8",     "\x00\x00\x00\x09\x00\x00\x80\x09" },
    { "BandMaster9",     "\x00\x00\x00\x09\x00\x00\x80\x0A" },
    { "BandMaster10",    "\x00\x00\x00\x09\x00\x00\x80\x0B" },
    { "BandMaster11",    "\x00\x00\x00\x09\x00\x00\x80\x0C" },
    { "BandMaster12",    "\x00\x00\x00\x09\x00\x00\x80\x0D" },
    { "BandMaster13",    "\x00\x00\x00\x09\x00\x00\x80\x0E" },
    { "BandMaster14",    "\x00\x00\x00\x09\x00\x00\x80\x0F" },
    { "BandMaster15",    "\x00\x00\x00\x09\x00\x00\x80\x10" },

    { "EraseMaster",     "\x00\x00\x00\x09\x00\x00\x84\x01" },

    { "User1",           "\x00\x00\x00\x09\x00\x03\x00\x01" },
    { "User2",           "\x00\x00\x00\x09\x00\x03\x00\x02" },
    { "User3",           "\x00\x00\x00\x09\x00\x03\x00\x03" },
    { "User4",           "\x00\x00\x00\x09\x00\x03\x00\x04" },
    { "User5",           "\x00\x00\x00\x09\x00\x03\x00\x05" },
    { "User6",           "\x00\x00\x00\x09\x00\x03\x00\x06" },
    { "User7",           "\x00\x00\x00\x09\x00\x03\x00\x07" },
    { "User8",           "\x00\x00\x00\x09\x00\x03\x00\x08" },
    { "User9",           "\x00\x00\x00\x09\x00\x03\x00\x09" },
    { "User10",          "\x00\x00\x00\x09\x00\x03\x00\x0A" },
    { "User11",          "\x00\x00\x00\x09\x00\x03\x00\x0B" },
    { "User12",          "\x00\x00\x00\x09\x00\x03\x00\x0C" },
    { "User13",          "\x00\x00\x00\x09\x00\x03\x00\x0D" },
    { "User14",          "\x00\x00\x00\x09\x00\x03\x00\x0E" },
    { "User15",          "\x00\x00\x00\x09\x00\x03\x00\x0F" },
    { "User16",          "\x00\x00\x00\x09\x00\x03\x00\x10" },

    { "Admin1",          "\x00\x00\x00\x09\x00\x01\x00\x01" },
    { "Admin2",          "\x00\x00\x00\x09\x00\x01\x00\x02" },
    { "Admin3",          "\x00\x00\x00\x09\x00\x01\x00\x03" },
    { "Admin4",          "\x00\x00\x00\x09\x00\x01\x00\x04" },
  };

#define _AUTHORITY_INFO_COUNT (sizeof(_authorityInfoMap)/sizeof(_authorityInfoStruct))

static _authorityInfoStruct *_findAuthority(char *authorityName) {
  unsigned int i;
  for (i = 0; i < _AUTHORITY_INFO_COUNT; i++) {
    if (strcmp(authorityName, _authorityInfoMap[i].name) == 0) {
      return &(_authorityInfoMap[i]);
    }
  }
  return NULL;
}
/** Return the UID associated with the given authorityName.
 *  If the authorityName is unknown, then NULL is returned.
 *  NOTE: The return value points into fixed static memory and should not be changed!
 */
uid authorityNameToUID(char *authorityName) {
  _authorityInfoStruct *aIS = _findAuthority(authorityName);
  if (aIS == NULL) {
    return NULL;
  }
  return (uid)aIS->UID;
}


typedef struct { char *name; char *UID; } _spNameInfoStruct;

static _spNameInfoStruct _spNameMapOpal[] = {
    { "AdminSP", "\x00\x00\x02\x05\x00\x00\x00\x01" },
    { "LockingSP", "\x00\x00\x02\x05\x00\x00\x00\x02" },
  };

static _spNameInfoStruct _spNameMapEnterprise[] = {
    { "AdminSP", "\x00\x00\x02\x05\x00\x00\x00\x01" },
    { "LockingSP", "\x00\x00\x02\x05\x00\x01\x00\x01" },
  };

#define _SPNAME_INFO_COUNT_OPAL (sizeof(_spNameMapOpal)/sizeof(_spNameInfoStruct))
#define _SPNAME_INFO_COUNT_ENTERPRISE (sizeof(_spNameMapEnterprise)/sizeof(_spNameInfoStruct))

static _spNameInfoStruct *_findSPName(char *spName) {
  unsigned int i;
  if (ourTransport->ssc == OPAL_SSC) {
    for (i = 0; i < _SPNAME_INFO_COUNT_OPAL; i++) {
      if (strcmp(spName, _spNameMapOpal[i].name) == 0) {
        return &(_spNameMapOpal[i]);
      }
    }
  } else {
    for (i = 0; i < _SPNAME_INFO_COUNT_ENTERPRISE; i++) {
      if (strcmp(spName, _spNameMapEnterprise[i].name) == 0) {
        return &(_spNameMapEnterprise[i]);
      }
    }
  }
  return NULL;
}

uid spNameToUID(char *spName) {
  _spNameInfoStruct *spNIS = _findSPName(spName);
  if (spNIS == NULL) {
    return NULL;
  }
  return (uid)spNIS->UID;
}

typedef struct { char *name; char *UID; } _tableNameInfoStruct;

static _tableNameInfoStruct _tableNameMap[] = {
    { "DataStore", "\x00\x00\x80\x01\x00\x00\x00\x00" },
    { "DataStore1", "\x00\x00\x10\x01\x00\x00\x00\x00" },
    { "DataStore2", "\x00\x00\x10\x02\x00\x00\x00\x00" },
    { "DataStore3", "\x00\x00\x10\x03\x00\x00\x00\x00" },
    { "DataStore4", "\x00\x00\x10\x04\x00\x00\x00\x00" },
    { "DataStore5", "\x00\x00\x10\x05\x00\x00\x00\x00" },
    { "DataStore6", "\x00\x00\x10\x06\x00\x00\x00\x00" },
    { "DataStore7", "\x00\x00\x10\x07\x00\x00\x00\x00" },
    { "DataStore8", "\x00\x00\x10\x08\x00\x00\x00\x00" },
    { "DataStore9", "\x00\x00\x10\x09\x00\x00\x00\x00" },
    { "DataStore10", "\x00\x00\x10\x0A\x00\x00\x00\x00" },
    { "DataStore11", "\x00\x00\x10\x0B\x00\x00\x00\x00" },
    { "DataStore12", "\x00\x00\x10\x0C\x00\x00\x00\x00" },
    { "DataStore13", "\x00\x00\x10\x0D\x00\x00\x00\x00" },
    { "DataStore14", "\x00\x00\x10\x0E\x00\x00\x00\x00" },
    { "DataStore15", "\x00\x00\x10\x0F\x00\x00\x00\x00" },
    { "DataStore16", "\x00\x00\x10\x10\x00\x00\x00\x00" },
    { "MBR", "\x00\x00\x08\x04\x00\x00\x00\x00" },
  };

#define _TABLE_NAME_INFO_COUNT (sizeof(_tableNameMap)/sizeof(_tableNameInfoStruct))

static _tableNameInfoStruct *_findTableName(char *tableName) {
  unsigned int i;
  for (i = 0; i < _TABLE_NAME_INFO_COUNT; i++) {
    if (strcmp(tableName, _tableNameMap[i].name) == 0) {
      return &(_tableNameMap[i]);
    }
  }
  return NULL;
}

uid tableNameToUID(char *tableName) {
  _tableNameInfoStruct *tNIS = _findTableName(tableName);
  if (tNIS == NULL) {
    return NULL;
  }
  return (uid)tNIS->UID;
}
