#ifndef PARAMETERS_H
#define PARAMETERS_H

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

/*! @file
 * Parameters are used for the values/types needed
 * to create TCG method call parameter lists and
 * to parse the results of method calls. This module
 * also includes helper/convenience functions.
 * NOTE: The high_level(.h) interface hides this,
 * but to use low_level(.h) this information is needed.
 *
 * There are five types of parameters/results that
 * are implemented here:
 *    two simple types: Integer and Bytes (strings);
 *    three structured types: Lists, Name/Value Pairs, and
 *        ID/Value Pairs.
 * To simplify memory management and
 * error handling, the structure holding the parameter list/
 * results list is fixed size with a maximum size on byte
 * string values. Those limits can be changed, but are big
 * enough to accomodate the use cases expected of this code.
 *
 */

#include <stdint.h>
#include "status_codes.h"
#include "transport.h"

/** The maximum number of parameters we buffer
 *  to/from a TCG method call encoding.
 *  Using fixed size buffers eliminates memory
 *  management issues but does have some limitation.
 */
#define MAX_PARAMETER_LIST_SIZE    150

/** The different kinds of parameter types
 *  that we represent. Note that the integer/byte
 *  value encoding/decoding size is not represented because
 *  we are using fixed size values/buffers in this code.
 */
typedef enum {UNDEF_TYPE,
              INT_TYPE,
              BYTE_TYPE,
              START_LIST_TYPE,
              END_LIST_TYPE,
              START_NAME_TYPE,
              END_NAME_TYPE
              } parameterType_t;

#define TCG_BYTE_VALUE_MAX_SIZE 1200

typedef struct {
  unsigned int len;
  unsigned char data[TCG_BYTE_VALUE_MAX_SIZE];
} tcgByteValue;

/** For encoding a parameter or parameter structure.
 *  Only supporting two value types on purpose,
 *  this can be expanded if needed.
 */
typedef struct {
  parameterType_t parameterType;
  union {
    uint64_t longValue;
    tcgByteValue byteValue;
  } value;
} aParameter;

/** a (fixed-size) parameter list holder.
 */
typedef struct {
  /** Keep track of how many slots have already been used. */
  unsigned int slotsUsed;
  /** Keep track of how many slots ahve already been read. */
  unsigned int slotsRead;
  /** List of parameter values and/or structure. */
  aParameter parameterList[MAX_PARAMETER_LIST_SIZE];
} parameters;

/** For Debugging ONLY!
 *  @param[in] v - the TCG Byte Value to convert.
 *  @return a null-terminate C-string.
 *  @return if v is NULL, NULL is returned.
 *  @return the value returned is a pointer to static memory
 *  and it should be copied before other functions are called.
 */
char *tcgByteValueDebugStr(tcgByteValue *v);

/** For Debugging ONLY!  will convert all characters as a Hex value in parenthesis
 *  @param[in] v - the TCG Byte Value to convert.
 *  @return a null-terminate C-string.
 *  @return if v is NULL, NULL is returned.
 *  @return the value returned is a pointer to static memory
 *  and it should be copied before other functions are called.
 */
char *tcgByteValueDebugAllHexStr(tcgByteValue *v);

/** For transient conversion of a native C string
 *  to a TCG Name (Byte value).
 *  @param[in] c_string pointer to a C string to convert (up until first null byte).
 *  @return pointer to fixed/static storage, so caller must be aware of
 *  how they use/capture the value.
 */
tcgByteValue *tcgTmpName(char *c_string);

/** For transient conversion of a native C string
 *  to a TCG Name (Byte value).
 *  @param[in] c_string pointer to C string to convert.
 *  @param[in] count how many bytes to convert.
 *  @return pointer to fixed/static storage, so caller must be aware of
 *  how they use/capture the value.
 */
tcgByteValue *tcgTmpNameCount(char *c_string, unsigned int count);

/** Copy a null-terminated C-string to the given Byte value
 *  @param[out] dest where to copy the C-string to.
 *  @param[in] src the null-terminate C-string to copy.
 */
void tcgByteValueFromString(tcgByteValue *dest, char *src);

/** Copy a counted C-string to the given Byte value
 *  @param[out] dest where to copy the C-string to.
 *  @param[in] src where to copy the C-string from.
 *  @param[in] count how many characters to copy.
 */
void tcgByteValueFromStringCount(tcgByteValue *dest, char *src, unsigned int count);

/** Copy the given Byte value into the given C-string, with NULL termination.
 *  NOTE: Byte values can contain NULLs, so there is no guarantee that
 *        strlen on the C-string will match the length of the byte value.
 *  NOTE: The caller must ensure that dest points to memory big enough to hold
 *        the value being copied plus the null termination.
 *  @param[out] dest destination of copy.
 *  @param[in] src source of copy.
 */
void stringFromTcgByteValue(char *dest, tcgByteValue *src);

/** Compare two TCG Byte values.
 *  @param[in] a - first value to compare.
 *  @param[in] b - second value to compare.
 *  @return non-Zero if a and b refer to equal byte values.
 *  @return (NULL pointers are not equal.)
 */
int tcgByteValueEqual(tcgByteValue *a, tcgByteValue *b);

/** Copy one Byte value to another. Unused bytes in the destination
 *  may or may not be zero'd.
 */
void tcgByteValueCopy(tcgByteValue *dest, tcgByteValue *src);

/** Get an integer value
 *  @param p - the parameters to read from.
 *  @param[out] result - where to store the result on SUCCESS
 *  @return error if there is a problem (p is NULL, the next value is not an integer, etc.)
 */
status getIntValue(parameters *p, uint64_t *result);

/** Get an integer value by ID. For Opal SSC or Opal SSC v2.00 only.
 *  If there is more than one ID/value pair with the same ID,
 *  only the first one is considered.
 *  @param[in] p - parameters to search.
 *  @param[in] id - TCG Int Value ID to search for.
 *  @param[out] result - where to store the result on SUCCESS.
 *  @return error if the ID is not associated with an integer value or cannot be found.
 */
status getIdIntValue(parameters *p, uint64_t id, uint64_t *result);

/** Get a named integer value. For Enterprise SSC only.
 *  If there is more than one name/value pair with the same name,
 *  only the first one is considered.
 *  @param[in] p - parameters to search.
 *  @param[in] name - TCG Int Value name to search for.
 *  @param[out] result - where to store the result on SUCCESS.
 *  @return error if the name is not associated with an integer value or cannot be found.
 */
status getNamedIntValue(parameters *p, tcgByteValue *name, uint64_t *result);

/** Get a byte value by ID (a.k.a. column number, for Core Spec 2)
 *  For Opal SSC or Opal SSC v2.00 only.
 *  If there is more than one ID/value pair with the same ID,
 *  only the first one is considered.
 *  @param[in] p - parameters to search.
 *  @param[in] id - column number to search for.
 *  @param[out] result - where to store the result on SUCCESS.
 *  @return error if the ID is not associated with an integer value or cannot be found.
 */
status getIdByteValue(parameters *p, uint64_t id, tcgByteValue *result);

/** Get a named byte value. For Enterprise SSC only. 
 *  If there is more than one name/value pair with the same name,
 *  only the first one is considered.
 *  @param[in] p - parameters to search.
 *  @param[in] name - TCG Byte Value name to search for.
 *  @param[out] result - where to store the result on SUCCESS.
 *  @return error if the name is not associated with a byte value or cannot be found.
 */
status getNamedByteValue(parameters *p, tcgByteValue *name, tcgByteValue *result);

/** Set an integer value
 *  @param p - the parameters to use.
 *  @param value - the integer value to set.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setIntValue(parameters *p, uint64_t value);

/** Set an integer value by ID. For Opal SSC or Opal SSC v2.00 only. 
 *  @param p - the parameters to use.
 *  @param id - the ID part of the ID/value pair.
 *  @param value - the integer value part of the ID/value pair.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setIdIntValue(parameters *p, uint64_t id, uint64_t value);

/** Set a named integer value. For Enterprise SSC only. 
 *  @param p - the parameters to use.
 *  @param name - the name part of the name/value pair.
 *  @param value - the integer value part of the name/value pair.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setNamedIntValue(parameters *p, tcgByteValue *name, uint64_t value);

/** Get the next byte value
 *  @param p parameters to use.
 *  @param[out] result - where to store the result on SUCCESS.
 *  @result is SUCCESS if everyting ok (p is not NULL, etc.)
 */
status getByteValue(parameters *p, tcgByteValue *result);

/** Set a byte value
 *  @param p - the parameters to use.
 *  @param value - the byte value to set.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setByteValue(parameters *p, tcgByteValue *value);

/** Set the byte value by ID. For Opal SSC or Opal SSC v2.00 only. 
 *  @param p - the parameters to use.
 *  @param id - the ID part of the ID/value pair.
 *  @param value - the integer value part of the name/value pair.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setIdByteValue(parameters *p, uint64_t id, tcgByteValue *value);

/** Set the named byte value. For Enterprise SSC only. 
 *  @param p - the parameters to use.
 *  @param name - the name part of the name/value pair.
 *  @param value - the integer value part of the name/value pair.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setNamedByteValue(parameters *p, tcgByteValue *name, tcgByteValue *value);

/** Consume a start-list indicator
 *  @param p - the parameters to use.
 *  @return SUCCESS if a start list token was next, p is not NULL, etc.
 */
status getStartList(parameters *p);

/** Set a start-list indicator
 *  @param p - parameters to use.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setStartList(parameters *p);

/** Consume an end-list indicator
 *  @param p - the parameters to use.
 *  @return SUCCESS if a end list token was next, p is not NULL, etc.
 */
status getEndList(parameters *p);

/** Check if an end-list indicator is ready to be gotten.
 *  @param p - the parameters to use.
 *  @return non-0 if getEndList would be successful, 0 otherwise.
 */
int peekEndList(parameters *p);

/** Set an end-list indicator
 *  @param p - parameters to use.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setEndList(parameters *p);

/** Set a start-name indicator
 *  @param p - parameters to use.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setStartName(parameters *p);

/** Set an end-name indicator
 *  @param p - parameters to use.
 *  @return SUCCESS if everything OK (p is not NULL, there is still room, etc.).
 */
status setEndName(parameters *p);

/** Reset the parameters to empty.
 *  @param p - parameters to use.
 */
void resetParameters(parameters *p);

/** how many parameters have been set?
 *  @param[in] p - parameters to check.
 *  @return the number of parameter slots in use.
 */
int countParameters(parameters *p);

/** find the first occurance of the name as an ID/value pair
 *  in the given parameters. For Opal SSC or Opal SSC v2.00 only. 
 *  @param[in] p - parameters to check.
 *  @param[in] id - the ID to look for.
 *  @return a pointer to the value associated with the ID,
 *  or NULL if the ID is not found.
 */
aParameter *findValueForId(parameters *p, uint64_t id);

/** find the first occurance of the name as a name/value pair
 *  in the given parameters. For Enterprise SSC only. 
 *  @param[in] p - parameters to check.
 *  @param[in] name - the name to look for.
 *  @return a pointer to the value associated with the name,
 *  or NULL if the name is not found.
 */
aParameter *findValueForName(parameters *p, tcgByteValue *name);

/** set the given parameters structure to read from the value of the given ID.
 *  For Opal SSC or Opal SSC v2.00 only. 
 *  @param[in] p - parameters to check.
 *  @param[in] id - the ID to look for.
 *  @return SUCCESS if the ID is found and the parameters structure is set to read its value,
 *  or an ERROR_ if the ID is not found.
 */
status setSlotForId(parameters *p, uint64_t id);

/** set the given parameters structure to read from the value of the given name.
 *  For Enterprise SSC only. 
 *  @param[in] p - parameters to check.
 *  @param[in] name - the name to look for.
 *  @return SUCCESS if the name is found and the parameters structure is set to read its value,
 *  or an ERROR_ if the name is not found.
 */
status setSlotForName(parameters *p, tcgByteValue *name);

/** print out the contents of parameters (debugging)
 *  @param[in] p - parameters to use.
 */
void dumpParameters(parameters *p);

/** Utility for copying tcgByteValues
 *  @param[out] dest - destination of copy.
 *  @param[in] src - source of copy.
 *  if either src or dest are NULL, returns without copying anything.
 */
void tcgByteValueCopy(tcgByteValue *dest, tcgByteValue *src);

#endif
