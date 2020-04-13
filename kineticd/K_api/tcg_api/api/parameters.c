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
#include <stdio.h>
#include <string.h>
#include <ctype.h>   /* For debugging only */
#include "parameters.h"
#include "utilities.h"
#include "debug.h"

static char tcgByteValueDebugBuffer[(TCG_BYTE_VALUE_MAX_SIZE * 2) + 1];

static char get_set_values_error_msg_buffer[2048]; /* Resize as needed */

char *tcgByteValueDebugStr(tcgByteValue *v) {
  unsigned int i;
  char *curPos;
  if (v == NULL) {
    return "<null>";
  }
  curPos = tcgByteValueDebugBuffer + sprintf(tcgByteValueDebugBuffer, "[%d]", v->len);
  for (i=0; i < v->len; i++) {
    if (isprint(v->data[i])) {
      *curPos++ = v->data[i];
    } else {
      curPos += sprintf(curPos, "(%02X)", v->data[i]);
    }
  }
  *curPos = '\0';
  return tcgByteValueDebugBuffer;
}

char *tcgByteValueDebugAllHexStr(tcgByteValue *v) {
  unsigned int i;
  char *curPos;
  if (v == NULL) {
    return "<null>";
  }
  curPos = tcgByteValueDebugBuffer + sprintf(tcgByteValueDebugBuffer, "[%d]", v->len);
  for (i=0; i < v->len; i++) {
   curPos += sprintf(curPos, "(%02X)", v->data[i]);
  }
  *curPos = '\0';
  return tcgByteValueDebugBuffer;
}

static tcgByteValue tempByteValue;

tcgByteValue *tcgTmpName(char *c_string) {
  if (c_string == NULL) {
    return NULL;
  }
  tcgByteValueFromString(&tempByteValue, c_string);
  return &tempByteValue;
}

tcgByteValue *tcgTmpNameCount(char *c_string, unsigned int count) {
  if (c_string == NULL) {
    return NULL;
  }
  tcgByteValueFromStringCount(&tempByteValue, c_string, count);
  return &tempByteValue;
}

int tcgByteValueEqual(tcgByteValue *a, tcgByteValue *b) {
  if ((a == NULL) || (b == NULL)) {
    return 0;
  }
  if (a->len != b->len) {
    return 0;
  }
  return !memcmp(a->data, b->data, a->len);
}

void tcgByteValueCopy(tcgByteValue *dest, tcgByteValue *src) {
  if ((dest == NULL) || (src == NULL)) {
    return;
  }
  dest->len = src->len;
  memcpy(dest->data, src->data, src->len);
}

void tcgByteValueFromString(tcgByteValue *dest, char *src) {
  if (dest == NULL) {
    return;
  }
  if (src == NULL) {
    dest->len = 0;
    return;
  }
  dest->len = strlen(src);
  memcpy(dest->data, src, dest->len);
}

void tcgByteValueFromStringCount(tcgByteValue *dest, char *src, unsigned int count) {
  if (dest == NULL) {
    return;
  }
  if (src == NULL) {
    dest->len = 0;
    return;
  }
  dest->len = count;
  memcpy(dest->data, src, dest->len);
}

void stringFromTcgByteValue(char *dest, tcgByteValue *src) {
  if (dest == NULL) {
    return;
  }
  if (src == NULL) {
    *dest = '\0';
    return;
  }
  memcpy(dest, src->data, src->len);
  dest[src->len] = '\0';
}

void resetParameters(parameters *p) {
  if (p == NULL) {
    return;
  }
  p->slotsUsed = 0;
  p->slotsRead = 0;
}

int countParameters(parameters *p) {
  if (p == NULL) {
    return -1;
  }
  return p->slotsUsed;
}

aParameter *findValueForId(parameters *p, uint64_t id) {
  int index;
  int lastPossibleSlot;
  CHECK_DEBUG;
  if (p == NULL) {
    return NULL;
  }
  lastPossibleSlot = p->slotsUsed - 4; /* ID values take up (at least) 4 slots */
  DEBUG(("Looking for ID '0x%llux', scanning %d slots\n", id, lastPossibleSlot));
  for (index = 0; index <= lastPossibleSlot; index++) {
    if (p->parameterList[index].parameterType == START_NAME_TYPE) {
      DEBUG(("Found Start Name\n"));
      DEBUG(("At ID slot, found: %d\n", p->parameterList[index+1].parameterType));
      if (p->parameterList[index+1].parameterType == INT_TYPE) {
        uint64_t candidate = p->parameterList[index+1].value.longValue;
        DEBUG(("...considering '0x%llux'\n", candidate));
        if (id == candidate) {
          DEBUG(("...Found it.\n"));
          return &(p->parameterList[index+2]);
        }
      }
    }
  }
  DEBUG(("...Could not find.\n"));
  return NULL;
}

aParameter *findValueForName(parameters *p, tcgByteValue *name) {
  int index;
  int lastPossibleSlot;
  CHECK_DEBUG;
  if ((p == NULL) || (name == NULL)) {
    return NULL;
  }
  lastPossibleSlot = p->slotsUsed - 4; /* Named values take up (at least) 4 slots */
  DEBUG(("Looking for Name '%s', scanning %d slots\n", tcgByteValueDebugStr(name), lastPossibleSlot));
  for (index = 0; index <= lastPossibleSlot; index++) {
    if (p->parameterList[index].parameterType == START_NAME_TYPE) {
      DEBUG(("Found Start Name\n"));
      DEBUG(("At name slot, found: %d\n", p->parameterList[index+1].parameterType));
      if (p->parameterList[index+1].parameterType == BYTE_TYPE) {
        tcgByteValue *candidate = &(p->parameterList[index+1].value.byteValue);
        DEBUG(("...considering '%s'\n", tcgByteValueDebugStr(candidate)));
        if (tcgByteValueEqual(name, candidate)) {
          DEBUG(("...Found it.\n"));
          return &(p->parameterList[index+2]);
        }
      }
    }
  }
  DEBUG(("...Could not find.\n"));
  return NULL;
}

status setSlotForId(parameters *p, uint64_t id) {
  int index;
  int lastPossibleSlot;
  CHECK_DEBUG;
  if (p == NULL) {
    return API_ERROR("setSlotForId given NULL pointer");
  }
  lastPossibleSlot = p->slotsUsed - 4; /* Named values take up (at least) 4 slots */
  DEBUG(("Looking for ID '0x%llux', scanning %d slots\n", id, lastPossibleSlot));
  for (index = 0; index <= lastPossibleSlot; index++) {
    if (p->parameterList[index].parameterType == START_NAME_TYPE) {
      DEBUG(("Found Start Name\n"));
      DEBUG(("At ID slot, found: %d\n", p->parameterList[index+1].parameterType));
      if (p->parameterList[index+1].parameterType == INT_TYPE) {
        uint64_t candidate = p->parameterList[index+1].value.longValue;
        DEBUG(("...considering '%llux'\n", candidate));
        if (id == candidate) {
          DEBUG(("...Found it.\n"));
          p->slotsRead = index+2;
          return SUCCESS;
        }
      }
    }
  }
  DEBUG(("...Could not find.\n"));
  return ERROR_("setSlotForName did not find the given ID");
}

status setSlotForName(parameters *p, tcgByteValue *name) {
  int index;
  int lastPossibleSlot;
  CHECK_DEBUG;
  if ((p == NULL) || (name == NULL)) {
    return API_ERROR("setSlotForName given NULL pointer");
  }
  lastPossibleSlot = p->slotsUsed - 4; /* Named values take up (at least) 4 slots */
  DEBUG(("Looking for Name '%s', scanning %d slots\n", tcgByteValueDebugStr(name), lastPossibleSlot));
  for (index = 0; index <= lastPossibleSlot; index++) {
    if (p->parameterList[index].parameterType == START_NAME_TYPE) {
      DEBUG(("Found Start Name\n"));
      DEBUG(("At name slot, found: %d\n", p->parameterList[index+1].parameterType));
      if (p->parameterList[index+1].parameterType == BYTE_TYPE) {
        tcgByteValue *candidate = &(p->parameterList[index+1].value.byteValue);
        DEBUG(("...considering '%s'\n", tcgByteValueDebugStr(candidate)));
        if (tcgByteValueEqual(name, candidate)) {
          DEBUG(("...Found it.\n"));
          p->slotsRead = index+2;
          return SUCCESS;
        }
      }
    }
  }
  DEBUG(("...Could not find.\n"));
  return ERROR_("setSlotForName did not find the given name");
}

status getStartList(parameters *p) {
  aParameter *foundItem;
  if (p == NULL) {
    return API_ERROR("getStartList: NULL paramaters pointer");
  }
  if (p->slotsRead >= p->slotsUsed) {
    return API_ERROR("getStartList: Would read past valid data");
  }
  foundItem = &(p->parameterList[p->slotsRead]);
  if (foundItem->parameterType != START_LIST_TYPE) {
    return ERROR_("getStartList: Token found is not START LIST token");
  }
  p->slotsRead++;
  return SUCCESS;
}

status setStartList(parameters *p) {
  if (p == NULL) {
    return API_ERROR("setStartList: NULL parameters pointer");
  }
  if (p->slotsUsed >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setStartList: parameters full");
  }
  p->parameterList[p->slotsUsed++].parameterType = START_LIST_TYPE;
  return SUCCESS;
}

int peekEndList(parameters *p) {
  aParameter *foundItem;
  if (p == NULL) {
    return 0;
  }
  if (p->slotsRead >= p->slotsUsed) {
    return 0;
  }
  foundItem = &(p->parameterList[p->slotsRead]);
  if (foundItem->parameterType != END_LIST_TYPE) {
    return 0;
  }
  return 1;
}

status getEndList(parameters *p) {
  aParameter *foundItem;
  if (p == NULL) {
    return API_ERROR("getEndList: NULL paramaters pointer");
  }
  if (p->slotsRead >= p->slotsUsed) {
    return API_ERROR("getEndList: Would read past valid data");
  }
  foundItem = &(p->parameterList[p->slotsRead]);
  if (foundItem->parameterType != END_LIST_TYPE) {
    return ERROR_("getEndList: Token found is not END LIST token");
  }
  p->slotsRead++;
  return SUCCESS;
}

status setEndList(parameters *p) {
  if (p == NULL) {
    return API_ERROR("setEndList: NULL parameters pointer");
  }
  if (p->slotsUsed >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setEndList: no room to add another value");
  }
  p->parameterList[p->slotsUsed++].parameterType = END_LIST_TYPE;
  return SUCCESS;
}

status setStartName(parameters *p) {
  if (p == NULL) {
    return API_ERROR("setStartName passed null parameters pointer");
  }
  if (p->slotsUsed >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setStartName: no room to add another value");
  }
  p->parameterList[p->slotsUsed++].parameterType = START_NAME_TYPE;
  return SUCCESS;
}

status setEndName(parameters *p) {
  if (p == NULL) {
    return API_ERROR("setEndName: NULL parameters pointer");
  }
  if (p->slotsUsed >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setEndName: no room to add another value");
  }
  p->parameterList[p->slotsUsed++].parameterType = END_NAME_TYPE;
  return SUCCESS;
}

status getIntValue(parameters *p, uint64_t *result) {
  aParameter *foundValue;
  if (p == NULL) {
    return API_ERROR("getIntValue: NULL paramaters pointer");
  }
  if (result == NULL) {
    return API_ERROR("getIntValue: NULL result pointer");
  }
  if (p->slotsRead >= p->slotsUsed) {
    return API_ERROR("getIntValue: would read past valid data");
  }
  foundValue = &(p->parameterList[p->slotsRead]);
  if (foundValue->parameterType != INT_TYPE) {
    return ERROR_("getIntValue: found value, but it was not an int value");
  }
  *result = foundValue->value.longValue;
  p->slotsRead++;
  return SUCCESS;
}

status setIntValue(parameters *p, uint64_t value) {
  int index;
  if (p == NULL) {
    return API_ERROR("setIntValue: NULL paramaters pointer");
  }
  index = p->slotsUsed;
  if (index >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setIntValue: no room to add another value");
  }
  p->parameterList[index].parameterType = INT_TYPE;
  p->parameterList[index].value.longValue = value;
  p->slotsUsed++;
  return SUCCESS;
}

status getIdIntValue(parameters *p, uint64_t id, uint64_t *result) {
  aParameter *value;
  if (p == NULL) {
    return API_ERROR("getIdIntValue: NULL paramaters pointer");
  }
  if (result == NULL) {
    return API_ERROR("getIdIntValue: NULL result pointer");
  }
  value = findValueForId(p, id);
  if (value == NULL) {
    sprintf(get_set_values_error_msg_buffer, "getIdIntValue: Cannot find ID '0x%llux'", id);
    return ERROR_(get_set_values_error_msg_buffer);
  }
  if (value->parameterType != INT_TYPE) {
    sprintf(get_set_values_error_msg_buffer, "getIdIntValue: value for ID '0x%llux' is not a Int Type", id);
    return ERROR_(get_set_values_error_msg_buffer);
  }
  *result = value->value.longValue;
  return SUCCESS;
}

status getNamedIntValue(parameters *p, tcgByteValue *name, uint64_t *result) {
  aParameter *value;
  if (p == NULL) {
    return API_ERROR("getNamedIntValue: NULL paramaters pointer");
  }
  if (name == NULL) {
    return API_ERROR("getNamedIntValue: NULL name pointer");
  }
  if (result == NULL) {
    return API_ERROR("getNamedIntValue: NULL result pointer");
  }
  value = findValueForName(p, name);
  if (value == NULL) {
    sprintf(get_set_values_error_msg_buffer, "getNamedIntValue: Cannot find name '%s'", tcgByteValueDebugStr(name));
    return ERROR_(get_set_values_error_msg_buffer);
  }
  if (value->parameterType != INT_TYPE) {
    sprintf(get_set_values_error_msg_buffer, "getNamedIntValue: value for name '%s' is not a Int Type", tcgByteValueDebugStr(name));
    return ERROR_(get_set_values_error_msg_buffer);
  }
  *result = value->value.longValue;
  return SUCCESS;
}

status setIdIntValue(parameters *p, uint64_t id, uint64_t value) {
  status s;
  if (p == NULL) {
    return API_ERROR("setIdIntValue: NULL paramaters pointer");
  }
  if (p->slotsUsed + 3 >= MAX_PARAMETER_LIST_SIZE) {
    sprintf(get_set_values_error_msg_buffer, "setIdIntValue: no room to add value for ID '0x%llux'", id);
    return API_ERROR(get_set_values_error_msg_buffer);
  }
  p->parameterList[p->slotsUsed++].parameterType = START_NAME_TYPE;
  s = setIntValue(p, id);
  if (s != SUCCESS) { return s; }
  s = setIntValue(p, value);
  if (s != SUCCESS) { return s; }
  p->parameterList[p->slotsUsed++].parameterType = END_NAME_TYPE;
  return SUCCESS;
}

status setNamedIntValue(parameters *p, tcgByteValue *name, uint64_t value) {
  status s;
  if (p == NULL) {
    return API_ERROR("setNamedIntValue: NULL paramaters pointer");
  }
  if (name == NULL) {
    return API_ERROR("setNamedIntValue: NULL name pointer");
  }
  if (p->slotsUsed + 3 >= MAX_PARAMETER_LIST_SIZE) {
    sprintf(get_set_values_error_msg_buffer, "setNamedIntValue: no room to add value for name '%s'", tcgByteValueDebugStr(name));
    return API_ERROR(get_set_values_error_msg_buffer);
  }
  p->parameterList[p->slotsUsed++].parameterType = START_NAME_TYPE;
  s = setByteValue(p, name);
  if (s != SUCCESS) { return s; }
  s = setIntValue(p, value);
  if (s != SUCCESS) { return s; }
  p->parameterList[p->slotsUsed++].parameterType = END_NAME_TYPE;
  return SUCCESS;
}

status getByteValue(parameters *p, tcgByteValue *result) {
  aParameter *src;
  if (p == NULL) {
    return API_ERROR("getByteValue: NULL paramaters pointer");
  }
  if (result == NULL) {
    return API_ERROR("getByteValue: NULL result pointer");
  }
  if (p->slotsRead >= p->slotsUsed) {
    return API_ERROR("getByteValue: would read past valid data");
  }
  src = &(p->parameterList[p->slotsRead]);
  if (src->parameterType != BYTE_TYPE) {
    sprintf(get_set_values_error_msg_buffer, "getByteValue: found value, but it was not a byte value");
    return ERROR_(get_set_values_error_msg_buffer);
  }
  tcgByteValueCopy(result, &(src->value.byteValue));
  p->slotsRead++;
  return SUCCESS;
}

status setByteValue(parameters *p, tcgByteValue *value) {
  int index;
  if (p == NULL) {
    return API_ERROR("setByteValue passed null parameters pointer");
  }
  if (value == NULL) {
    return API_ERROR("setByteValue passed null value pointer");
  }
  index = p->slotsUsed;
  if (index >= MAX_PARAMETER_LIST_SIZE) {
    sprintf(get_set_values_error_msg_buffer, "setByteValue no room to add another value");
    return API_ERROR(get_set_values_error_msg_buffer);
  }
  p->parameterList[index].parameterType = BYTE_TYPE;
  tcgByteValueCopy(&(p->parameterList[index].value.byteValue), value);
  p->slotsUsed++;
  return SUCCESS;
}

status getIdByteValue(parameters *p, uint64_t id, tcgByteValue *result) {
  aParameter *value;
  if (p == NULL) {
    return API_ERROR("getIdByteValue passed null parameters pointer");
  }
  if (result == NULL) {
    return API_ERROR("getIdByteValue passed null result pointer");
  }
  value = findValueForId(p, id);
  if (value == NULL) {
    sprintf(get_set_values_error_msg_buffer, "getIdByteValue: Cannot find ID: '0x%llux'", id);
    return ERROR_(get_set_values_error_msg_buffer);
  }
  if (value->parameterType != BYTE_TYPE) {
    sprintf(get_set_values_error_msg_buffer, "getIdByteValue:  value for ID '0x%llux' is not an Int Type", id);
    return ERROR_(get_set_values_error_msg_buffer);
  }
  tcgByteValueCopy(result, &(value->value.byteValue));
  return SUCCESS;
}

status getNamedByteValue(parameters *p, tcgByteValue *name, tcgByteValue *result) {
  aParameter *value;
  if (p == NULL) {
    return API_ERROR("getNamedByteValue passed null parameters pointer");
  }
  if (name == NULL) {
    return API_ERROR("getNamedByteValue passed null name pointer");
  }
  if (result == NULL) {
    return API_ERROR("getNamedByteValue passed null result pointer");
  }
  value = findValueForName(p, name);
  if (value == NULL) {
    sprintf(get_set_values_error_msg_buffer, "getNamedByteValue: Cannot find name: '%s'", tcgByteValueDebugStr(name));
    return ERROR_(get_set_values_error_msg_buffer);
  }
  if (value->parameterType != BYTE_TYPE) {
    sprintf(get_set_values_error_msg_buffer, "getNamedByteValue:  value for name '%s' is not a Byte Type", tcgByteValueDebugStr(name));
    return ERROR_(get_set_values_error_msg_buffer);
  }
  tcgByteValueCopy(result, &(value->value.byteValue));
  return SUCCESS;
}

status setIdByteValue(parameters *p, uint64_t id, tcgByteValue *value) {
  status s;
  if (p == NULL) {
    return API_ERROR("setIdByteValue: NULL paramaters pointer");
  }
  if (value == NULL) {
    return API_ERROR("setIdByteValue: NULL value pointer");
  }
  if (p->slotsUsed + 3 >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setIdByteValue: no room to add ID value");
  }
  p->parameterList[p->slotsUsed++].parameterType = START_NAME_TYPE;
  s = setIntValue(p, id);
  if (s != SUCCESS) { return s; }
  s = setByteValue(p, value);
  if (s != SUCCESS) { return s; }
  p->parameterList[p->slotsUsed++].parameterType = END_NAME_TYPE;
  return SUCCESS;
}

status setNamedByteValue(parameters *p, tcgByteValue *name, tcgByteValue *value) {
  status s;
  if (p == NULL) {
    return API_ERROR("setNamedByteValue: NULL paramaters pointer");
  }
  if (name == NULL) {
    return API_ERROR("setNamedByteValue: NULL name pointer");
  }
  if (value == NULL) {
    return API_ERROR("setNamedByteValue: NULL value pointer");
  }
  if (p->slotsUsed + 3 >= MAX_PARAMETER_LIST_SIZE) {
    return API_ERROR("setNamedByteValue: no room to add named value");
  }
  p->parameterList[p->slotsUsed++].parameterType = START_NAME_TYPE;
  s = setByteValue(p, name);
  if (s != SUCCESS) { return s; }
  s = setByteValue(p, value);
  if (s != SUCCESS) { return s; }
  p->parameterList[p->slotsUsed++].parameterType = END_NAME_TYPE;
  return SUCCESS;
}

void dumpParameters(parameters *p) {
  unsigned int i;
  printf("Parameters block for %p\n", (void*)p);
  if (p == NULL) {
    printf("-->NULL POINTER, cannot debug\n");
    return;
  }
  printf("Slots: %d\n", p->slotsUsed);
  for (i = 0; i < p->slotsUsed; i++) {
    unsigned int ut;
    ut = p->parameterList[i].parameterType;
    printf("%03d: (type %02d) - ", i, ut);
    switch (ut) {
    case UNDEF_TYPE: printf("Undefined - SHOULD NOT HAPPEN!\n"); break;
    case INT_TYPE: printf("%jd\n", (uintmax_t)(p->parameterList[i].value.longValue)); break;
    case BYTE_TYPE: printf("\"%s\"\n", tcgByteValueDebugStr(&(p->parameterList[i].value.byteValue))); break;
    case START_LIST_TYPE: printf("<Start List>\n"); break;
    case END_LIST_TYPE: printf("<End List>\n"); break;
    case START_NAME_TYPE: printf("<Start Name>\n"); break;
    case END_NAME_TYPE: printf("<End Name>\n"); break;
    default: printf("Unknown type - SHOULD NOT HAPPEN!\n"); break;
    }
  }
}
