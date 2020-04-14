#ifndef KS_MANDIAGS_H
#define KS_MANDIAGS_H
#ifdef __cplusplus
extern "C" {
#endif
//-----------------------------------------------------------------------------
//
// Header: ks_mandiags.h
// Date: 2014/03/18
// Author: Chris N Allo
//
// Description: Types and definitions for ks_mandiags function.
//
//-----------------------------------------------------------------------------
// **
// * Do NOT modify or remove this copyright and confidentiality notice.
 // *
 // * Copyright 2014 Seagate Technology LLC.
 // *
 // * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 // * and may be covered under one or more Non-Disclosure Agreements. All or
 // * portions are also trade secret. Any use, modification, duplication,
 // * derivation, distribution or disclosure of this code, for any reason,
 // * not expressly authorized is prohibited. All other rights are expressly
 // * reserved by Seagate Technology LLC.
 // *
//


/**
 * @file ks_mandiags.h
 *
 * Implementation Notes:
 * This function operates in 4 modes : 
 *                                            
 * * KS_GET_STATUS: 
 *            function will determine status of port and return either     
 *                   PORT_LOCKED (33) or PORT_UNLOCKED (55)
 * 
 * * KS_CHALLENGE: 
 * * KS_PAM_CHALLENGE:
 *             Function will open a session to the LockingSP.  It will retrieve
 *             the SID pin data from the Datastore area and decrypt the 
 *             SID PIN then issue an authenticate to the SID PIN. if all is a 
 *             success to this point the function will issue an authenticate to 
 *             MakerSymK to get a challenge string. this module will present the 
 *             returned challenge string to the user with a KS_SUCCESS unless an     
 *             error occurs. This function also returns a "session info" string
 *             which will need to be entered on the next function call to
 *             the ks_mandiags for the KS_RESPONSE task.  KS_PAM_CHALLENGE is the
 *             same as KS_CHALLENGE except it does not printf to the screen to
 *             Show the CHALLENGE string. (quiet mode)
 *                                             
 * * KS_RESPONSE: 
 *             Function will issue an authenticate to MakerSymK but use the 
 *             user input "response" string and the session info string. 
 *             Upon success it will formulate a command string to unlock/lock the Diag
 *             port. if all is a success to this point the function will
 *             return a PORT_UNLOCKED or PORT_LOCKED code to the user else an error. 
 *  
 * * KS_CHALLENGE_AND_RESPONSE: 
 *             Function will open a session to the LockingSP.  It will retrieve
 *             the SID pin data from the Datastore area and decrypt the 
 *             SID PIN then issue an authenticate to the SID PIN. If all is a 
 *             success to this point the function will issue an authenticate to 
 *             MakerSymK to get a challenge string. this module will present the 
 *             returned challenge string to the user.  It will then wait for the 
 *             user to use the TDCI tool and enter the response string returned.
 *             Upon success it will formulate a command string to unlock/lock the Diag
 *             port. if all is a success to this point the function will
 *             return a PORT_UNLOCKED or PORT_LOCKED code to the user else an error.
 *
 * * NOTE:
 *             Challenge - Response steps are skipped if the port is already in
 *             the state that the user is asking for.
 *              
 * * @param[in] Device Identifier
 * * @param[in] Task   (what to do 0 = get status  1 = challenge  2 = response )
 * * @param[in] Lock  (whether to lock or unlock the port [0 = unlock, 1 = lock])
 * * @param[out] challenge string for KS_CHALLENGE or KS_CHALLENGE_AND_RESPONSE tasks
 * * @param[in]  response string to KS_RESPONSE task
 * * @param[out] Session Info string from KS_CHALLENGE task
 * * @param[in] Session Info string to KS_RESPONSE task
 * * @return PORT_LOCKED, PORT_UNLOCKED or SUCCESS if all goes well, otherwise the first error encountered.
  * 
 */

/* Return codes used for Port status */
/* these are special so they dont conflict with other processing error codes */
#define PORT_UNLOCKED	55
#define PORT_LOCKED	33
#define PORT_AUTHORIZED 44

#define UNLOCK_PORT	0
#define LOCK_PORT	1
#define AUTHORIZE_PORT  2


/* Allowable tasks for Manage Diagnostics calls */
typedef enum {
  KS_GET_STATUS = 0,
  KS_CHALLENGE,
  KS_RESPONSE,
  KS_CHALLENGE_AND_RESPONSE,
  KS_PAM_CHALLENGE
} tasks;

extern int ks_mandiags(char *Device_ID, tasks Task, uint64_t lock, char *Chall_Resp, char *Session_Info);

#ifdef __cplusplus
}
#endif
#endif
