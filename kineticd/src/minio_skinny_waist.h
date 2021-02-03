#ifndef KINETIC_MINIO_SKINNY_WAIST_H_
#define KINETIC_MINIO_SKINNY_WAIST_H_
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include "kernel_mem_mgr.h"

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


#ifdef _cplusplus
extern "C" {
#endif

typedef struct _CPrimaryStoreValue {
    char* version;
    char* tag;
    //char* value;
    int32_t algorithm;
} _CPrimaryStoreValue;

//char* allocate_pvalue_buffer(int size);

//void CInitMain();
void CInitMain(char* store_partition);

char* Get(int64_t user_id, char* key, struct _CPrimaryStoreValue* psvalue, char*** buff, uint32_t* size, uint32_t* status);

int Put(int64_t user_id, char* key, char* current_version, struct _CPrimaryStoreValue* psvalue, char* value, size_t size,
         _Bool sync, int64_t sequence, int64_t connID);

int Delete(int64_t user_id, char* key, char* current_version,  _Bool sync, int64_t sequence, int64_t connID);

void GetKeyRange(int64_t user_id, char* startKey, char* endKey, bool startKeyInclusive, bool endKeyInclusive, uint32 maxReturned,
                 bool reverse, char* results, int* size);


#ifdef _cplusplus
}
#endif
#endif  // KINETIC_MINIO_SKINNY_WAIST_H_

