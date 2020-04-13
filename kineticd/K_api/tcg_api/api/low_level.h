#ifndef LOW_LEVEL_H
#define LOW_LEVEL_H

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
 * @file low_level.h
 *
 * Functions to start and stop sessions, do TCG method calls, etc.
 * These functions are used by high_level.h functions and can also be called
 * directly.
 * 
 */

#include "level_0_discovery.h"
#include "transport.h"
#include "parameters.h"
#include "utilities.h"


#define CHECK_TRANSPORT { \
  if (!transport_set) { \
    return API_ERROR("setTransport not called"); \
  } \
  if (ourTransport->comID == 0) { \
    return API_ERROR("transport has no ComID assigned"); \
  } \
}

extern int transport_set;
extern transport *ourTransport;

/** 
 * If this is not called first, the other functions will return an error code.
 *
 * @param[in] t (required) pointer to transport structure to use.
 * @return SUCCESS if transport is accepted. Null will cause an ERROR.
 */
extern status setTransport(transport *t);



/** 
 * This function performs the STACK_RESET command specified in the TCG
 * SWG Enterprise SSC specification and the TCG Core Specification. 
 * This command is typically used when the last TCG session ended unexpectedly.
 *
 * @param[in] comId (optional) The COMID to send the STACK_RESET to (If NULL, the
 *                             comID set in the transport structure is used).
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueStackReset(int *comId);


/** 
 * This function performs the TPER_RESET command specified in the TCG
 * SWG Opal SSC v2.00 specification. This command is typically used to perform
 * a programmatic reset. For Opal SSC v2.00 only.
 *
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueTperReset(void);


/** 
 * This function issues a Level 0 Discovery IF-RECV with Protocol 01 to
 * the device. Note that this function does not have to be called within
 * a TCG session.
 *
 * @param[out] result (required) The results of Level 0 Discovery
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueLevel0Discovery(discoveryData result);



/**
 * This function issues a Properties method call on Session 0 (Session Manager).
 *
 * @param[out] results (required) the results from the Properties method call.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueProperties(parameters *results);



/** 
 * This function begins a TCG session by invoking the StartSession method
 * to the specified SP. Note that nested TCG sessions are not supported.
 *
 * @param[in] sp (required) The specified SP (Security Provider)
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueStartSession(char *sp);

#ifdef SECURE_MESSAGING
/** 
 * This function begins a TCG session by invoking the StartTLSSession method
 * to the specified SP. Note that nested TCG sessions are not supported.
 *
 * @param[in] sp (required) The specified SP (Security Provider)
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueStartTLSSession(char *sp);
#endif

/**
 * This function invokes the Authenticate method on the specified
 * Authority with the specified PIN. This function must be called within a TCG
 * session.
 *
 * @param[in] authority (required) The authority to authenticate
 * @param[in] pin (required) The PIN of the authority
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueAuthenticate(char *authority, tcgByteValue *pin);



/**
 * This function invokes the Authenticate method on the specified Authority using
 * a two-step challenge/reponse pattern.
 * An authority linked to a challenge/response credential needs to do authentication
 * in two steps. The first step is to request a challenge from the TPer, the second
 * step is to use the challenge to create a response and send the response back to
 * the TPer. As such challenge and response parameters are used independently.
 * The cannot both be NULL and they cannot both be non-NULL.
 * The host first calls issueAuthenticateCR with challenge non-NULL in order to store
 * the challenge from the TPer. Then, once the response is computed, the host
 * calls issueAuthenticateCR again, this time passing in a non-NULL response.
 * If the host instead attempts to authenticate to another authority after the first step,
 * the authentication will fail as the TPer is expecting the response to be sent.
 *
 * @param[in] authority (required) The authority to authenticate
 * @param[out] challenge (optional) Where to store the TPer challenge from the TPer.
 * @param[in] response (optional) The response to be sent back to the TPer.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueAuthenticateCR(char *authority, tcgByteValue *challenge, tcgByteValue *response);



/**
 * This function invokes the Get method on the specified object.
 * Note that as per TCG, only values readable to the authenticated authorities (if any)
 * will be returned. This function must be called within a TCG session.
 *
 * @param[in] objectUid (required) The UID of the object to get
 * @param[in] getParameters (optional) To be passed as parameters of the Get method call.
 * @param[out] results (required) To be filled in with the results of the Get method call
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueGet(uid objectUid, parameters *getParameters, parameters *results);



/**
 * This function invokes the Set method on the specified column(s)
 * in the specified object using the specified value(s). This function
 * can set multiple adjacent columns. This function must be called within a TCG
 * session.
 * NOTE: The order in which columns are added to the valuesToSet parameters
 *       structure is important. This function will derive the TCG starting
 *       and ending columns from the valuesToSet parameters structure,
 *       and it will pass parameters to the TCG method in the
 *       order that they were added to the valuesToSet parameters structure.
 *
 * @param[in] objectUid (required) The UID of the object to set
 * @param[in] valuesToSet (required) The values to set
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueSet(uid objectUid,
                       parameters *valuesToSet);

/**
 * This function invokes the Activate method on the Locking SP.
 * This function must be called within a TCG Admin SP session.
 * The SID authority must be authenticated for this method to be successful.
 * For Opal SSC or Opal SSC v2.00 only.
 *
 * @param[in] valuesToActivate (required) The parameters for the Activate 
 * method.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueActivate(parameters *valuesToActivate);

/**
 * This function invokes the Reactivate method on the current SP.
 * This function must be called within a TCG Locking SP session.
 * The Admin1 authority must be authenticated for this method to be successful.
 * For Opal SSC or Opal SSC v2.00 Single User mode only.
 *
 * @param[in] valuesToReactivate (required) The parameters for the Reactivate 
 * method.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueReactivate(parameters *valuesToReactivate);

/**
 * This function invokes the GenKey method on the specified band.
 * The GenKey method is only valid for Opal SSC or Opal SSC v2.00 drives.
 * This function must be called within a TCG session.
 *
 * @param[in] bandNumber (required) The band number (0-15) to GenKey
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueGenKey(int bandNumber);


/**
 * This function invokes the Erase method on the specified band.
 * This function must be called within a TCG session.
 * For Enterprise SSC or for Opal SSC or Opal SSC v2.00 Single User Mode only.
 *
 * @param[in] bandNumber (required) The band number (0-15) to Erase
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueErase(int bandNumber);

/**
 * This function invokes the RevertSP method Admin SP. This
 * function must be called within a TCG session.
 * @param[in] valuesToRevertSP (required) The parameters for the RevertSP 
 * method.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueRevertSP(parameters *valuesToRevertSP);

/**
 * This function invokes the Random method with the specified number of
 * bytes to return. This function must be called within a TCG session.
 *
 * @param[in] byteCount (required) The size of the random number (1-32), in bytes
 * @param[out] result (required) Where to store the resulting random number (must be large enough to hold byteCount bytes)
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueRandom(int byteCount, char *result);


/**
 * This function attempts to start a Transaction.
 * This function must be called within a TCG session.
 *
 * @return SUCCESS if a transaction is started, otherwise the first error encountered.
 */
extern status issueStartTransaction(void);


/**
 * This function attempts to Abort the current transaction.
 * This function must be called within a TCG session.
 * If this function is not called while a transaction is active,
 * the TPer will return an error. This interface does not
 * keep track if a transaction is active or not.
 *
 * @return SUCCESS if a transaction is Aborted, otherwise the first error encountered.
 */
extern status issueAbortTransaction(void);


/**
 * This function attempts to Commit the current transaction.
 * This function must be called within a TCG session.
 * If this function is not called while a transaction is active,
 * the TPer will return an error. This interface does not
 * keep track if a transaction is active or not.
 *
 * @return SUCCESS if a transaction is Committed, otherwise the first error encountered.
 */
extern status issueCommitTransaction(void);


/**
 * This function invokes the CloseSession method on the current
 * session. This function must be called within a TCG Session and will
 * result in that session ending.
 * @return SUCCESS if all goes well, otherwise the first error encountered.
 */
extern status issueCloseSession(void);


/* Unpublished on purpose!
 *
 * saveState returns a pointer to a static buffer, null terminated,
 * of printable data that captures the current state of the low_level
 * module. Such things as session ids, and anything else that may be
 * necessary for loadState to restore the module to the current state.
 * All that is guaranteed is that the result is a null-terminated
 * C string that can be printed (no spaces, line breaks, funny characters, etc.)
 */
extern char *saveState(void);


/* Unpublished on purpose!
 *
 * loadState takes a string as was formatted by saveState and restores
 * the encoded state.
 */

extern void loadState(char *state_from_saveState);

#endif
