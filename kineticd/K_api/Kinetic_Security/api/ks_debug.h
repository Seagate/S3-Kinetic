#ifndef KS_DEBUG_H
#define KS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_debug.h
// Date: 2014/03/14
// Author: Chris N Allo
//
// Description: Types and definitions for ks_debug function.
//
//-----------------------------------------------------------------------------


// **
// Do NOT modify or remove this copyright and confidentiality notice!
//
// Copyright (c) 2001-2014 Seagate Technology, LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology, LLC.
// Portions are also trade secret. Any use, duplication, derivation, distribution
// or disclosure of this code, for any reason, not expressly authorized is
// prohibited. All other rights are expressly reserved by Seagate Technology, LLC.
//
// **


/**
 * @file ks_debug.h
 *
 * Definitions and globals for ks_debug function.
 *
 * Output debug information to console and log file depending on debug level.
 * If debug level is > 0 data is output through printf or method coded in this call
 * 
 * Next step will be the action type specified by user
 * This function will output information only if the debug level > 0 and the status != 0
 * If action type is KS_INFO this function will only print Info string even if Ret_Status != 0
 *
 * Implementation Notes:
 *
 * @param[in] Enterprise API status returned.
 * @param[in] Action type to perform
 * @param[in] Informational String  (info or lastTcgStatusDescription() )
 * @return SUCCESS if all goes well, otherwise nothing
 *
 */

/* From EAPI functions */
#include "status_codes.h"
#include "low_level.h"

/* standards for the debug information output */
#define DEBUG_OFF	0
#define DEBUG_EAPI	100
#define DEBUG_KSAPI	2
#define DEBUG_CRYPTO    50

/* Actions to perform for error found */
typedef enum {
  KS_INFO = 0x80,
  KS_RETURN,
  KS_CLOSE,
  KS_ABORT,
  KS_REVERT,
  KS_APIRETURN,
  KS_GCMRETURN,
  KS_DEBABORT,
  KS_EAPIABORT,
  KS_GCMABORT
} ks_action_type;

/* extern void ks_debug(status Ret_Status, ks_action_type Action, char * Info_String); */

#define KS_DEBUG(Ret_Status,Action,Info_String) {\
\
   status temp_status;\
   temp_status = Ret_Status;\
  /* Check Status to see if we are handling an error or just a message for output */\
  /* if Ret_Status != 0 we will print Info_String, lasttcgdescription and do what the Action asks */\
  if ( temp_status != 0) {\
    /* EAPI function failed.  print message and last TCG data */\
    if (Debug_Info > 0) {\
      printf ("Error returned from function call to EAPI Function = %d action = %d  debug_level = %d\n",Ret_Status,Action,Debug_Info);\
      printf ("   USER INFO :'%s'\n",Info_String);\
      printf ("   TCG INFO  :'%s'\n",lastTcgStatusDescription());\
    } /* end if */\
  }\
  else {\
    /* Ret_Status was ok.  Just print informational message */\
    if (Debug_Info > 0) {\
      printf ("   USER INFO :'%s'\n",Info_String);\
    } /* end if */\
  }  /* end else */\
\
  switch (Action) {\
   case KS_INFO: \
     {\
      /* dont take any action */\
     /* printf (" KS INFO :\n");*/\
      break;\
     }\
   case KS_RETURN:\
     {\
       /* Cause calling function to return if func fails*/ \
       if ( temp_status != 0) {printf (" KS RETURN :\n");return(KS_RETURN);}\
      break;\
     }\
   case KS_APIRETURN: /* ?? */\
     {\
       /* Cause calling function to return if func fails*/ \
       if ( temp_status != 0) {printf (" KS API RETURN :  API error = %d\n",temp_status);return(KS_APIRETURN);}\
      break;\
     }\
   case KS_CLOSE: /* close session and return */\
     {\
      if ( temp_status != 0) { printf (" KS CLOSE :\n");issueCloseSession();return(temp_status/*KS_CLOSE*/);}\
      break;\
     }\
   case KS_ABORT:  /* abort transaction, close session and return */\
     {\
      if ( temp_status != 0) { printf (" KS ABORT : %d\n",temp_status);issueAbortTransaction();issueCloseSession();return(KS_ABORT);}\
      break; \
     }\
   case KS_EAPIABORT: /* Enterprise API abort */\
     {\
       /* Cause calling function to return if func fails*/ \
       if ( temp_status != 0) {printf (" KS EAPI ABORT : EAPI error (%d) = %s\n",temp_status,lastTcgStatusDescription());issueAbortTransaction();issueCloseSession();return(KS_EAPIABORT);}\
      break;\
     }\
   case KS_REVERT:  /* abort transaction, close session and return */\
     {\
/* remove after testing */\
       printf ("revert drive \n");\
\
      /*issueRevert();   not done yet */\
      break;\
     }\
   default:  /* do nothing */\
     {\
      break;\
     }\
   } /* end switch */\
} /* end KS_DEBUG */ 

#ifdef __cplusplus
}
#endif
#endif
