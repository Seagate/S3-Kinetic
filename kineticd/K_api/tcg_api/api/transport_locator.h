#ifndef TRANSPORT_LOCATOR_H
#define TRANSPORT_LOCATOR_H

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

#include "transport.h"

/** @file
 * Transport locator is used to build transport structures based on c-string designator.
 */

extern transport *transport_locator(char *transport_id);

#endif
