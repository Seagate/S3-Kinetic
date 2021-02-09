#ifndef MINIO_SKINNY_H
#define MINIO_SKINNY_H
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
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


#ifdef _cplusplus
extern "C" {
#endif

typedef struct CKVObject {
	char* key_;
	char* value_;

	// Meta data
	int keySize_;
	int valueSize_;
	char* version_;
	char* tag_;
	int32_t algorithm_;
} CKVObject;

typedef struct _CPrimaryStoreValue {
    char* version;
    char* tag;
    char* value;
    int32_t algorithm;
} _CPrimaryStoreValue;

//void* CInitMain(char* store_partition);

void CInitMain(int argc, char* argv[]);

static void* allocArgv(int argc) {
    return malloc(sizeof(char *) * argc);
}

static void printArgs(int argc, char** argv) {
    int i;
    for (i = 0; i < argc; i++) {
        printf("%s\n", argv[i]);
    }
}

char* allocate_pvalue_buffer(int size);
void deallocate_gvalue_buffer(char* buff);


char* Get(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int* size, int* status);

int Put(int64_t user_id, char* key, char* current_version, struct _CPrimaryStoreValue* psvalue, char* value, size_t size,
          _Bool sync, int64_t sequence, int64_t connID);

int Delete(int64_t user_id, char* key, char* current_version,  _Bool sync, int64_t sequence, int64_t connID);

void GetKeyRange(int64_t user_id, char* startKey, char* endKey, bool startKeyInclusive, bool endKeyInclusive, uint32_t maxReturned, bool reverse, char* results, int* size);

#ifdef _cplusplus
}
#endif
#endif

