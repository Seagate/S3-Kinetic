#ifndef DEBUG_H
#define DEBUG_H

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
 * @file debug.h
 *
 * Functions and macros to control and implement debugging.
 * 
 */

/** 
 * If amount = 0, no debugging is printed.
 * If amount < 0, every function's debugging output is printed.
 * If amount > 0, then 'amount' low-level calls will have
 *                     debugging output printed and then debugging output will
 *                     be turned off (i.e. each low_level.h function called will decrement it).
 *
 * @param amount Amount of debugging required
 */
extern void setDebug(int amount);

#define DEFAULT_DEBUG_LEVEL 0

extern int debugLevel;

/* Must come after last variable declaration and before first code.
 * Uses local value as a cache because functions may need to emit
 * multiple debugging statements and we don't want each debugging
 * statement to decrement the counter.
 */
#define CHECK_DEBUG  int doDebug = 0; { \
  if (debugLevel > 0) { \
    debugLevel--; \
    doDebug = 1; \
  } else if (debugLevel < 0) { \
    doDebug = 1; \
  } \
}

#define DEBUG(args) {if (doDebug) { printf args; }}
#define DEBUG_S(arg) {if (doDebug) { arg; }}

#endif
