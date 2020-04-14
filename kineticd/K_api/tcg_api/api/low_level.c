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
   Need to add 2 new TLS calls for secure messaging
   1) StartTLS
   2) SyncTLS
*/

/**
 * @file low_level.c
 *
 * Functions to start and stop sessions, do TCG method calls, etc.
 * These functions are used by high_level.h functions and can also be called
 * directly.
 *
 */

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>  /* For nanosleep() */
#include "debug.h"
#include "low_level.h"
#include "tcg_constants.h"
#include "transport.h"


/** How many times to retry on pending responses before timing out */
#define RECV_RETRY_COUNT                10

/** How many milliseconds to wait between retry on pending responses */
#define RECV_RETRY_DELAY_MILLIS               50


/* This is the old C90 code:
#define DELAY_MILLIS(amt) usleep((amt)*1000)
*/

/* C99 version: */
/* 1 millisecond = 1,000,000 Nanoseconds */
#define NANO_SECOND_MULTIPLIER  1000000
static struct timespec _time_to_sleep = {0, RECV_RETRY_DELAY_MILLIS * NANO_SECOND_MULTIPLIER};

#define DELAY_A_WHILE nanosleep(&_time_to_sleep, NULL)


/** Used to tell packet decoder if you are expecting a packet
    for the session manager or for a regular session.
*/
#define PACKET_SESSION_MANAGER  1
#define PACKET_SESSION          0

int transport_set = 0;
transport *ourTransport;

/** When a session is open, these variables will hold the sessions numbers: */
static uint64_t currentTPerSessionID = 0;
static uint64_t currentHostSessionID = 0;

/** The next HostSessionID to assign.
 *  Host Session IDs are completely arbitrary.
 */
static uint64_t nextHostSessionID = 100;


#define SAVE_STATE_SIZE  (    4  +  /* various single character markers */ \
                          (16*3) +  /* three 64 bit hex numbers (max size of 16 hex pairs each) */ \
                              1     /* Trailing NULL */ \
                         )

static char saveStateBuffer[SAVE_STATE_SIZE];

char *saveState(void) {
    sprintf(saveStateBuffer, "T%jxH%jxN%jx+",
            (uintmax_t)currentTPerSessionID,
            (uintmax_t)currentHostSessionID,
            (uintmax_t)nextHostSessionID);
    return saveStateBuffer;
}

void loadState(char *state_from_saveState) {
    uintmax_t savedTPerSessionID = 0, savedHostSessionID = 0, savedNextHostSessionID = 0;

    if (state_from_saveState == NULL) {
        printf("...NULL\n");
        return;
    }
    sscanf(state_from_saveState, "T%jxH%jxN%jx+",
          &savedTPerSessionID,
          &savedHostSessionID,
          &savedNextHostSessionID);

    currentTPerSessionID = (uint64_t)savedTPerSessionID;
    currentHostSessionID = (uint64_t)savedHostSessionID;
    nextHostSessionID = (uint64_t)savedNextHostSessionID;
}


void clearSessionIDs(void) {
  currentTPerSessionID = 0;
  currentHostSessionID = 0;
}

#define SHORT_COMID(longComID) (((longComID) >> 16) & 0xFFFF)

#define CHECK_TRANSPORT_SET { \
  if (!transport_set) { \
    return API_ERROR("setTransport not called"); \
  } \
}

#define CHECK_SESSION_OPEN { \
  if ((currentTPerSessionID == 0) && \
      (currentHostSessionID == 0)) { \
    return API_ERROR("No session open"); \
  } \
}

#define CHECK_NO_SESSION_OPEN { \
  if ((currentTPerSessionID != 0) || \
      (currentHostSessionID != 0)) { \
    return API_ERROR("Session already open"); \
  } \
}

#define SET_ID_BYTE_VALUE(a, b, c) { \
  status s_i_b_v = setIdByteValue((a),(b),(c)); \
  if (s_i_b_v != SUCCESS) { \
    return s_i_b_v; \
  } \
}

#define SET_NAMED_BYTE_VALUE(a, b, c) { \
  status s_n_b_v = setNamedByteValue((a),(b),(c)); \
  if (s_n_b_v != SUCCESS) { \
    return s_n_b_v; \
  } \
}

#define SET_BYTE_VALUE(a, b) { \
  status s_b_v = setByteValue((a),(b)); \
  if (s_b_v != SUCCESS) { \
    return s_b_v; \
  } \
}

#define READ_BYTE(expr) { \
  if (ourTransport->recvBufferPos >= ourTransport->recvBufferTail) { \
    return API_ERROR("Attempt to read past valid data"); \
  } \
  expr = ((*(ourTransport->recvBufferPos++)) & 0xFF); \
}

#define WRITE_BYTE(b) { \
  unsigned char w_b_v = ((b) & 0xFF); \
  if (ourTransport->sendBufferTail >= &(ourTransport->sendBuffer[PAYLOAD_BUFFER_SIZE])) { \
    return API_ERROR("Attempt to send too many bytes"); \
  } \
  *(ourTransport->sendBufferTail++) = w_b_v; \
}

#define WRITE_INT(value) { \
  unsigned int w_i_v = value; \
  WRITE_BYTE(w_i_v >> 8); \
  WRITE_BYTE(w_i_v); \
}

#define WRITE_LONG(value) { \
  uint64_t w_l_v = value; \
  WRITE_BYTE(w_l_v >> 24); \
  WRITE_BYTE(w_l_v >> 16); \
  WRITE_BYTE(w_l_v >> 8); \
  WRITE_BYTE(w_l_v); \
}

#define WRITE_TINY_INT_TOKEN(value) { \
  unsigned char w_t_i_t_v = ((value) & 0xFF); \
  if (w_t_i_t_v > 63) { \
    return INTERNAL_ERROR("WRITE_TINY_INT_TOKEN call with value that is too big"); \
  } \
  WRITE_BYTE(w_t_i_t_v); \
}

#define READ_TINY_INT_TOKEN(header, into) {                             \
  uint64_t r_t_i_t = header;                                         \
  if (r_t_i_t > 63) {                                                     \
    return INTERNAL_ERROR("READ_TINY_INT_TOKEN call with value that is too big"); \
  }                                                                     \
  into = r_t_i_t;                                                         \
}

#define WRITE_SHORT_BYTE_TOKEN_HEADER(length) { \
  unsigned char w_s_b_t_h_len = (length); \
  if (w_s_b_t_h_len > 15) { \
    return INTERNAL_ERROR("WRITE_SHORT_BYTE_TOKEN called with value that is too big"); \
  } \
  WRITE_BYTE(0xA0 | w_s_b_t_h_len); \
}

#define WRITE_MEDIUM_BYTE_TOKEN_HEADER(length) { \
  unsigned int w_m_b_t_h_len = (length); \
  if (w_m_b_t_h_len > 2047) { \
    return INTERNAL_ERROR("WRITE_MEDIUM_BYTE_TOKEN_HEADER call with value that is too big"); \
  } \
  WRITE_BYTE(0xD0 | (w_m_b_t_h_len >> 8)); \
  WRITE_BYTE(w_m_b_t_h_len & 0xFF); \
}

#define VERIFY_SHORT_BYTE_TOKEN_HEADER(length) { \
  unsigned char v_s_b_t_h_byte; \
  unsigned char v_s_b_t_h_expectedLen = (length); \
  if (v_s_b_t_h_expectedLen > 15) { \
    return INTERNAL_ERROR("VERIFY_SHORT_BYTE_TOKEN called with value that is too big"); \
  } \
  READ_BYTE(v_s_b_t_h_byte); \
  if (v_s_b_t_h_byte != (0xA0 | v_s_b_t_h_expectedLen)) { \
    return ERROR_("VERIFY_SHORT_BYTE_TOKEN: unexpected token when looking for short byte token"); \
  } \
}

#define WRITE_SHORT_INT_TOKEN_HEADER(length) { \
  unsigned char w_s_i_t_h_len = (length); \
  if (w_s_i_t_h_len > 15) { \
    return INTERNAL_ERROR("WRITE_SHORT_INT_TOKEN called with value that is too big"); \
  } \
  WRITE_BYTE(0x80 | w_s_i_t_h_len); \
}

#define READ_TOKEN_HEADER(into) { \
  READ_BYTE(into); \
}

#define ENCODE_INTEGER(value) { \
  uint64_t e_i_v = value; \
  if (e_i_v < 64) { \
    WRITE_TINY_INT_TOKEN(e_i_v); \
  } else if (e_i_v < INT_MAX) { \
    WRITE_SHORT_INT_TOKEN_HEADER(4); \
    WRITE_BYTE((e_i_v>>24) & 0xFF);  \
    WRITE_BYTE((e_i_v>>16) & 0xFF);  \
    WRITE_BYTE((e_i_v>> 8) & 0xFF);  \
    WRITE_BYTE((e_i_v    ) & 0xFF);  \
  } else {                           \
    WRITE_SHORT_INT_TOKEN_HEADER(8); \
    WRITE_BYTE((e_i_v>>56) & 0xFF);  \
    WRITE_BYTE((e_i_v>>48) & 0xFF);  \
    WRITE_BYTE((e_i_v>>40) & 0xFF);  \
    WRITE_BYTE((e_i_v>>32) & 0xFF);  \
    WRITE_BYTE((e_i_v>>24) & 0xFF);  \
    WRITE_BYTE((e_i_v>>16) & 0xFF);  \
    WRITE_BYTE((e_i_v>> 8) & 0xFF);  \
    WRITE_BYTE((e_i_v    ) & 0xFF);  \
  } \
}

#define DECODE_INTEGER(into) {                          \
  uint64_t d_i_temp;                           \
  status s = _decodeInteger(&d_i_temp);                 \
  if (s != SUCCESS) {                                   \
    return s;                                           \
  }                                                     \
  into = d_i_temp;                                      \
}

static char _decodeHeaderErrorMessage[100];

#define DECODE_HEADER(header, tokenType, tokenLength) {         \
  if ((header & 0x80) == 0) {                                   \
    tokenType = INT_TYPE;                                       \
    tokenLength = 0;                                            \
  } else if ((header & 0xC0) == 0x80) {                         \
    tokenType = ((header & 0x20) == 0) ? INT_TYPE : BYTE_TYPE;  \
    tokenLength = header & 0XF;                                 \
  } else if ((header & 0xE0) == 0xC0) {                         \
    unsigned char d_h_temp;                                     \
    tokenType = ((header & 0x10) == 0) ? INT_TYPE : BYTE_TYPE;  \
    tokenLength = header & 0x7;                                \
    READ_BYTE(d_h_temp);                                        \
    tokenLength = (tokenLength << 8) | d_h_temp;                \
  } else if ((header & 0xFC) == 0xE0) {                         \
    unsigned char d_h_temp;                                     \
    tokenType = ((header & 0x02) == 0) ? INT_TYPE : BYTE_TYPE;  \
    tokenLength = 0;                                            \
    READ_BYTE(d_h_temp);                                        \
    tokenLength = (tokenLength << 8) | d_h_temp;                \
    READ_BYTE(d_h_temp);                                        \
    tokenLength = (tokenLength << 8) | d_h_temp;                \
    READ_BYTE(d_h_temp);                                        \
    tokenLength = (tokenLength << 8) | d_h_temp;                \
  } else {                                                      \
    sprintf(_decodeHeaderErrorMessage, "DECODE_HEADER: Unable to decode token header: %d/0x%x", header, header);      \
    return ERROR_(_decodeHeaderErrorMessage);                   \
  }                                                             \
}


#define ENCODE_BYTES(value) { \
  unsigned int e_b_len = (value).len; \
  unsigned int e_b_index; \
  unsigned char *e_b_bytes = (value).data; \
  if (e_b_len < 16) { \
    WRITE_SHORT_BYTE_TOKEN_HEADER(e_b_len); \
  } else { \
    WRITE_MEDIUM_BYTE_TOKEN_HEADER(e_b_len); \
  } \
  for (e_b_index = 0; e_b_index < e_b_len; e_b_index++) { \
    WRITE_BYTE(e_b_bytes[e_b_index]); \
  } \
}

#define ENCODE_UID(value) { \
  int e_u_i; \
  unsigned char * e_u_uid = (value);            \
  WRITE_SHORT_BYTE_TOKEN_HEADER(8); \
  for (e_u_i=0; e_u_i < 8; e_u_i++) { \
    WRITE_BYTE(e_u_uid[e_u_i]); \
  } \
}

#define DECODE_UID(expectedValue) { \
  unsigned char d_u_value[8]; \
  int d_u_i; \
  VERIFY_SHORT_BYTE_TOKEN_HEADER(8); \
  for (d_u_i = 0; d_u_i < 8; d_u_i++) { \
    READ_BYTE(d_u_value[d_u_i]); \
  } \
  if (memcmp(expectedValue, d_u_value, 8)) { \
    return ERROR_("DECODE_UID: Unexpected UID received"); \
  } \
}

/* As per Table 08 of the Enterprise SSC Document */
#define PAYLOAD_KEEP_TRYING() ((decodeComPacketLength == 0) && (decodeOutstandingData == 1) && (decodeMinTransfer == 0))
#define PAYLOAD_NO_DATA()     ((decodeComPacketLength == 0) && (decodeOutstandingData == 0) && (decodeMinTransfer == 0))
#define PAYLOAD_BUFFER_TOO_SMALL() ((decodeComPacketLength == 0) && (decodeOutstandingData > 1))


#define RETURN_METHOD_STATUS {                                  \
  uint64_t r_m_s_status;                                   \
  uint64_t r_m_s_zero;                                     \
  DECODE_SIMPLE_TOKEN(START_LIST_TOKEN);                        \
  DECODE_INTEGER(r_m_s_status);                                 \
  DECODE_INTEGER(r_m_s_zero);                                   \
  if (r_m_s_zero != 0) {                                        \
    return ERROR_("RETURN_METHOD_STATUS: Method call status list formatting failure"); \
  }                                                             \
  DECODE_INTEGER(r_m_s_zero);                                   \
  if (r_m_s_zero != 0) {                                        \
    return ERROR_("RETURN_METHOD_STATUS: Method call status list formatting failure"); \
  }                                                             \
  DECODE_SIMPLE_TOKEN(END_LIST_TOKEN);                          \
  return METHOD_ERROR(r_m_s_status, "RETURN_METHOD_STATUS: Unexpected Method Status");\
}

#define SEND_PAYLOAD(protocolId, shortComId) { \
  status s_p_status = ourTransport->sendFunction(protocolId, shortComId);  \
  if (s_p_status != SUCCESS) { return s_p_status; } \
}

#define RECV_PAYLOAD(protocolId, shortComId) { \
  status r_p_s; \
  resetRecvPayload(); \
  r_p_s = ourTransport->recvFunction(protocolId, shortComId); \
  if (r_p_s != SUCCESS) { return r_p_s; } \
}

#define READ_LONG(expr) { \
  unsigned char r_l_b4, r_l_b3, r_l_b2, r_l_b1; \
  READ_BYTE(r_l_b4); \
  READ_BYTE(r_l_b3); \
  READ_BYTE(r_l_b2); \
  READ_BYTE(r_l_b1); \
  expr = (((r_l_b4 << 24) | (r_l_b3 << 16) | (r_l_b2 << 8) | r_l_b1) & 0xFFFFFFFF); \
}

#define READ_INT(expr) { \
  unsigned char r_i_b2, r_i_b1; \
  READ_BYTE(r_i_b2); \
  READ_BYTE(r_i_b1); \
  expr = (((r_i_b2 << 8) | r_i_b1) & 0xFFFF); \
}


void resetSendPayload(void) {
  ourTransport->sendBufferTail = ourTransport->sendBuffer;
  ourTransport->comPacketLengthPtr = NULL;
  ourTransport->packetLengthPtr = NULL;
  ourTransport->subPacketLengthPtr = NULL;
}

void resetRecvPayload(void) {
  ourTransport->recvBufferTail = ourTransport->recvBuffer;
  ourTransport->recvBufferPos = ourTransport->recvBuffer;
  memset(ourTransport->recvBuffer , 0x00, sizeof(ourTransport->recvBuffer));
  ourTransport->comPacketLengthPtr = NULL;
  ourTransport->packetLengthPtr = NULL;
  ourTransport->subPacketLengthPtr = NULL;
}

static uint64_t decodeComId;
static uint64_t decodeOutstandingData;
static uint64_t decodeMinTransfer;
static uint64_t decodeComPacketLength;
/**/
static uint64_t decodeTPerSessionID;
static uint64_t decodeHostSessionID;
static uint64_t decodeTPerSequenceNumber;
static uint64_t decodeAckNackStatus;
static uint64_t decodeAckNackID;
static uint64_t decodePacketLength;
/**/
static uint64_t decodeReservedLong;
static unsigned int decodeReservedInt;
static unsigned int decodeSubPacketKind;
static uint64_t decodeSubPacketLength;


/** Format of a COMPACKET_HEADER as defined in section 3.2.3.2 of the TCG Core Specification document */
#define ENCODE_COMPACKET_HEADER(comId) { \
  WRITE_LONG(0);  /* Reserved value */ \
  WRITE_LONG(comId); \
  WRITE_LONG(0);   /* Host does not set Outstanding Data */ \
  WRITE_LONG(0);   /* Host does not set MinTransfer */ \
  ourTransport->comPacketLengthPtr = ourTransport->sendBufferTail; \
  WRITE_LONG(0);   /* Placeholder for ComPacket length */ \
  /* Payload starts here */ \
}

#define DECODE_COMPACKET_HEADER(comId) {                                \
  READ_LONG(decodeReservedLong);                                        \
  READ_LONG(decodeComId);                                               \
  READ_LONG(decodeOutstandingData);                                     \
  READ_LONG(decodeMinTransfer);                                         \
  READ_LONG(decodeComPacketLength);                                     \
  ourTransport->comPacketLengthPtr = ourTransport->recvBufferPos;       \
  if ((comId) != decodeComId) {                                         \
    return ERROR_("DECODE_COMPACKET_HEADER: Bad comID on received payload"); \
  }                                                                     \
}

static uint64_t sequenceNumber = 0x01;

#define RESET_SEQUENCE_NUMBER { \
  sequenceNumber = 0x01; \
  }

#define ENCODE_PACKET_HEADER(TPerSessionID, HostSessionID) { \
  WRITE_LONG(TPerSessionID); \
  WRITE_LONG(HostSessionID); \
  WRITE_LONG(sequenceNumber); \
  sequenceNumber++; \
  WRITE_INT(0);   /* Reserved */ \
  WRITE_INT(0);   /* No ACK, no NACK */ \
  WRITE_LONG(0);  /* No ACK/NACK ID */ \
  ourTransport->packetLengthPtr = ourTransport->sendBufferTail; \
  WRITE_LONG(0);  /* Placeholder for packet length */ \
}

#define DECODE_PACKET_HEADER { \
  READ_LONG(decodeTPerSessionID); \
  READ_LONG(decodeHostSessionID); \
  READ_LONG(decodeTPerSequenceNumber); \
  READ_INT(decodeReservedInt); \
  READ_INT(decodeAckNackStatus); \
  READ_LONG(decodeAckNackID); \
  READ_LONG(decodePacketLength); \
  ourTransport->packetLengthPtr = ourTransport->recvBufferPos; \
}


#define ENCODE_SUBPACKET_HEADER { \
  WRITE_LONG(0);  /* Reserved fields */ \
  WRITE_INT(0);   /* Reserved fields */ \
  WRITE_INT(DATA_SUBPACKET_KIND); \
  ourTransport->subPacketLengthPtr = ourTransport->sendBufferTail; \
  WRITE_LONG(0);  /* Placeholder for subPacket length */ \
}

#define DECODE_SUBPACKET_HEADER { \
  READ_LONG(decodeReservedLong); \
  READ_INT(decodeReservedInt); \
  READ_INT(decodeSubPacketKind); \
  READ_LONG(decodeSubPacketLength); \
  ourTransport->subPacketLengthPtr = ourTransport->recvBufferPos; \
}


#define ENCODE_TO_SESSION {\
  ENCODE_COMPACKET_HEADER(ourTransport->comID); \
  ENCODE_PACKET_HEADER(currentTPerSessionID, currentHostSessionID); \
  ENCODE_SUBPACKET_HEADER; \
}

#define ENCODE_TO_SESSION_MANAGER {\
  ENCODE_COMPACKET_HEADER(ourTransport->comID); \
  ENCODE_PACKET_HEADER(0, 0); \
  ENCODE_SUBPACKET_HEADER; \
}


#define ENCODE_END_OF_SESSION_TOKEN { \
  WRITE_BYTE(END_OF_SESSION_TOKEN);   \
}

#define DECODE_END_OF_SESSION_TOKEN {           \
  DECODE_SIMPLE_TOKEN(END_OF_SESSION_TOKEN); \
}

#define ENCODE_END_OF_DATA_TOKEN { \
  WRITE_BYTE(END_OF_DATA_TOKEN); \
}

#define ENCODE_METHOD_CALL(objectUID, methodUID) { \
  WRITE_BYTE(CALL_TOKEN); \
  ENCODE_UID(objectUID); \
  ENCODE_UID(methodUID); \
}

#define ENCODE_START_NAME { \
  WRITE_BYTE(START_NAME_TOKEN); \
}

#define ENCODE_END_NAME { \
  WRITE_BYTE(END_NAME_TOKEN); \
}

#define ENCODE_START_LIST { \
  WRITE_BYTE(START_LIST_TOKEN); \
}

#define ENCODE_START_TRANSACTION { \
  WRITE_BYTE(START_TRANSACTION_TOKEN); \
}

#define DECODE_START_TRANSACTION { \
  DECODE_SIMPLE_TOKEN(START_TRANSACTION_TOKEN); \
}

#define ENCODE_END_TRANSACTION { \
  WRITE_BYTE(END_TRANSACTION_TOKEN); \
}

#define DECODE_END_TRANSACTION { \
  DECODE_SIMPLE_TOKEN(END_TRANSACTION_TOKEN); \
}



#define GET_START_LIST(parameterPtr) {                  \
  status g_s_l = getStartList(parameterPtr);            \
  if (g_s_l != SUCCESS) {                               \
    return g_s_l;                                       \
  }                                                     \
}

#define GET_END_LIST(parameterPtr) {                  \
  status g_e_l = getEndList(parameterPtr);            \
  if (g_e_l != SUCCESS) {                             \
    return g_e_l;                                     \
  }                                                   \
}

#define SET_INT_VALUE(parameterPtr, value) {                        \
  status s_i_v = setIntValue(parameterPtr, value);                  \
  if (s_i_v != SUCCESS) {                                           \
    return s_i_v;                                                   \
  }                                                                 \
}

#define GET_INT_VALUE(parameterPtr, value) {                            \
  uint64_t g_i_v_value;                                            \
  status g_i_v_status = getIntValue(parameterPtr, &g_i_v_value);        \
  if (g_i_v_status != SUCCESS) {                                        \
    return g_i_v_status;                                                \
  }                                                                     \
  value = g_i_v_value;                                                  \
}

#define SET_BYTE_VALUE_C_STR(parameterPtr, cString) {                   \
  status s_b_v_c_s_status;                                              \
  tcgByteValue *s_b_v_c_s_tmpByteValue = tcgTmpName(cString);           \
  if (s_b_v_c_s_tmpByteValue == NULL) {                                 \
    return ERROR_("SET_BYTE_VALUE_C_STR: Cannot convert C String to TCG Byte Value");          \
  }                                                                     \
  s_b_v_c_s_status = setByteValue(parameterPtr, s_b_v_c_s_tmpByteValue); \
  if (s_b_v_c_s_status != SUCCESS) {                                    \
    return s_b_v_c_s_status;                                            \
  }                                                                     \
}


#define ENCODE_START_PARAMETER_LIST ENCODE_START_LIST

#define ENCODE_END_LIST { \
  WRITE_BYTE(END_LIST_TOKEN); \
}

#define ENCODE_END_PARAMETER_LIST ENCODE_END_LIST

status _encodeParameters(parameters *p) {
  unsigned int index;
  if (p == NULL) {
    return API_ERROR("_encodeParameters: NULL parameters pointer");
  }
  for(index = 0 ; index < p->slotsUsed; index++) {
    switch (p->parameterList[index].parameterType) {
    case INT_TYPE:
      ENCODE_INTEGER(p->parameterList[index].value.longValue);
      break;
    case BYTE_TYPE:
      ENCODE_BYTES(p->parameterList[index].value.byteValue);
      break;
    case START_LIST_TYPE:
      ENCODE_START_LIST;
      break;
    case END_LIST_TYPE:
      ENCODE_END_LIST;
      break;
    case START_NAME_TYPE:
      ENCODE_START_NAME;
      break;
    case END_NAME_TYPE:
      ENCODE_END_NAME;
      break;
    default:
      return ERROR_("_encodeParameters: Unknown parameters type, cannot encode");
    }
  }
  return SUCCESS;
}

#define ENCODE_PARAMETERS(parametersPtr) { \
  status e_p_s = _encodeParameters(parametersPtr); \
  if (e_p_s != SUCCESS) { return e_p_s; } \
}

#define ENCODE_METHOD_STATUS_SUCCESS { \
  WRITE_BYTE(START_LIST_TOKEN); \
  ENCODE_INTEGER(METHOD_STATUS_SUCCESS); \
  ENCODE_INTEGER(0); \
  ENCODE_INTEGER(0); \
  WRITE_BYTE(END_LIST_TOKEN); \
}

/* Length fields do not count their own length.
 * Since the pointers are saved at the front of the length
 * field, the length field's length must be subtracted.
 */
#define FIX_PAYLOAD_HEADERS {                                           \
  unsigned char *f_p_h_subPacketEndPtr = ourTransport->sendBufferTail;       \
  uint64_t f_p_h_rawLength = f_p_h_subPacketEndPtr - ourTransport->sendBuffer;        \
  unsigned int f_p_h_subPacketPadding = ((4 - (f_p_h_rawLength % 4)) % 4); \
  unsigned char *f_p_h_sendEndPtr;                                         \
  while (f_p_h_subPacketPadding-- > 0) {WRITE_BYTE(0x00); }; \
  f_p_h_sendEndPtr = ourTransport->sendBufferTail;                         \
  ourTransport->sendBufferTail = ourTransport->subPacketLengthPtr;      \
  WRITE_LONG((f_p_h_subPacketEndPtr - ourTransport->subPacketLengthPtr) - 4); \
  ourTransport->sendBufferTail = ourTransport->packetLengthPtr;         \
  WRITE_LONG((f_p_h_sendEndPtr - ourTransport->packetLengthPtr) - 4);   \
  ourTransport->sendBufferTail = ourTransport->comPacketLengthPtr;      \
  WRITE_LONG((f_p_h_sendEndPtr - ourTransport->comPacketLengthPtr) - 4); \
  ourTransport->sendBufferTail = f_p_h_sendEndPtr;                      \
}

#define DECODE_PAYLOAD_HEADERS {                        \
  DECODE_COMPACKET_HEADER(ourTransport->comID);         \
  DECODE_PACKET_HEADER;                                 \
  DECODE_SUBPACKET_HEADER;                              \
}

#define VERIFY_PAYLOAD_HEADERS(for_session_manager) {   \
  DECODE_PAYLOAD_HEADERS;                               \
  CHECK_DECODE_HEADERS(for_session_manager);            \
}

status _check_decode_headers(int for_session_manager) {
  unsigned char *comPacketTail = ourTransport->comPacketLengthPtr + decodeComPacketLength;
  unsigned char *packetTail = ourTransport->packetLengthPtr + decodePacketLength;
  unsigned char *subPacketTail = ourTransport->subPacketLengthPtr + decodeSubPacketLength;

  if (comPacketTail > ourTransport->recvBufferTail) {
    return ERROR_("_check_decode_headers: ComPacketLength longer than data received");
  }
  /* if we get data past the end of the compacket, which
     some transports might return due to buffer issues,
     we'll ignore it */
  ourTransport->recvBufferTail = comPacketTail;
  if (packetTail > comPacketTail) {
    return ERROR_("_check_decode_headers: PacketLength longer than ComPacketLength");
  }
  if (packetTail < comPacketTail) {
    return ERROR_("_check_decode_headers: PacketLength shorter than ComPacketLength, only one Packet per ComPacket supported");
  }
  if (subPacketTail > packetTail) {
    return ERROR_("_check_decode_headers: SubPacketLength longer than PacketLength");
  }
  if (subPacketTail < packetTail) {
    unsigned int magicPaddingLen = ((4 - (decodeSubPacketLength %4)) % 4);
    /*
    printf("Decode headers: SubPacketTail = %p\n", subPacketTail);
    printf("Decode headers: magicPaddingLen = 0x%0x\n", magicPaddingLen);
    printf("Decode headers: packetTail = %p\n", packetTail);
    */
    if ((subPacketTail + magicPaddingLen) != packetTail) {
        return ERROR_("_check_decode_headers: SubPacketLength shorter than PacketLength, only one SubPacket per Packet supported");
    }
  }

  if (for_session_manager) {
    if ((decodeTPerSessionID != 0) || (decodeHostSessionID != 0)) {
      return ERROR_("_check_decode_headers: Session Manager packet expected, found session packet instead");
    }
  } else {
    if (decodeTPerSessionID != currentTPerSessionID) {
      printf("TPerSessionID: decoded = 0x%jx, expected = 0x%jx\n", (uintmax_t)decodeTPerSessionID, (uintmax_t)currentTPerSessionID);
      return ERROR_("_check_decode_headers: TPerSessionID mismatch");
    }
    if (decodeHostSessionID != currentHostSessionID) {
      return ERROR_("_check_decode_headers: HostSessionID mismatch");
    }
  }

  return SUCCESS;
}

#define CHECK_DECODE_HEADERS(for_session_manager) {           \
  status _c_d_l = _check_decode_headers(for_session_manager); \
  if (_c_d_l != SUCCESS) { return _c_d_l; }                   \
}

#define RECV_PAYLOAD_WITH_RETRY(protocolID, comID, for_session_manager) { \
    unsigned int r_p_w_r_recv_count = RECV_RETRY_COUNT + 1;             \
    while (1) {                                                         \
      RECV_PAYLOAD(protocolID, comID);                                  \
      DECODE_PAYLOAD_HEADERS;                                           \
      r_p_w_r_recv_count--;                                             \
      if (PAYLOAD_NO_DATA()) {                                          \
        return ERROR_("RECV_PAYLOAD_WITH_RETRY: No Data returned!");     \
      }                                                                 \
      if (PAYLOAD_BUFFER_TOO_SMALL()) {                                 \
        return ERROR_("RECV_PAYLOAD_WITH_RETRY: Response too big!");     \
      }                                                                 \
      if (PAYLOAD_KEEP_TRYING()) {                                      \
        if (r_p_w_r_recv_count == 0) {                                  \
          return ERROR_("RECV_PAYLOAD_WITH_RETRY: too many retries");    \
        } else {                                                        \
          DELAY_A_WHILE;                                                \
        }                                                               \
      } else {                                                          \
        break;                                                          \
      }                                                                 \
    }                                                                   \
    CHECK_DECODE_HEADERS(for_session_manager);                          \
  }

static char _d_s_t_error_msg_buffer[100];

#define DECODE_SIMPLE_TOKEN(expectedToken) { \
  unsigned char d_s_t_token; \
  READ_BYTE(d_s_t_token); \
  if (d_s_t_token != expectedToken) { \
    sprintf(_d_s_t_error_msg_buffer, "DECODE_SIMPLE_TOKEN: got unexpected token: 0x%x when expecting 0x%x", d_s_t_token, expectedToken); \
    return ERROR_(_d_s_t_error_msg_buffer);                              \
  } \
}

#define DECODE_START_PARAMETER_LIST DECODE_SIMPLE_TOKEN(START_LIST_TOKEN)

int _peek_is_token(unsigned char token) {
  unsigned char actual_token;
  if (ourTransport->recvBufferPos >= ourTransport->recvBufferTail) {
    return 0;
  }
  actual_token = *(ourTransport->recvBufferPos);
  return (actual_token == token);
}

#define PEEK_START_LIST_TOKEN() _peek_is_token(START_LIST_TOKEN)
#define PEEK_END_LIST_TOKEN() _peek_is_token(END_LIST_TOKEN)

#define PEEK_END_PARAMETER_LIST() PEEK_END_LIST_TOKEN()

#define PEEK_START_NAME_TOKEN() _peek_is_token(START_NAME_TOKEN)
#define PEEK_END_NAME_TOKEN() _peek_is_token(END_NAME_TOKEN)

#define DECODE_END_PARAMETER_LIST DECODE_SIMPLE_TOKEN(END_LIST_TOKEN)

#define DECODE_END_OF_DATA_TOKEN DECODE_SIMPLE_TOKEN(END_OF_DATA_TOKEN)

#define DECODE_METHOD_CALL(expectedMethodCallUID) { \
  DECODE_SIMPLE_TOKEN(CALL_TOKEN); \
  DECODE_UID(SMUID); \
  DECODE_UID(expectedMethodCallUID); \
}

status _decodeInteger(uint64_t *result) {
  unsigned char header;
  unsigned char tokenType;
  uint64_t tokenLength;
  uint64_t _dI = 0;
  READ_TOKEN_HEADER(header);
  DECODE_HEADER(header, tokenType, tokenLength);
  if (tokenType != INT_TYPE) {
    return ERROR_("_decodeInteger: found non-Integer token");
  }
  if (tokenLength == 0) { /* Special case for TINY ATOMs */
    _dI = header;
  } else {
    for (;tokenLength > 0; tokenLength--) {
      unsigned char byteValue;
      READ_BYTE(byteValue);
      _dI = (_dI << 8) | byteValue;
    }
  }
  *result = _dI;
  return SUCCESS;
}

status _decodeParameter(parameters *into) {
  status s;
 //CHECK_DEBUG;
 // DEBUG(("Decode parameter enter\n"));
  if (PEEK_START_NAME_TOKEN()) {
    DECODE_SIMPLE_TOKEN(START_NAME_TOKEN);
   // DEBUG(("...Start Name\n"));
    s = setStartName(into);
    if (s != SUCCESS) {return s;}
    s = _decodeParameter(into);
    if (s != SUCCESS) {return s;}
    s = _decodeParameter(into);
    if (s != SUCCESS) {return s;}
    DECODE_SIMPLE_TOKEN(END_NAME_TOKEN);
   // DEBUG(("...End Name\n"));
    s = setEndName(into);
    if (s != SUCCESS) {return s;}
  } else if (PEEK_START_LIST_TOKEN()) {
    DECODE_SIMPLE_TOKEN(START_LIST_TOKEN);
   // DEBUG(("...Start List\n"));
    s = setStartList(into);
    if (s != SUCCESS) {return s;}
    while (!PEEK_END_LIST_TOKEN()) {
      s = _decodeParameter(into);
      if (s != SUCCESS) { return s; }
    }
    DECODE_SIMPLE_TOKEN(END_LIST_TOKEN);
   // DEBUG(("...End List\n"));
    s = setEndList(into);
    if (s != SUCCESS) {return s;}
  } else {
    unsigned char header;
    unsigned char tokenType;
    uint64_t tokenLength;
    READ_TOKEN_HEADER(header);
    DECODE_HEADER(header, tokenType, tokenLength);
    if (tokenType == INT_TYPE) {
      uint64_t tokenValue = 0;
     // DEBUG(("...Int Token\n"));
      if (tokenLength == 0) { /* Special case for TINY ATOMs */
        tokenValue = header;
      } else {
        for (;tokenLength > 0; tokenLength--) {
          unsigned char byteValue;
          READ_BYTE(byteValue);
          tokenValue = (tokenValue << 8) | byteValue;
        }
      }
      s = setIntValue(into, tokenValue);
      if (s != SUCCESS) { return s; }
    } else if (tokenType == BYTE_TYPE) {
      tcgByteValue tokenValue;
      uint64_t count;
     // DEBUG(("...Byte Token\n"));
      tokenValue.len = tokenLength;
      for(count = 0; count < tokenLength; count++) {
        READ_BYTE(tokenValue.data[count]);
      }
      s = setByteValue(into, &tokenValue);
      if (s != SUCCESS) { return s; }
    } else {
   //   DEBUG(("...Unknown token: %d\n", tokenType));
      return ERROR_("_decodeParameter: unexpected Token. Expected Integer or Byte encoding");
    }
  }
 // DEBUG(("Decode parameter exit success\n"));
  return SUCCESS;
}

#define DECODE_PARAMETER(decodeResults) { \
  status d_p_s = _decodeParameter(decodeResults); \
  if (d_p_s != SUCCESS) {return(d_p_s); }\
}

static char _methodStatusError[100];

#define DECODE_METHOD_STATUS_SUCCESS {                                  \
  uint64_t d_m_s_s_methodStatus;                                   \
  uint64_t d_m_s_s_zero;                                           \
  DECODE_SIMPLE_TOKEN(START_LIST_TOKEN);                                \
  DECODE_INTEGER(d_m_s_s_methodStatus);                                 \
  DECODE_INTEGER(d_m_s_s_zero);                                         \
  if (d_m_s_s_zero != 0) {                                              \
    sprintf(_methodStatusError,"DECODE_METHOD_STATUS_SUCCESS: Failed to get Method Status required 0 (middle): %jd", (uintmax_t)d_m_s_s_zero);    \
    return ERROR_(_methodStatusError);    \
  }                                                                     \
  DECODE_INTEGER(d_m_s_s_zero);                                         \
  if (d_m_s_s_zero != 0) {                                              \
    sprintf(_methodStatusError,"DECODE_METHOD_STATUS_SUCCESS: Failed to get Method Status required 0 (end): %jd", (uintmax_t)d_m_s_s_zero);       \
    return ERROR_(_methodStatusError);       \
  }                                                                     \
  DECODE_SIMPLE_TOKEN(END_LIST_TOKEN);                                  \
  if (d_m_s_s_methodStatus != 0) {                                      \
    sprintf(_methodStatusError, "Failed to get Method Status of SUCCESS: %jd/0x%jx", (uintmax_t)d_m_s_s_methodStatus, (uintmax_t)d_m_s_s_methodStatus); \
    return METHOD_ERROR(d_m_s_s_methodStatus, _methodStatusError); \
  }                                                                     \
}


status setTransport(transport *t) {
  CHECK_DEBUG;
  DEBUG(("setTransport: %p\n", (void *)t));
  if (t == NULL) {
    return API_ERROR("setTransport: NULL transport pointer");
  }
  ourTransport = t;
  transport_set = 1;
  return SUCCESS;
}


status issueStackReset(int *comId) {
  /* Formatting and values as per TCG Core Spec,
   * Section 3.3.4.7.5 STACK_RESET, Table 34 STACK_RESET Command Request
   * and the TCG Enterprise SSC Spec, section 4.5.2 Protocol Stack Reset Commands
   */
  /* Recv Buffer Decoding: */
  uint64_t extendedComIdToReset = 0;
  unsigned int comIdToReset = 0;
  uint64_t recvComID = 0;
  uint64_t resetStatus = 0;
  unsigned int availableDataLength = 0;
  unsigned int reserved;
  uint64_t recvRequestCode = 0;

  CHECK_DEBUG;
  CHECK_TRANSPORT;
  DEBUG(("issueStackReset: %p\n", (void *)comId));
  if (comId == NULL) {
    extendedComIdToReset = ourTransport->comID;
    comIdToReset = extendedComIdToReset >> 16;
  } else {
    /* Enterprise SSC uses 0-padding to create extended ComIDs */
    extendedComIdToReset = (((uint64_t)*comId) << 16);
    comIdToReset = *comId;
  }
  DEBUG(("issueStackReset using comId: %jx\n", (uintmax_t)extendedComIdToReset));
  resetSendPayload();
  WRITE_LONG(extendedComIdToReset);
  WRITE_LONG(STACK_RESET_REQUEST_CODE);
  SEND_PAYLOAD(STACK_RESET_PROTOCOL_ID, comIdToReset);
  DEBUG(("issueStackReset - payload sent\n"));
  /* Enterprise SSC (see section numbers above) says that if
   * availableDataLength is 0, the reset is still in progress.
   */
  while (availableDataLength == 0) {
    RECV_PAYLOAD(STACK_RESET_PROTOCOL_ID, comIdToReset);
    READ_LONG(recvComID);
    DEBUG(("issueStackReset - got response from TPer, comId: %jx\n", (uintmax_t)recvComID));
    if (recvComID != extendedComIdToReset) {
      return ERROR_("issueStackReset: Bad response ComID");
    }
    READ_LONG(recvRequestCode);
    DEBUG(("issueStackReset - receive request code: %ju\n", (uintmax_t)recvRequestCode));
    if (recvRequestCode == STACK_RESET_NO_RESPONSE_AVAILABLE) {
      return ERROR_("issueStackReset: No Response Available");
    } else if (recvRequestCode != STACK_RESET_REQUEST_CODE) {
      return ERROR_("issueStackReset: Bad response request code");
    }
    READ_INT(reserved);
    (void)reserved; /* Tell compiler we intend to not use this variable */
    DEBUG(("reserved = 0x%x\n", reserved));
    READ_INT(availableDataLength);
    DEBUG(("issueStackReset - receive availableDataLength: %u\n", availableDataLength));
    if (availableDataLength == 4) {
      READ_LONG(resetStatus);
      DEBUG(("issueStackReset - received status: 0x%jx\n", (uintmax_t)resetStatus));
      break;
    } else if (availableDataLength != 0) {
      return ERROR_("issueStackReset: Unexpectd data length returned");
    }
  }
  if (resetStatus == STACK_RESET_STATUS_SUCCESS) {
    clearSessionIDs();
    return SUCCESS;
  } else if (resetStatus == STACK_RESET_STATUS_FAILURE) {
    return ERROR_("issueStackReset: TPer return status 'FAILURE'");
  }
  return ERROR_("issueStackReset: TPer returned unknown status value");
}


status issueTperReset(void) {
  /* Formatting and values as per TCG Opal SSC v2.00,
   * Section 3.2.3 TPER_RESET
   */
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  DEBUG(("issueTperReset\n"));
  resetSendPayload();
  WRITE_BYTE(0x00); /* dummy */
  SEND_PAYLOAD(TPER_RESET_PROTOCOL_ID, TPER_RESET_COMID);
  DEBUG(("issueTperReset - payload sent\n"));
  return SUCCESS;
}


status issueLevel0Discovery(discoveryData result) {
  unsigned int dataSize;
  byteSource copyResult;
  CHECK_DEBUG;
  /* Make sure that we have a transport, but
   * don't check for a ComID assignment yet,
   * until we finish discovery, we won't know
   * what ComID to use.
   */
  DEBUG(("Check Transport\n"));
  CHECK_TRANSPORT_SET;
  DEBUG(("issueLevel0Discovery called.\n"));
  RECV_PAYLOAD(LEVEL_0_DISCOVERY_SECURITY_PROTOCOL_ID,
               LEVEL_0_DISCOVERY_SECURITY_PROTOCOL_COMID);
  dataSize = ourTransport->recvBufferTail - ourTransport->recvBuffer;
  dataSize = (dataSize < LEVEL_0_DISCOVERY_DATA_MAX_SIZE) ? dataSize : LEVEL_0_DISCOVERY_DATA_MAX_SIZE;
  copyResult = copyBytes(ourTransport->recvBuffer, dataSize, result);
  if (copyResult == NULL) {
    return ERROR_("issueLevel0Discovery: Unable to copy Level 0 Discovery Data");
  }
  DEBUG(("issueLevel0Discovery Returning Success.\n"));
  return SUCCESS;
}


status issueProperties(parameters *results) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;

  DEBUG(("Formulating Properties Method\n"));
  DEBUG(("ComID is: %jx\n", (uintmax_t)ourTransport->comID));
  resetSendPayload();
  ENCODE_TO_SESSION_MANAGER;
  ENCODE_METHOD_CALL(SMUID,PROPERTIES_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Properties Method\n"));

  resetParameters(results);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION_MANAGER);
  DECODE_METHOD_CALL(PROPERTIES_METHOD_UID);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(results);
  }
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;

  return SUCCESS;
}

static parameters driveResponse;
static parameters startSessionParameters;

status issueStartSession(char *sp) {
  uint64_t syncSessionHostID = 0;
  uint64_t syncSessionTPerID = 0;
  uint64_t newHostSessionID = 0;
  uid spUID = NULL;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_NO_SESSION_OPEN;
  if (sp == NULL) {
    return API_ERROR("issueStartSession: NULL sp name");
  }
  spUID = spNameToUID(sp);
  if (spUID == NULL) {
    return API_ERROR("issueStartSession: unknown sp name");
  }
  RESET_SEQUENCE_NUMBER;
  resetSendPayload();
  ENCODE_TO_SESSION_MANAGER;
  ENCODE_METHOD_CALL(SMUID, START_SESSION_METHOD_UID);
  newHostSessionID = nextHostSessionID++;
  resetParameters(&startSessionParameters);
  SET_INT_VALUE(&startSessionParameters, newHostSessionID);
  SET_BYTE_VALUE(&startSessionParameters, tcgTmpNameCount((char *)spUID, 8));
  SET_INT_VALUE(&startSessionParameters, 1);  /* Always open a Write Session */
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(&startSessionParameters);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent StartSession Method\n"));

  resetParameters(&driveResponse);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION_MANAGER);
  DECODE_METHOD_CALL(SYNC_SESSION_METHOD_UID);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&driveResponse);
  }
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DEBUG(("End Of Data Token received.\n"));
  DECODE_METHOD_STATUS_SUCCESS;
  DEBUG(("Method call status was a success.\n"));

  GET_INT_VALUE(&driveResponse, syncSessionHostID);
  GET_INT_VALUE(&driveResponse, syncSessionTPerID);

  if (syncSessionHostID != newHostSessionID) {
    printf("SyncSessionHostID: %jd, expected: %jd\n", (uintmax_t)syncSessionHostID, (uintmax_t)newHostSessionID);
    return ERROR_("SyncSession: wrong Host Session ID");
  }

  currentTPerSessionID = syncSessionTPerID;
  currentHostSessionID = newHostSessionID;
  DEBUG(("Started Session! HostID: 0x%jx, TPerID: 0x%jx\n", (uintmax_t)currentHostSessionID, (uintmax_t)currentTPerSessionID));
  return SUCCESS;
}

#ifdef SECURE_MESSAGING
status issueStartTLSSession(char *sp) {
  uint64_t syncSessionHostID = 0;
  uint64_t syncSessionTPerID = 0;
  uint64_t newHostSessionID = 0;
  uid spUID = NULL;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_NO_SESSION_OPEN;
  if (sp == NULL) {
    return API_ERROR("issueStartTLSSession: NULL sp name");
  }
  spUID = spNameToUID(sp);
  if (spUID == NULL) {
    return API_ERROR("issueStartTLSSession: unknown sp name");
  }
  RESET_SEQUENCE_NUMBER;
  resetSendPayload();
  ENCODE_TO_SESSION_MANAGER;
  ENCODE_METHOD_CALL(SMUID, START_TLS_SESSION_METHOD_UID);
  newHostSessionID = nextHostSessionID++;
  resetParameters(&startSessionParameters);
  SET_INT_VALUE(&startSessionParameters, newHostSessionID);
  SET_BYTE_VALUE(&startSessionParameters, tcgTmpNameCount((char *)spUID, 8));
  SET_INT_VALUE(&startSessionParameters, 1);  /* Always open a Write Session */
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(&startSessionParameters);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent StartTLSSession Method\n"));

  resetParameters(&driveResponse);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION_MANAGER);
printf("AFTER RECV_PAYLOAD\n");
  DECODE_METHOD_CALL(SYNC_TLS_SESSION_METHOD_UID);
printf("AFTER DECODE METHOD CALL\n");
  DECODE_START_PARAMETER_LIST;
printf("AFTER DECODE START PARAMETER LIST\n");
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&driveResponse);
  }
printf("AFTER PEEK\n");
  DECODE_END_PARAMETER_LIST;
printf("AFTER DECODE END\n");
  DECODE_END_OF_DATA_TOKEN;
  DEBUG(("End Of Data Token received.\n"));
  DECODE_METHOD_STATUS_SUCCESS;
  DEBUG(("Method call status was a success.\n"));

  GET_INT_VALUE(&driveResponse, syncSessionHostID);
  GET_INT_VALUE(&driveResponse, syncSessionTPerID);

  if (syncSessionHostID != newHostSessionID) {
    printf("SyncTLSSessionHostID: %jd, expected: %jd\n", (uintmax_t)syncSessionHostID, (uintmax_t)newHostSessionID);
    return ERROR_("SyncTLSSession: wrong Host Session ID");
  }

  currentTPerSessionID = syncSessionTPerID;
  currentHostSessionID = newHostSessionID;
  DEBUG(("Started TLS Session! HostID: 0x%jx, TPerID: 0x%jx\n", (uintmax_t)currentHostSessionID, (uintmax_t)currentTPerSessionID));

  /* set Global TLS Flag so all messages will be encrypted or decrypted accordingly                */
  /* with this flag set the current CIphersuite method that was chosen to be used for all messages */
  return SUCCESS;
  
}
#endif


parameters issueAuthenticateParameters;
parameters issueAuthenticateResults;

status issueAuthenticate(char *authority, tcgByteValue *pin) {
  status s;
  uid authorityUID;
  uint64_t authenticate_result;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  if (authority == NULL) {
    return API_ERROR("issueAuthenticate: NULL authority pointer");
  }
  if (pin == NULL) {
    return API_ERROR("issueAuthenticate: NULL pin pointer");
  }

  authorityUID = authorityNameToUID(authority);
  if (authorityUID == NULL) {
    return API_ERROR("issueAuthenticate: unknown authority name");
  }

  resetParameters(&issueAuthenticateParameters);
  SET_BYTE_VALUE(&issueAuthenticateParameters, tcgTmpNameCount((char *)authorityUID, 8));
  if (ourTransport->ssc == OPAL_SSC) {
    SET_ID_BYTE_VALUE(&issueAuthenticateParameters, AUTHENTICATE_METHOD_PROOF_PARAMETER_ID, pin);
  } else {
    SET_NAMED_BYTE_VALUE(&issueAuthenticateParameters, tcgTmpName("Challenge"), pin);
  }

  resetSendPayload();
  ENCODE_TO_SESSION;
  if (ourTransport->ssc == OPAL_SSC) {
    ENCODE_METHOD_CALL(SPUID, AUTHENTICATE_METHOD_UID_OPAL);
  } else {
    ENCODE_METHOD_CALL(SPUID, AUTHENTICATE_METHOD_UID_ENTERPRISE);
  }
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(&issueAuthenticateParameters);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Authenticate Method\n"));

  resetParameters(&issueAuthenticateResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DEBUG(("Received Authenticate Result\n"))

  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueAuthenticateResults);
  }
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;

  if (countParameters(&issueAuthenticateResults) != 1) {
    return ERROR_("issueErase: Authenticate method results, expected only one value returned.");
  }

  s = getIntValue(&issueAuthenticateResults, &authenticate_result);
  if (hasError(s)) {
     return ERROR_("Authenticate result not an integer");
  }
  if (authenticate_result == 0) {
    return ERROR_("issueAuthenticate: Authenticate Method failed");
  }
  return SUCCESS;
}

parameters issueAuthenticateCRParameters;
parameters authenticateCRResults;

status issueAuthenticateCR(char *authority, tcgByteValue *challenge, tcgByteValue *response) {
  uid authorityUID;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  if (authority == NULL) {
    return API_ERROR("issueAuthenticateCR: NULL authority pointer");
  }

  if ((challenge == NULL) && (response == NULL)) {
    return API_ERROR("issueAuthenticateCR: challenge and response cannot both be NULL");
  }

  if ((challenge != NULL) && (response != NULL)) {
    return API_ERROR("issueAuthenticateCR: challenge and response cannot both be non-NULL");
  }

  authorityUID = authorityNameToUID(authority);
  if (authorityUID == NULL) {
    return API_ERROR("issueAuthenticateCR: unknown authority name");
  }

  resetParameters(&issueAuthenticateCRParameters);
  SET_BYTE_VALUE(&issueAuthenticateCRParameters, tcgTmpNameCount((char *)authorityUID, 8));
  if (response != NULL) {
    if (ourTransport->ssc == OPAL_SSC) {
     SET_ID_BYTE_VALUE(&issueAuthenticateCRParameters, AUTHENTICATE_METHOD_PROOF_PARAMETER_ID, response);
    } else {
     SET_NAMED_BYTE_VALUE(&issueAuthenticateCRParameters, tcgTmpName("Challenge"), response);
    }
  }

  resetSendPayload();
  ENCODE_TO_SESSION;
  if (ourTransport->ssc == OPAL_SSC) {
    ENCODE_METHOD_CALL(SPUID, AUTHENTICATE_METHOD_UID_OPAL);
  } else {
    ENCODE_METHOD_CALL(SPUID, AUTHENTICATE_METHOD_UID_ENTERPRISE);
  }
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(&issueAuthenticateCRParameters);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Authenticate Method\n"));

  resetParameters(&authenticateCRResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DEBUG(("Received Authenticate Result\n"))
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&authenticateCRResults);
  }
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;

  if (countParameters(&authenticateCRResults) != 1) {
    return ERROR_("issueAuthenicateCR: Incorrect number of results returned");
  }
  if (challenge != NULL) {
    getByteValue(&authenticateCRResults, challenge);
  } else {
    uint64_t authenticate_result;
    getIntValue(&authenticateCRResults, &authenticate_result);
    if (authenticate_result == 0) {
      return ERROR_("issueAuthenticateCR: Authenticate Method failed");
    }
  }

  return SUCCESS;
}


/*Make sure to check the packet headers for session numbers!*/
status issueGet(uid objectUid, parameters *getParameters, parameters *results) {
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  if (objectUid == NULL) {
    return API_ERROR("issueGet: NULL objectUid pointer");
  }
  if (results == NULL) {
    return API_ERROR("issueGet: NULL results pointer");
  }
  resetSendPayload();
  ENCODE_TO_SESSION;
  if (ourTransport->ssc == OPAL_SSC) {
    ENCODE_METHOD_CALL(objectUid, GET_METHOD_UID_OPAL);
  } else {
    ENCODE_METHOD_CALL(objectUid, GET_METHOD_UID_ENTERPRISE);
  }
  ENCODE_START_PARAMETER_LIST;
  if (getParameters == NULL) {
      ENCODE_START_PARAMETER_LIST;
      ENCODE_END_PARAMETER_LIST;
  } else {
      ENCODE_PARAMETERS(getParameters);
  }
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));

  resetParameters(results);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(results);
  }
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;

  return SUCCESS;
}


parameters issueSetResults;
status issueSet(uid objectUid, parameters *valuesToSet) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  if (ourTransport->ssc == OPAL_SSC) {
    ENCODE_METHOD_CALL(objectUid, SET_METHOD_UID_OPAL);
  } else {
    ENCODE_METHOD_CALL(objectUid, SET_METHOD_UID_ENTERPRISE);
  }
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(valuesToSet);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Set Method\n"));

  resetParameters(&issueSetResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueSetResults);
  }
  /* Note: The parameter returned by Set is redundant with the method status.
           Persnickety clients may wish to double check the value.
     Note: This redundancy was eliminated in Core Spec 2 where only the method
           status is used, there is no result value.
  */
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;

  return SUCCESS;
}

parameters issueActivateResults;

status issueActivate(parameters *valuesToActivate) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_METHOD_CALL(spNameToUID("LockingSP"), ACTIVATE_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(valuesToActivate);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Activate Method\n"));

  resetParameters(&issueActivateResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueActivateResults);
  }
  DECODE_END_PARAMETER_LIST;
  if (countParameters(&issueActivateResults) != 0) {
    return ERROR_("issueActivate: Activate method returned results, none expected");
  }
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;
  return SUCCESS;
}

parameters issueReactivateResults;

status issueReactivate(parameters *valuesToReactivate) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_METHOD_CALL(SPUID, REACTIVATE_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(valuesToReactivate);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Reactivate Method\n"));

  resetParameters(&issueReactivateResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueReactivateResults);
  }
  DECODE_END_PARAMETER_LIST;
  if (countParameters(&issueReactivateResults) != 0) {
    return ERROR_("issueReactivate: Reactivate method returned results, none expected");
  }
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;
  return SUCCESS;
}



parameters issueGenKeyResults;

status issueGenKey(int bandNumber) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_METHOD_CALL(bandNumberToMEKUID(bandNumber), GENKEY_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent GenKey Method\n"));

  resetParameters(&issueGenKeyResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueGenKeyResults);
  }
  DECODE_END_PARAMETER_LIST;
  if (countParameters(&issueGenKeyResults) != 0) {
    return ERROR_("issueGenKey: GenKey method returned results, none expected");
  }
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;
  return SUCCESS;
}



parameters issueEraseResults;

status issueErase(int bandNumber) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_METHOD_CALL(bandNumberToUID(bandNumber), ERASE_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Erase Method\n"));

  resetParameters(&issueEraseResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueEraseResults);
  }
  DECODE_END_PARAMETER_LIST;
  if (countParameters(&issueEraseResults) != 0) {
    return ERROR_("issueErase: Erase method returned results, none expected");
  }
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;
  return SUCCESS;
}


parameters issueRevertSPResults;

status issueRevertSP(parameters *valuesToRevertSP) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_METHOD_CALL(SPUID, REVERTSP_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_PARAMETERS(valuesToRevertSP);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent RevertSP Method\n"));

  resetParameters(&issueRevertSPResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueRevertSPResults);
  }
  DECODE_END_PARAMETER_LIST;
  if (countParameters(&issueRevertSPResults) != 0) {
    return ERROR_("issueRevertSP: RevertSP method returned results, none expected");
  }
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;

  DEBUG(("Got RevertSP Result\n"));

  /* Revert SP will close all sessions once we've retreived the method status result,
   * but will do so without sending any other indication to the host.
   */
  clearSessionIDs();
  return SUCCESS;
}


parameters issueRandomResults;

status issueRandom(int byteCount, char *result) {
  status haveResult;
  tcgByteValue tcgResult;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  if ((byteCount <= 0) || (byteCount > 32)) {
    return API_ERROR("issueRandom: byteCount value out of range.");
  }
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_METHOD_CALL(SPUID, RANDOM_METHOD_UID);
  ENCODE_START_PARAMETER_LIST;
  ENCODE_INTEGER(byteCount);
  ENCODE_END_PARAMETER_LIST;
  ENCODE_END_OF_DATA_TOKEN;
  ENCODE_METHOD_STATUS_SUCCESS;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Random Method\n"));

  resetParameters(&issueRandomResults);
  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_PARAMETER_LIST;
  while (!PEEK_END_PARAMETER_LIST()) {
    DECODE_PARAMETER(&issueRandomResults);
  }
  DECODE_END_PARAMETER_LIST;
  DECODE_END_OF_DATA_TOKEN;
  DECODE_METHOD_STATUS_SUCCESS;
  haveResult = getByteValue(&issueRandomResults, &tcgResult);
  if (haveResult != SUCCESS) { return haveResult; }
  if (tcgResult.len != (unsigned int)byteCount) {
    return ERROR_("issueRandom: result not of length requested.");
  }
  memcpy(result, tcgResult.data, tcgResult.len);  /* Do NOT null terminate! */
  return SUCCESS;
}

static char _transaction_error_msg[200];

status issueStartTransaction(void) {
  uint64_t startAckValue = 0xDEADBEEF;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;

  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_START_TRANSACTION;
  ENCODE_INTEGER(0);   /* Drive should ignore, but a placeholder is needed. */
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Start Transaction"));

  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_START_TRANSACTION;
  DECODE_INTEGER(startAckValue);
  DEBUG(("...Drive Start Transaction response code: 0x%0llux\n", startAckValue));
  if (startAckValue == 0) {
    return SUCCESS;
  }
  sprintf(_transaction_error_msg, "issueStartTransaction: error starting transaction: 0x%0llux", startAckValue);
  return ERROR_(_transaction_error_msg);

}


status issueAbortTransaction(void) {
  uint64_t abortAckValue = 0xDEADBEEF;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;

  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_END_TRANSACTION;
  ENCODE_INTEGER(TRANSACTION_ABORT_STATUS_CODE);
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Abort Transaction"));

  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_END_TRANSACTION;
  DECODE_INTEGER(abortAckValue);
  DEBUG(("...Drive Abort Transaction response code: 0x%0llux\n", abortAckValue));
  if (abortAckValue == TRANSACTION_ABORT_STATUS_CODE) {
    return SUCCESS;
  }
  sprintf(_transaction_error_msg, "issueAbortTransaction: error aborting transaction: 0x%0llux", abortAckValue);
  return ERROR_(_transaction_error_msg);
}


status issueCommitTransaction(void) {
  uint64_t commitAckValue = 0xDEADBEEF;
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;

  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_END_TRANSACTION;
  ENCODE_INTEGER(TRANSACTION_COMMIT_STATUS_CODE);
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent Commit Transaction"));

  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_END_TRANSACTION;
  DECODE_INTEGER(commitAckValue);
  DEBUG(("...Drive Commit Transaction response code: 0x%0llux\n", commitAckValue));
  if (commitAckValue == TRANSACTION_COMMIT_STATUS_CODE) {
    return SUCCESS;
  }
  sprintf(_transaction_error_msg, "issueCommitTransaction: error commiting transaction: 0x%0llux", commitAckValue);
  return ERROR_(_transaction_error_msg);
}


status issueCloseSession(void) {
  CHECK_DEBUG;
  CHECK_TRANSPORT;
  CHECK_SESSION_OPEN;
  resetSendPayload();
  ENCODE_TO_SESSION;
  ENCODE_END_OF_SESSION_TOKEN;
  FIX_PAYLOAD_HEADERS;
  SEND_PAYLOAD(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID));
  DEBUG(("Sent End of Session Token\n"));

  RECV_PAYLOAD_WITH_RETRY(TCG_METHOD_CALL_PROTOCOL_ID, SHORT_COMID(ourTransport->comID), PACKET_SESSION);
  DECODE_END_OF_SESSION_TOKEN;
  clearSessionIDs();

  return SUCCESS;
}

