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

#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "high_level.h"
#include "transport_fake.h"

#include "debug_level_0_data_dump.h"

static unsigned char propertiesResponseBuf[] = {

  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Reserved */
  0x00, 0x00, 0x07, 0xFE,  /* ComPacket: Extended ComID */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Outstanding Data */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: MinTransfer */

  0x00, 0x00, 0x00, 24 + 12 + 189,  /* ComPacket: Length */
  0x00, 0x00, 0x00, 0x00,  /* Packet: TPer Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Host Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Sequence # */

  0x00, 0x00,              /* Packet: Reserved */
  0x00, 0x00,              /* Packet: AckType */

  0x00, 0x00, 0x00, 0x00,  /* Packet: Acknowledgement */
  0x00, 0x00, 0x00, 12 + 189,  /* Packet: Length */

  0x00, 0x00, 0x00, 0x00,  /* SubPacket: Reserved1of2 */
  0x00, 0x00,              /* SubPacket: Reserved2of2 */

  0x00, 0x00,              /* SubPacket: Kind */

  0x00, 0x00, 0x00,   189,  /* SubPacket: Length */
  0xF8, 0xA8,                  /*  2 */
  0x00, 0x00, 0x00, 0x00,      /*  6 */
  0x00, 0x00, 0x00, 0xFF,      /* 10 */

  0xA8,                        /* 11 */
  0x00, 0x00, 0x00, 0x00,      /* 15 */
  0x00, 0x00, 0xFF, 0x01,      /* 19 */
  
  0xF0,                        /* 20 */
  0xF0,                        /* 21 */

  0xF2,                        /* 22 */
  0xAD, 'M', 'a', 'x',         /* 26 */
  'P', 'a', 'c', 'k',          /* 30 */
  'e', 't', 'S', 'i',          /* 34 */
  'z', 'e',                    /* 36 */
  0x04,  /* Way too small */   /* 37 */
  0xF3,                        /* 38 */

  0xF2,                        /* 39 */
  0xD0,                        /* 40 */
  16, 'M', 'a', 'x',           /* 44 */
  'C',  'o', 'm', 'P',         /* 48 */
  'a', 'c', 'k', 'e',          /* 52 */
  't', 's', 'i', 'z',          /* 56 */
  'e',                         /* 57 */
  0x02,  /* Way too small */   /* 58 */
  0xF3,                        /* 59 */

  0xF2,                        /* 60 */
  0xD0,                        /* 61 */
   24, 'M', 'a', 'x',          /* 65 */
  'R', 'e', 's', 'p',          /* 69 */
  'o', 'n', 's', 'e',          /* 73 */
  'C', 'o', 'm', 'P',          /* 77 */
  'a', 'c', 'k', 'e',          /* 81 */
  't', 'S', 'i', 'z',          /* 85 */
  'e',                         /* 86 */
  0x0E,                        /* 87 */
  0xF3,                        /* 88 */

  0xF2,                        /* 89 */
  0xD0,                        /* 90 */
   18, 'M', 'a', 'x',          /* 94 */
  'A', 'u', 't', 'h',          /* 98 */
  'e', 'n', 't', 'i',          /* 102 */
  'c', 'a', 't', 'i',          /* 106 */
  'o', 'n', 's',               /* 109 */
  0x0C, /* Way too small */    /* 110 */
  0xF3,                        /* 111 */

  0xF2,                        /* 112 */
  0xD0,                        /* 113 */
   21, 'D', 'e', 'f',          /* 117 */
  'a', 'u', 'l', 't',          /* 121 */
  'S', 'e', 's', 's',          /* 125 */
  'i', 'o', 'n', 'T',          /* 129 */
  'i', 'm', 'e', 'o',          /* 133 */
  'u', 't',                    /* 135 */
  0x09, /* Way too small */    /* 136 */
  0xF3,                        /* 137 */

  0xF2,                        /* 138 */
  0xD0,                        /* 139 */
   17, 'M', 'a', 'x',          /* 143 */
  'S', 'e', 's', 's',          /* 147 */
  'i', 'o', 'n', 'T',          /* 151 */
  'i', 'm', 'e', 'o',          /* 155 */
  'u', 't',                    /* 157 */
  0x02, /* Way too small */    /* 158 */
  0xF3,                        /* 159 */

  0xF2,                        /* 160 */
  0xD0,                        /* 161 */
   17, 'M', 'i', 'n',          /* 165 */
  'S', 'e', 's', 's',          /* 169 */
  'i', 'o', 'n', 'T',          /* 173 */
  'i', 'm', 'e', 'o',          /* 177 */
  'u', 't',                    /* 179 */
  0x08, /* Way too small */    /* 180 */
  0xF3,                        /* 181 */

  0xF1,                        /* 182 */
  0xF1,                        /* 183 */
  0xF9,                        /* 184 */
  0xF0, 0x00, 0x00, 0x00,      /* 188 */
  0xF1,                        /* 189 */
};

static unsigned char startSessionResponseBuf[] = {

  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Reserved */
  0x00, 0x00, 0x07, 0xFE,  /* ComPacket: Extended ComID */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Outstanding Data */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: MinTransfer */

  0x00, 0x00, 0x00, 24 + 12 + 32,  /* ComPacket: Length */
  0x00, 0x00, 0x00, 0x00,  /* Packet: TPer Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Host Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Sequence # */

  0x00, 0x00,              /* Packet: Reserved */
  0x00, 0x00,              /* Packet: AckType */

  0x00, 0x00, 0x00, 0x00,  /* Packet: Acknowledgement */
  0x00, 0x00, 0x00, 12 + 32,  /* Packet: Length */

  0x00, 0x00, 0x00, 0x00,  /* SubPacket: Reserved1of2 */
  0x00, 0x00,              /* SubPacket: Reserved2of2 */

  0x00, 0x00,              /* SubPacket: Kind */

  0x00, 0x00, 0x00,   32,  /* SubPacket: Length */
  0xF8, 0xA8,                  /*  2 */
  0x00, 0x00, 0x00, 0x00,      /*  6 */
  0x00, 0x00, 0x00, 0xFF,      /* 10 */

  0xA8,                        /* 11 */
  0x00, 0x00, 0x00, 0x00,      /* 15 */
  0x00, 0x00, 0xFF, 0x03,      /* 19 */
  
  0xF0,                        /* 20 */
  0xF0,                        /* 21 */

  0x81, 100,                   /* 23 */
  0x0A,                        /* 24 */

  0xF1,                        /* 25 */
  0xF1,                        /* 26 */
  0xF9,                        /* 27 */
  0xF0, 0x00, 0x00, 0x00,      /* 31 */
  0xF1,                        /* 32 */
};


static unsigned char getForMSIDResponseBuf[] = {

  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Reserved */
  0x00, 0x00, 0x07, 0xFE,  /* ComPacket: Extended ComID */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Outstanding Data */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: MinTransfer */

  0x00, 0x00, 0x00, 24 + 12 + 52,  /* ComPacket: Length */
  0x00, 0x00, 0x00, 0x0A,  /* Packet: TPer Session # */
  0x00, 0x00, 0x00, 0x64,  /* Packet: Host Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Sequence # */

  0x00, 0x00,              /* Packet: Reserved */
  0x00, 0x00,              /* Packet: AckType */

  0x00, 0x00, 0x00, 0x00,  /* Packet: Acknowledgement */
  0x00, 0x00, 0x00, 12 + 52,  /* Packet: Length */

  0x00, 0x00, 0x00, 0x00,  /* SubPacket: Reserved1of2 */
  0x00, 0x00,              /* SubPacket: Reserved2of2 */

  0x00, 0x00,              /* SubPacket: Kind */

  0x00, 0x00, 0x00,   52,  /* SubPacket: Length */
  0xF0,                        /* 1 */
  0xF0,                        /* 2 */
  0xF0,                        /* 3 */
  0xF2,                        /* 4 */
  0xA3, 'P', 'I', 'N',         /* 8 */
  0xD0, 32,                    /* 10 */
  'A', 'A', 'A', 'A',          /* 14 */
  'A', 'A', 'A', 'A',          /* 18 */
  'A', 'A', 'A', 'A',          /* 22 */
  'A', 'A', 'A', 'A',          /* 26 */
  'A', 'A', 'A', 'A',          /* 30 */
  'A', 'A', 'A', 'A',          /* 34 */
  'A', 'A', 'A', 'A',          /* 38 */
  'A', 'A', 'A', 'A',          /* 42 */
  0xF3,                        /* 43 */

  0xF1,                        /* 44 */
  0xF1,                        /* 45 */
  0xF1,                        /* 46 */
  0xF9,                        /* 47 */
  0xF0, 0x00, 0x00, 0x00,      /* 51 */
  0xF1,                        /* 52 */
};


static unsigned char closeSessionResponseBuf[] = {

  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Reserved */
  0x00, 0x00, 0x07, 0xFE,  /* ComPacket: Extended ComID */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: Outstanding Data */
  0x00, 0x00, 0x00, 0x00,  /* ComPacket: MinTransfer */

  0x00, 0x00, 0x00, 24 + 12 + 30,  /* ComPacket: Length */
  0x00, 0x00, 0x00, 0x00,  /* Packet: TPer Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Host Session # */
  0x00, 0x00, 0x00, 0x00,  /* Packet: Sequence # */

  0x00, 0x00,              /* Packet: Reserved */
  0x00, 0x00,              /* Packet: AckType */

  0x00, 0x00, 0x00, 0x00,  /* Packet: Acknowledgement */
  0x00, 0x00, 0x00, 12 + 30,  /* Packet: Length */

  0x00, 0x00, 0x00, 0x00,  /* SubPacket: Reserved1of2 */
  0x00, 0x00,              /* SubPacket: Reserved2of2 */

  0x00, 0x00,              /* SubPacket: Kind */

  0x00, 0x00, 0x00,   30,  /* SubPacket: Length */
  0xF8, 0xA8,                  /*  2 */
  0x00, 0x00, 0x00, 0x00,      /*  6 */
  0x00, 0x00, 0x00, 0xFF,      /* 10 */

  0xA8,                        /* 11 */
  0x00, 0x00, 0x00, 0x00,      /* 15 */
  0x00, 0x00, 0xFF, 0x06,      /* 19 */
  0xF0,                        /* 20 */
  0x81, 100,                   /* 22 */
  0x0A,                        /* 23 */
  0xF1,                        /* 24 */
  0xF9,                        /* 25 */
  0xF0, 0x00, 0x00, 0x00,      /* 29 */
  0xF1,                        /* 30 */
};

static transport t;
static int recv_count = 0;

status bogusSend(int protocolId, int comId) {
  CHECK_DEBUG;
  DEBUG(("\nbogusSend(%d, 0x%x)\n", protocolId, comId));
  DEBUG_S(dumpSendBuffer(&t));
  return SUCCESS;
}

static void _copyRecvBuff(unsigned char *dummyData, uint64_t dummySize) {
  memcpy(t.recvBuffer, dummyData, dummySize);
  t.recvBufferTail = t.recvBuffer + dummySize;
}

status bogusRecv(int protocolId, int comId) {
  CHECK_DEBUG;
  DEBUG(("\nbogusRecv(%d, 0x%x)\n", protocolId, comId));
  recv_count++;
  switch (recv_count) {
  case 1:
    _copyRecvBuff(discoveryDataBuf1, sizeof(discoveryDataBuf1));
    break;
  case 2:
    _copyRecvBuff(propertiesResponseBuf, sizeof(propertiesResponseBuf));
    break;
  case 3:
    _copyRecvBuff(startSessionResponseBuf, sizeof(startSessionResponseBuf));
    break;
  case 4:
    _copyRecvBuff(getForMSIDResponseBuf, sizeof(getForMSIDResponseBuf));
    break;
  case 5:
    _copyRecvBuff(closeSessionResponseBuf, sizeof(closeSessionResponseBuf));
    break;
  }

  DEBUG_S(dumpRecvBuffer(&t));
  return SUCCESS;
}

transport *fake(void) {
  t.sendFunction = bogusSend;
  t.recvFunction = bogusRecv;
  return &t;
}
