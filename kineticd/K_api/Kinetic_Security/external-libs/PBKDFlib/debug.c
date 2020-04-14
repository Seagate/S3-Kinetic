
/**
 * tempo file to satisfy M_Memcpy for linker
 *
 */
#include <string.h>
#include <stdio.h>



void M_Memcpy(void *Dest, const void *Source, size_t Size)
 {
  memcpy(Dest, Source, Size);
 }

void M_DebugXmitString(const char *string)
 {
	printf("%s", string);
 }
