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

#include <string.h>
#include "high_level.h"
#include "transport_locator.h"
#include "transport_fake.h"


transport *CA_transport_locator(char *transport_name) {
  if (transport_name == NULL) {
    return NULL;
  }
  if (strcmp(transport_name, "fake") == 0) {
    return fake();
  }
  return NULL;
}
