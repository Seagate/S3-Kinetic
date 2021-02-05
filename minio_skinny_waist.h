#ifndef MINIO_SKINNY_H
#define MINIO_SKINNY_H
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

typedef struct _CPrimaryStoreValue{
    char* version;
    char* tag;
    char* value;
    int32_t algorithm;
} _CPrimaryStoreValue;

void CInitMain();

char* Get(int64_t user_id, char* key, struct _CPrimaryStoreValue* psvalue, char* value, uint32_t* size);

int Put(int64_t user_id, char* key, char* current_version, struct _CPrimaryStoreValue* psvalue, char* value, size_t size, _Bool sync, int64_t sequence, int64_t connID);

void NPut(CKVObject* obj, RequestContext& context);

#ifdef _cplusplus
}
#endif
#endif

