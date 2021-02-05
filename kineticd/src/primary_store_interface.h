#ifndef KINETIC_PRIMARY_STORE_INTERFACE_H_
#define KINETIC_PRIMARY_STORE_INTERFACE_H_

#include <string>

#include "kinetic/incoming_value.h"

#include "key_value_store_interface.h"
#include "outgoing_value.h"
#include "kinetic.pb.h"
#include "request_context.h"
#include "KVObject.h"

using namespace com::seagate::kinetic::proto; //NOLINT

/*
 * A wrapper around a KeyValueStoreInterface that stores an arbitrary version
 * string and tag string alongside each value
 */

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {
    class BatchSet;
}

using ::kinetic::IncomingValueInterface;
using com::seagate::kinetic::cmd::BatchSet;

struct PrimaryStoreValue {
    std::string version;
    std::string tag;
    std::string value;
    int32_t algorithm;
};

class PrimaryStoreIteratorInterface {
    public:
    /* PrimaryStoreIteratorInterface takes ownership of
     * KeyValueStoreIterator and assumes it was allocated on the heap. It frees
     * the iterator automatically in its destructor.
     */
    virtual ~PrimaryStoreIteratorInterface() {}

    virtual IteratorStatus Init() = 0;
    virtual IteratorStatus Next() = 0;
    virtual IteratorStatus Last() = 0;
    virtual IteratorStatus Prev() = 0;

    virtual std::string Key() = 0;
    virtual bool Version(std::string *version) = 0;
    virtual StoreOperationStatus Value(NullableOutgoingValue *value) = 0;
    virtual StoreOperationStatus MScanValue(NullableOutgoingValue *value) = 0;
    virtual bool Tag(std::string *tag) = 0;
    virtual bool Algorithm(int32_t *algorithm) = 0;
};

class PrimaryStoreInterface {
    public:
    virtual ~PrimaryStoreInterface() {}
    virtual bool InitUserDataStore(bool create_if_missing = false) = 0;
    virtual bool Close() = 0;
    virtual bool IsOpen() = 0;
    virtual void SetPreUsedBytes() = 0;
    virtual StoreOperationStatus Get(
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value, char* buff = NULL) = 0;
    virtual StoreOperationStatus Put(
        const std::string& key,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token) = 0;
    virtual StoreOperationStatus Delete(const std::string& key,
                                        bool guarantee_durable,
                                        const std::tuple<int64_t, int64_t> token) = 0;
    virtual StoreOperationStatus Clear(std::string pin, bool SecureRequested) = 0;
    virtual PrimaryStoreIteratorInterface* Find(const std::string& key) = 0;
    virtual bool SetRecordStatus(const std::string& key, bool bad = true) = 0;
    virtual leveldb::Status Flush(bool toSST = false, bool clearMems = false, bool toClose = false) = 0;
    virtual bool Write(BatchSet* batchSet, Command& commandResponse, const std::tuple<int64_t, int64_t> token,
        int64_t user_id, RequestContext& request_context) = 0;
    virtual StoreOperationStatus DoesKeyExist(const string& key) = 0;

    virtual StoreOperationStatus NPut(KVObject* obj, RequestContext& reqContext) = 0;

};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PRIMARY_STORE_INTERFACE_H_
