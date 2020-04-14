/*-----------------------------------------------------------------------------
*
*     Header: seagate.c
*     Date: 2014/04/30
*     Author: Chris N Allo
*
*     Description: PAM module for port access
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
 * @file seagate.c
 *
 *          
 * No parameters in or out
 * This module is called by the PAM authenticator and will require the user to interact with the TDCITOOL.
 *  This code is liinked to ks_mandiags.c to generate a challenge with MakerSymK and require the user to 
 *  respond with the response string from TDCITOOL.  It will then verify authentication and then either allow the
 *  user login or reject the login.
 * Optionally there is code which is compiled out right now that would unlock or lock a port through PAM.  It was 
 *  determined after the code was designed and written that the Kinetic group would not lock/unlock this way.
 *  Code is still there just in case the minds of decision change....
 *
 *  NOTES:
 *    This code will be built and generate a seagate.so module which needs to be placed in /lib/security
 *
 *
 */

/* standard c includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* PAM includes */
#include <security/pam_appl.h>
#include <security/pam_modules.h>

/* Local functions */
#include "ks_globals.h"
#include "ks_mandiags.h"
#include "ks_utilities.h"
#include "ks_debug.h"

/* From EAPI functions */
#include "low_level.h"
#include "transport.h"
#include "transport_locator.h"
#include "high_level.h"
#include "tcg_constants.h"
#include "debug.h"

/* From PBKDF functions */
#include "types.h"
#include "sc_pbkdf.h"
#include "aes_gcm.h"
#include "sc_aes.h"
#include "sc_aescrypt.h"
#include "sc_hmac.h"
#include "sc_sha2.h"

/* Unused hook */
PAM_EXTERN int pam_sm_setcred( pam_handle_t *pamh, int flags, int argc, const char **argv ) {
      return PAM_IGNORE ;
}


/* this function is ripped from pam_unix/support.c, it lets us do IO via PAM */
int converse( pam_handle_t *pamh, int nargs, struct pam_message **message, struct pam_response **response ) {
        int retval ;
        struct pam_conv *conv ;

        retval = pam_get_item( pamh, PAM_CONV, (const void **) &conv ) ;

        if( retval==PAM_SUCCESS ) {
          /* This actually sends the challenge to the terminal and waits for user to input response */
          retval = conv->conv( nargs, 
                               (const struct pam_message **) message, 
                               response, 
                               conv->appdata_ptr ) ;
        }

        return retval ;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv) {
        /* Unused */
       return PAM_SUCCESS;
}

/* This is where we get called from PAM Authenticate routine                    */
/* we need to call ks_mandiags function from here to process challenge response */
/* auth    required seagate.so DEVICE=/dev/sdX */
/* account sufficient seagate.so DEVICE=/dev/sdX */
/* where X is the drive offset  ie a, b, c, etc */
PAM_EXTERN int pam_sm_authenticate( pam_handle_t *pamh, int flags,int argc, const char **argv ) {
        int retval ;
        int i ;

        /* Input will hold response from converse() */
        char 	*CResponse ;
        uint64_t lock_request;
        struct 	pam_message msg[1],*pmsg[1];
        struct 	pam_response *resp;
        char 	Challenge_Response[65];  /* make larger enough to hold at least 32x2 bytes */
        char 	Session_Info[32];
        char    Device_Id[100];
        char    Temp_Str[160];
        char    lock_str[32];

        /* retrieving parameters */
        int got_deviceid = 0 ;

        /* get Device parameter need to look through all argv's in case user enters comments*/
        for( i=0 ; i<argc ; i++ ) {
          if( strncmp(argv[i], "DEVICE=", 7)==0 ) {
             strncpy( Device_Id, argv[i]+7, 93 ) ;  /* we can only hold 100 chars */
             got_deviceid = 1 ;
          }
        }
        if( got_deviceid==0 ) {
          sprintf(Temp_Str, "\n DEVICE ID ERROR \n");
          pmsg[0] = &msg[0] ;
          msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
          msg[0].msg = Temp_Str;
          resp = NULL ;
          if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
            return retval ;
          }
          return PAM_AUTH_ERR ;
        }

        strcpy(lock_str, "To Authorize Port");
#ifdef Original_Code

        /* we need to determine if user is wanting to lock or unlock the diag port */
        
        /* set up a Prompt for User */
        sprintf(Temp_Str, "Enter 1 to Lock or 0 to UNlock Port ==> ");

        /* setting up conversation call prompting for Lock direction */
        pmsg[0] = &msg[0] ;
        msg[0].msg_style = PAM_PROMPT_ECHO_ON ;  /* This will wait for user to respond */
        msg[0].msg = Temp_Str;
        resp = NULL ;
        if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
          /* if this function fails, make sure that ChallengeResponseAuthentication in sshd_config is set to yes */
          return retval ;
        }

        /* At this point we should have either a "0" or "1" or a NULL that the user entered */
        if( resp ) {
          if( (flags & PAM_DISALLOW_NULL_AUTHTOK) && resp[0].resp == NULL ) {
            free( resp );  /* user did not enter anything  */
            return PAM_AUTH_ERR;
          }
            CResponse = resp[ 0 ].resp;
            resp[ 0 ].resp = NULL;
        }
        else {
          return PAM_CONV_ERR;
        }


        if (strcmp(CResponse, "0") == 0) {
          /* user wants to unlock port */
          lock_request = UNLOCK_PORT;
          strcpy(lock_str, "To UNLOCK Port");
        }
        else {
          if (strcmp(CResponse, "1") == 0) {
            /* user wants to lock port */
            lock_request = LOCK_PORT;
            strcpy(lock_str, "To LOCK Port");
          }
          else {
            free (resp);
            return PAM_AUTH_ERR;
          }
        }
        /* we are done with this value */
        free( CResponse ) ;
#endif
        /* Just look for Unlock for this version of code CA 6/24/14 */
        lock_request = UNLOCK_PORT;

        /* Lets call ks_mandiags to get the challenge string */
        retval = ks_mandiags(Device_Id, KS_PAM_CHALLENGE, lock_request, Challenge_Response, Session_Info);

        if ((retval != PORT_LOCKED) && ( retval != PORT_UNLOCKED) ) 
         {
           /* something failed getting the information about the drive */
           /* at this point we need to return a failure */
           sprintf(Temp_Str, "\n PORT Access Error %d\n",retval);
           pmsg[0] = &msg[0] ;
           msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
           msg[0].msg = Temp_Str;
           resp = NULL ;
           if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
             return retval ;
           }
           return PAM_AUTH_ERR;
         }
        /* KS_MANDIAGS will return port_status in retval as a result of this call */
        /* if the port_status is the same as what is requested we can end now and return */
        /* otherwise we will need to continue the response side of the challenge */
        if ((retval == PORT_UNLOCKED) && (lock_request == UNLOCK_PORT)) {
          /* Port is already in the UNLOCK state we are are asking for.  We can return success */
          sprintf(Temp_Str, "\nPort is Already UN-Locked \n");
          pmsg[0] = &msg[0] ;
          msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
          msg[0].msg = Temp_Str;
          resp = NULL ;
          if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
            return retval ;
          }
          return PAM_SUCCESS ;
        }
#ifdef Original_Code    /* only now looking for the locked port */
        else
          if ((retval == PORT_LOCKED) && (lock_request == LOCK_PORT)) 
          {
            /* Port is already in the LOCK state we are are asking for.  We can return auth failure to deny access */       
            sprintf(Temp_Str, "\nPort is Already Locked \n");
            /* setting up conversation call prompting for Lock direction */
            pmsg[0] = &msg[0] ;
            msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
            msg[0].msg = Temp_Str;
            resp = NULL ;
            if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
              return retval ;
            }
            free ( resp );
            return PAM_AUTH_ERR;
          }
#endif
         
        /* set up a Prompt for User */
        sprintf(Temp_Str, "Challenge = '%s' \n %s Enter Response ==> ",Challenge_Response,lock_str);

        /* setting up conversation call prompting for challenge response */
        pmsg[0] = &msg[0] ;
        msg[0].msg_style = PAM_PROMPT_ECHO_ON ;  /* This will wait for user to respond */
        msg[0].msg = Temp_Str;
        resp = NULL ;
        if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
          /* if this function fails, make sure that ChallengeResponseAuthentication in sshd_config is set to yes */
          return retval ;
        }

        /* At this point we should have resp = the response that the user entered */
        if( resp ) {
          if( (flags & PAM_DISALLOW_NULL_AUTHTOK) && resp[0].resp == NULL ) {
            free( resp );
            return PAM_AUTH_ERR;
          }
            CResponse = resp[ 0 ].resp;
            resp[ 0 ].resp = NULL;
        }
        else {
          return PAM_CONV_ERR;
        }
 
        /* need to force this resp into a real string format */
        memcpy(Challenge_Response, CResponse, 64);
        Challenge_Response[64] = '\0';  /* null terminate */

        /* Just look for AUTHORIZATION for this version of code CA 6/24/14 */
        lock_request = AUTHORIZE_PORT;

        /* Verify Response entered by user */
        /* we will call ks_mandiags with response task and see if it is correct   */
        retval = ks_mandiags(Device_Id, KS_RESPONSE, lock_request, Challenge_Response, Session_Info);

    /* the following is special code for authorization only */
        if (retval == PORT_AUTHORIZED) {
          /* Successful C-R port is authorized per user request */
          sprintf(Temp_Str, "\nPORT is Now AUTHORIZED \n");
          /* setting up conversation call prompting for Lock direction */
          pmsg[0] = &msg[0] ;
          msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
          msg[0].msg = Temp_Str;
          resp = NULL ;
          if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
            /* if this function fails, make sure that ChallengeResponseAuthentication in sshd_config is set to yes */
            return retval ;
          }
          free( CResponse ) ;
          return PAM_SUCCESS ;
        }
#ifdef Original_Code
        if ((retval == PORT_UNLOCKED) && (lock_request == UNLOCK_PORT)) {
          /* Successful C-R port is unlocked per user request */
          sprintf(Temp_Str, "\nPORT is Now UN-LOCKED \n");
          /* setting up conversation call prompting for Lock direction */
          pmsg[0] = &msg[0] ;
          msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
          msg[0].msg = Temp_Str;
          resp = NULL ;
          if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
            /* if this function fails, make sure that ChallengeResponseAuthentication in sshd_config is set to yes */
            return retval ;
          }
          free( CResponse ) ;
          return PAM_SUCCESS ;
        }

        else {  /* so if the user successfully locked the port we dont want to allow access anymore */
          if ((retval == PORT_LOCKED) && (lock_request == LOCK_PORT)) {
            /* Successful C-R port is unlocked per user request */
            sprintf(Temp_Str, "\nPORT is Now LOCKED \n");
            pmsg[0] = &msg[0] ;
            msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
            msg[0].msg = Temp_Str;
            resp = NULL ;
            if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
              return retval ;
            }
            free( resp );
            free( CResponse ) ;
            return PAM_AUTH_ERR ;
          } /* end if port_locked */
#endif
          else {
            /* C-R failed User Login will fail*/
            sprintf(Temp_Str, "\nPORT Status Change attempt FAILED %d\n",retval);
            pmsg[0] = &msg[0] ;
            msg[0].msg_style = PAM_TEXT_INFO ;  /* Display a message */
            msg[0].msg = Temp_Str;
            resp = NULL ;
            if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
              return retval ;
            }
            free( resp );
            free( CResponse ) ;
            return PAM_AUTH_ERR;
          }
#ifdef Original_Code
        }
#endif

        /* we shouldn't get to this point, but if we do, we might as well return something bad */
        return PAM_AUTH_ERR ;
}


