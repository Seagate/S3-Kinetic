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
#include <ctype.h>
#include "transport.h"

#define LINE_WIDTH 8  /* Bytes per line */

static char dumpPrintables[LINE_WIDTH+1];
static int printablesIndex;

void dumpBuffer(unsigned char *start, unsigned char *tail) {
  uint64_t dumpCount;
  unsigned char *dumpPtr;
  uint64_t count;
  dumpCount = tail - start;
  printablesIndex = 0;
  printf("%jd(0x%02jx) bytes to transfer: %p to %p\n", (uintmax_t)dumpCount, (uintmax_t)dumpCount, start, tail);
  for (dumpPtr = start, count = 0; dumpPtr < tail ; dumpPtr++, count++) {
    if ((count % LINE_WIDTH) == 0) {
      printf("0x%04llux: ", count);
    }
    printf("  0x%02x", *dumpPtr);
    dumpPrintables[printablesIndex++] = isprint(*dumpPtr) ? (*dumpPtr) : '#';

    if ((count % LINE_WIDTH) == (LINE_WIDTH - 1)) {
      dumpPrintables[printablesIndex] = '\0';
      printf(" - %s\n", dumpPrintables);
      printablesIndex = 0;
    }
  }
  printf("\n\n");
}

void dumpSendBuffer(transport *t) {
  if (t == NULL) {
    printf("dumpSendBuffer: NULL transport pointer\n");
    return;
  }
  dumpBuffer(t->sendBuffer, t->sendBufferTail);
}
void dumpRecvBuffer(transport *t) {
  if (t == NULL) {
    printf("dumpRecvBuffer: NULL transport pointer\n");
    return;
  }
  dumpBuffer(t->recvBuffer, t->recvBufferTail);
}
