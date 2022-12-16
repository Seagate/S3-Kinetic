#ifndef KINETIC_C_OPERATIONS_H_
#define KINETIC_C_OPERATIONS_H_
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#ifndef __cplusplus
#define bool _Bool
#define true 1
#define false 0
#else
#define _Bool bool
#define bool bool
#define false false
#define true true
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CPrimaryStoreValue {
    int metaSize;
    int32_t algorithm;
    char* version;
    char* tag;
    char* meta;
} _CPrimaryStoreValue;


char* allocate_pvalue_buffer(int size);
void deallocate_pvalue_buffer(char* buff, int n);
void deallocate_gvalue_buffer(char* buff);


char* Get(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int* dataSize, int* status);
char* PartialGet(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int offset, int reqSize, int* dataSize, int* st);
char* GetMeta(int64_t user_id, char* key, struct _CPrimaryStoreValue* psvalue, int* size, int* status);

int Put(int64_t user_id, char* key, char* current_version, _CPrimaryStoreValue* psvalue, char* value, size_t size,
          _Bool sync, uint64_t sequence, int64_t connID);

int Delete(int64_t user_id, char* key, char* current_version,  _Bool sync, uint64_t sequence, int64_t connID);

void GetKeyRange(int64_t user_id, char* startKey, char* endKey, bool startKeyInclusive, bool endKeyInclusive, uint32_t maxReturned,
                 bool reverse, char* results, int* size);
int Flush();

#ifdef __cplusplus
}
#endif
#endif  // KINETIC_C_OPERATIONS_H_

