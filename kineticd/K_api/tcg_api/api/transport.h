#ifndef TRANSPORT_H
#define TRANSPORT_H

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


#include <stdint.h>
#include "status_codes.h"

/**
 * TCG Payload sizes. Does not account for interface specific overhead (CDB, TFR, etc.)
 * Size may need to be rounded up to a transport "block" boundary, and if so, is left as
 * an exersize to the implementor. 512 was picked as the "block" size for ATA transfers,
 * and does not have to match the user data "block" size on the media.
 */
#define PAYLOAD_SECTOR_SIZE  512

/**
 * Sector count of 3 was chosen to allow retrieval of the DataStore table which is 1024 bytes
 * on Enterprise drives. Because of overhead for the TCG structures, we cannot retrieve
 * the full DataStore table in just two sectors. If you need to retrieve larger amounts of data
 * from the drive, you may need to increase this number.
 */
#define PAYLOAD_SECTOR_COUNT 3

#define PAYLOAD_BUFFER_SIZE (PAYLOAD_SECTOR_COUNT*PAYLOAD_SECTOR_SIZE)

/** The SSC (Security Subsystem Class) type. This is used strictly in determining
 *  whether to use Core Spec 1 or Core Spec 2 protocol to talk to the drive. For
 *  this purpose, Opal SSC and Opal SSC v2.00 are both considered Opal.
 */
typedef enum {ENTERPRISE_SSC,
              OPAL_SSC
              } ssc_t;

/**
 * transport is a structure that captures the data and functions needed by the API in order
 * to exchange TCG payloads with the drive. As such it is interface agnostic and it is not
 * intended to generalize to other kinds of drive commands.
 *
 * To simplify memory management, a transport structure contains all the buffers it needs.
 *
 * NOTE: The functions in the transport structure do not take a transport parameter, they are
 * required to know which transport structure to operate on.
 *
 * The sendFunction and recvFunction are expected to send the interface specific TCG command
 * and return a status back to the caller. It is further assumed that the calling code
 * is free to invoke other send/recvFunction calls without having to take additional
 * steps to "clear or unblock" the underlying interface. The send/recvFunction calls are
 * expected to handle all the details of delivering and retrieving status from the command
 * and payload. The size of the data to be transfered can be computed from the buffer
 * and the corresponding Tail pointer. The only fields of the transport structure
 * that the recvFunction should change are the recvBuffer and recvBuffer tail.
 * The sendFunction should not change any of the transport fields.
 */

struct transport {
    /** Used to deliver a TCG payload to the device, from the sendBuffer field. */
    status (*sendFunction)(int protocolId, int comId);
    
    /** Used to receive a TCG payload from the device, into the recvBuffer field. */
    status (*recvFunction)(int protocolId, int comId);

    /** Extended ComID to use with this transport. Short form derived as needed. */
    uint64_t comID;

    /** Which SSC (Opal/Opalv2, or Enterprise) that this drive is supporting.
     *  The transport layer does not use this directly, it is here so that low_level
     *  and friends can use it, as that code supports having more than one transport
     *  talking to more than one kind of drive.
     */
    ssc_t ssc;
  
    /** space to hold a payload to be delivered to the device.
     *  The + 1 is to allow room for the tail to still point within the buffer
     *  and is a concession to some compilers which might not support the standard
     *  that requires a pointer to be allowed to point to one past the end of an array.
     */
    unsigned char sendBuffer[PAYLOAD_BUFFER_SIZE + 1];

    /** points into sendBuffer one byte past the last valid byte to be sent */
    unsigned char *sendBufferTail;
  
    /** Address of first (MSB) byte of the ComPacketLength field.
     *  When sending, to be back filled after payload is constructed.
     *  When receiving, set as the payload is parsed.
     */
    unsigned char *comPacketLengthPtr;

    /** Address of first (MSB) byte of the PacketLength field.
     *  When sending, to be back filled after payload is constructed.
     *  When receiving, set as the payload is parsed.
     */
    unsigned char *packetLengthPtr;

    /** Address of first (MSB) byte of the SubPacketLength field.
     *  When sending, to be back filled after payload is constructed.
     *  When receiving, set as the payload is parsed.
     */
    unsigned char *subPacketLengthPtr;

    /** space to hold a payload retrieved from the device. See the description
     *  of sendBuffer for why there is a plus 1 here. */
    unsigned char recvBuffer[PAYLOAD_BUFFER_SIZE + 1];

    /** points into recvBuffer one byte past the last valid byte read from the device. */
    unsigned char *recvBufferTail;

    /** points into recvBuffer one byte past the last byte read so far from the buffer.
     *  (If the less than recvBufferTail, this will point to the next byte to read.)
     */
    unsigned char *recvBufferPos;
};


typedef struct transport transport;

extern void dumpSendBuffer(transport *t);
extern void dumpRecvBuffer(transport *t);
extern void dumpBuffer(unsigned char *start, unsigned char *tail);

#endif
