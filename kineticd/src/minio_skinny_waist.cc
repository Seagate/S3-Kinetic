#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include "primary_store_interface.h"
#include "kinetic/incoming_value.h"
#include "kinetic/outgoing_value.h"
#include "request_context.h"
#include "skinny_waist.h"
#include <iostream>
#include "kernel_mem_mgr.h"
#include "mem/DynamicMemory.h"
#include "store_operation_status.h"

using namespace std; // NOLINT
using namespace com::seagate::kinetic; // NOLINT
using ::kinetic::IncomingValueInterface;
using ::kinetic::IncomingBuffValue;

extern "C" {
    extern bool kineticd_idle;
}

SkinnyWaist *pskinny_waist__ = NULL;

typedef struct _CPrimaryStoreValue {
    int metaSize;
    int32_t algorithm;
    char* version;
    char* tag;
    char* meta;
} _CPrimaryStoreValue;

char* s;
extern "C" {
char* allocate_pvalue_buffer(int n) {
    char* buff = NULL;
    buff = smr::DynamicMemory::getInstance()->allocate(n);
    return buff;
}

void deallocate_gvalue_buffer(char* buff) {
    //cout << "DEALLOC BUFF " << (void*)buff << endl;
    KernelMemMgr::pInstance_->FreeMem((void*)buff);
}

int Put(int64_t user_id, char* key, char* current_version, _CPrimaryStoreValue* psvalue, char* value,
        size_t size, _Bool sync, uint64_t sequence, int64_t connID) {
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Enter" << endl;
    kineticd_idle = false;
    std::tuple<uint64_t, int64_t> token {sequence, connID}; // NOLINT
    PrimaryStoreValue primaryStoreValue;
    primaryStoreValue.version = string(psvalue->version);
    primaryStoreValue.tag = string(psvalue->tag);
    primaryStoreValue.value = "";
    primaryStoreValue.algorithm = psvalue->algorithm;
    primaryStoreValue.meta = string(psvalue->meta, psvalue->metaSize);
    IncomingBuffValue ivalue(value, size);
    RequestContext requestContext;
    requestContext.is_ssl = false;
    StoreOperationStatus status = ::pskinny_waist__->Put(user_id, string(key), string(current_version), primaryStoreValue,
                                                         &ivalue, true, sync, requestContext, token);
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit" << endl;
    return status;
}

char* GetMeta(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int* size, int* st) {
    kineticd_idle = false;
    PrimaryStoreValue primaryStoreValue;
    primaryStoreValue.version = string(psvalue->version);
    primaryStoreValue.tag = string(psvalue->tag);
    primaryStoreValue.value = "";
    primaryStoreValue.algorithm = psvalue->algorithm;
    primaryStoreValue.meta = "";
    NullableOutgoingValue *ovalue = NULL;
    RequestContext requestContext;
    requestContext.is_ssl = false;
    StoreOperationStatus status = ::pskinny_waist__->Get(user_id, string(key), &primaryStoreValue, requestContext,  ovalue, bvalue);
    *st = int(status);
    *size = 0;
    if (status == StoreOperationStatus::StoreOperationStatus_SUCCESS) {
        *size = primaryStoreValue.meta.size();
        memcpy(bvalue, primaryStoreValue.meta.data(), *size);
    }
    return bvalue;
}

char* Get(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int* size, int* st) {
    kineticd_idle = false;

    PrimaryStoreValue primaryStoreValue;
    primaryStoreValue.version = string(psvalue->version);
    primaryStoreValue.tag = string(psvalue->tag);
    primaryStoreValue.value = "";
    primaryStoreValue.algorithm = psvalue->algorithm;
    primaryStoreValue.meta = "";
    NullableOutgoingValue *ovalue = new NullableOutgoingValue();
    RequestContext requestContext;
    requestContext.is_ssl = false;
    *size = 0;
    StoreOperationStatus status = ::pskinny_waist__->Get(user_id, string(key), &primaryStoreValue, requestContext,  ovalue, bvalue);
    s = NULL;
    *st = int(status);
    switch (status) {
        case StoreOperationStatus::StoreOperationStatus_SUCCESS:
            *size = int(ovalue->size());
            s = ovalue->get_value_buff();
            delete ovalue;
            return s;
        case StoreOperationStatus::StoreOperationStatus_NOT_FOUND:
            delete ovalue;
            return NULL;
        case StoreOperationStatus::StoreOperationStatus_STORE_CORRUPT:
            delete ovalue;
            return NULL;
        case StoreOperationStatus::StoreOperationStatus_DATA_CORRUPT:
            delete ovalue;
            return NULL;
        default:
            LOG(ERROR) << "IE store status";
            delete ovalue;
            return NULL;
    };
}


int Delete(int64_t user_id, char* key, char* current_version,  _Bool sync, uint64_t sequence, int64_t connID) {
    kineticd_idle = false;
    std::tuple<uint64_t, int64_t> token {sequence, connID}; // NOLINT
    RequestContext requestContext;
    requestContext.is_ssl = false;
    StoreOperationStatus status =  ::pskinny_waist__->Delete(user_id, string(key), string(current_version), true, sync, requestContext, token);
    return status;
}

void GetKeyRange(int64_t user_id, char* startKey, char* endKey, bool startKeyInclusive, bool endKeyInclusive, uint32_t maxReturned,
                 bool reverse, char* results, int* size) {
    kineticd_idle = false;
    RequestContext requestContext;
    requestContext.is_ssl = false;
    std::vector<std::string> keys;
    ::pskinny_waist__->GetKeyRange(user_id, startKey, startKeyInclusive, endKey, endKeyInclusive, maxReturned, reverse, &keys, requestContext);
    vector<string>::iterator it;  // declare an iterator to a vector of strings
    char* temp = results;
    int totalSize = 0;
    for ( it = keys.begin(); it != keys.end(); it++ ) {
        string key = *it;
        strcpy(temp, key.c_str());
        temp += key.size();
        *temp++ = ':';
        totalSize += key.size() + 1;
    }
    *size = totalSize;
}
}
