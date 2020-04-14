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


#include "status_codes.h"

static status v_lastTcgStatus;
static char *v_lastTcgStatusDescription;
static uint64_t v_lastTcgMethodStatus;

status _status_code(status value, char *extraMsg) {
  v_lastTcgStatus = value;
  v_lastTcgStatusDescription = extraMsg;
  return value;
}

status _method_status_code(uint64_t method_status, char *extraMsg) {
  v_lastTcgMethodStatus = method_status;
  return _status_code(_TCG_METHOD_ERROR, extraMsg);
}

char *lastTcgStatusDescription(void) {
  return v_lastTcgStatusDescription;
}

status lastTcgStatus(void) {
  return v_lastTcgStatus;
}

uint64_t lastTcgMethodStatus(void) {
  return v_lastTcgMethodStatus;
}

status setLastTcgStatus(status s) {
  v_lastTcgStatus = s;
  return s;
}
