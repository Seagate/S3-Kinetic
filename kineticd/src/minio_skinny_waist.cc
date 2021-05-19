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
//#include "KVObject.h"

using namespace std; // NOLINT
using namespace com::seagate::kinetic; // NOLINT
using ::kinetic::IncomingValueInterface;
using ::kinetic::IncomingBuffValue;

extern "C" {
    extern bool kineticd_idle;
}

SkinnyWaist *pskinny_waist__ = NULL;
/*
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
*/
typedef struct _CPrimaryStoreValue {
    char* version;
    char* tag;
    int32_t algorithm;
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

int Put(int64_t user_id, char* key, char* current_version, struct _CPrimaryStoreValue* psvalue, char* value,
        size_t size, _Bool sync, uint64_t sequence, int64_t connID) {
    kineticd_idle = false;
    std::tuple<uint64_t, int64_t> token {sequence, connID}; // NOLINT
    PrimaryStoreValue primaryStoreValue;
    primaryStoreValue.version = string(psvalue->version);
    primaryStoreValue.tag = string(psvalue->tag);
    primaryStoreValue.value = "";
    primaryStoreValue.algorithm = psvalue->algorithm;
    primaryStoreValue.meta = psvalue->meta;
    IncomingBuffValue ivalue(value, size);
    RequestContext requestContext;
    requestContext.is_ssl = false;
    StoreOperationStatus status = ::pskinny_waist__->Put(user_id, string(key), string(current_version), primaryStoreValue,
                                                         &ivalue, true, sync, requestContext, token);
    return status;
}

char* GetMeta(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int* size, int* st) {
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Enter" << endl;
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
    *st = int(status); //int(-1);
    *size = 0; //primaryStoreValue.meta.size();
/*
    if (*size > 0 && status == StoreOperationStatus::StoreOperationStatus_SUCCESS) {
        memcpy(bvalue, primaryStoreValue.meta.data(), primaryStoreValue.meta.size() + 1);
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Meta = " << bvalue << "***, Meta string len = " << *size << endl;
    } else {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": No meta" << endl;
    }
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit" << endl;
    return bvalue;
*/
//    char* metaBuf = NULL; 
    if (status == StoreOperationStatus::StoreOperationStatus_SUCCESS) {
        *size = primaryStoreValue.meta.size();
        memcpy(bvalue, primaryStoreValue.meta.data(), *size);
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Meta size = " << *size << ", status = " << *st << endl;
//        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Meta = " << bvalue << "***, Meta string len = " << *size << endl;
/*
        if (*size > 0) {
            //int s = posix_memalign((void**)&metaBuf, 4096, ROUNDUP(1048576, 4096)); //(size_t)(*size),4096));
            metaBuf = (char*)calloc(*size + 1, sizeof(char));

            //if (s == 0) { //metaBuf) {
            if (metaBuf) {
                memcpy(metaBuf, primaryStoreValue.meta.data(), primaryStoreValue.meta.size()+1);
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Meta size = " << *size << ", status = " << *st << endl;
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Meta = " << metaBuf << "***, Meta string len = " << *size << endl;
            } else {
                *st = StoreOperationStatus::StoreOperationStatus_INTERNAL_ERROR;
                *size = 0;
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to allocate mem, size = " << *size << endl;
            }
        } else {
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": No meta" << endl;
        }
*/
    } else {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to Get, status = " << status << endl;
    }
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit, status = " << *st << ", metaBuf addr = " << (void*)bvalue << endl;
    return bvalue; //metaBuf; //bvalue;
}

char* Get(int64_t user_id, char* key, char* bvalue, struct _CPrimaryStoreValue* psvalue, int* size, int* st) {
    cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit" << endl;
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
    *st = int(status); //int(-1);
    switch (status) {
        case StoreOperationStatus::StoreOperationStatus_SUCCESS:
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": success" << endl;
            *size = int(ovalue->size());
            s = ovalue->get_value_buff();
            delete ovalue;
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit success" << endl;
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
    char* temp = results; //(char*)malloc(4096 * sizeof(char));
    //cout << " ADDRESS " << (void*)results << " " << (void*)temp << endl;
    int totalSize = 0;
    for ( it = keys.begin(); it != keys.end(); it++ ) {
        string key = *it;
        strcpy(temp, key.c_str());
        temp += key.size();
        *temp++ = ':';
        totalSize += key.size() + 1;
    }
    //cout << " RESULTS " << string(results) << endl;
    *size = totalSize;
}

//==========
// Operations with new signatures
//==========

int NPut(CKVObject* C_kvObj, CRequestContext* C_reqCtx) { //int64_t userId) {
cout << "NPut: Enter" << endl;
cout << "version = " << string(C_kvObj->version_) << endl;
cout << "usrId = " << C_reqCtx->userId_ << ", writeThru = " << C_reqCtx->writeThrough_ << endl;
    KVObject kvObj(C_kvObj);
cout << "Client meta: " << kvObj.clientMeta() << "*****" << endl;
    RequestContext reqContext;
    reqContext.setUserId(C_reqCtx->userId_);
    reqContext.setSsl(C_reqCtx->ssl_ == 1); //false);
    reqContext.setWriteThrough(C_reqCtx->writeThrough_ == 1); //false);
    reqContext.setIgnoreVersion(C_reqCtx->ignoreVersion_ == 1); //false);
    reqContext.setSeq(C_reqCtx->seq_);
    reqContext.setConnId(C_reqCtx->connId_);
cout << "Calling skinny waist.NPut()" << endl;
    StoreOperationStatus status = ::pskinny_waist__->NPut(&kvObj, reqContext);
    return status;
}
 
}
