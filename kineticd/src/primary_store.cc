#include "primary_store.h"
#include <string>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <iostream>
#include <fstream>
#include <set>

#include "glog/logging.h"
#include "profiler.h"
#include "popen_wrapper.h"

#include "leveldb/mydata.h"
#include "leveldb/status.h"
#include "mem/DynamicMemory.h"
#include "smrdisk/DriveEnv.h"
#include "BatchSet.h"
#include "connection_handler.h"
#include "server.h"
#include "smrdisk/ValueFileCache.h"
#include "smrdisk/Disk.h"

using namespace com::seagate::kinetic::cmd; //NOLINT
namespace com {
namespace seagate {
namespace kinetic {
using ::kinetic::OutgoingBuffValue;

using ::kinetic::OutgoingStringValue;
using proto::InternalValueRecord;


const uint64_t PrimaryStore::kMinFreeSpace = smr::Disk::NO_SPACE_THRESHOLD;

namespace {
char* allocate_getvalue_buffer() {
    char* buff = NULL;
    buff = (char*) KernelMemMgr::pInstance_->AllocMem();
    return buff;
}

void deallocate_getvalue_buffer(char* buff) {
    KernelMemMgr::pInstance_->FreeMem((void*) buff);
}
} // namespace

PrimaryStoreIterator::PrimaryStoreIterator(bool corrupt,
    FileSystemStoreInterface& file_system_store,
    KeyValueStoreIteratorInterface* it,
    const std::string& key_value_store_name)
    : corrupt_(corrupt), file_system_store_(file_system_store), it_(it),
      key_value_store_name_(key_value_store_name) {}

PrimaryStoreIterator::~PrimaryStoreIterator() {
    delete it_;
}

IteratorStatus PrimaryStoreIterator::Init() {
    if (corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    return it_->Init();
}

IteratorStatus PrimaryStoreIterator::Last() {
    if (corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    return it_->Last();
}

IteratorStatus PrimaryStoreIterator::Next() {
    if (corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    return it_->Next();
}

IteratorStatus PrimaryStoreIterator::Prev() {
    if (corrupt_) {
        return IteratorStatus_STORE_CORRUPT;
    }

    return it_->Prev();
}

std::string PrimaryStoreIterator::Key() {
    return it_->Key();
}

bool PrimaryStoreIterator::getInternalValueRecord(proto::InternalValueRecord& record, const char* caller) {
    LevelDBData myData;
    const LevelDBData* pData = (LevelDBData*) it_->Value();

    if (isSerialized(pData->type)) {
        myData.deserialize(it_->Value());
        pData = &myData;
    }
    if (!record.ParseFromArray(pData->header, pData->headerSize)) {
        LOG(ERROR) << caller << ": IE Store Status";
        return false;
    }
    return true;
}

bool PrimaryStoreIterator::Version(std::string *version) {
    InternalValueRecord internal_value_record;
    if (getInternalValueRecord(internal_value_record, "VERSION")) {
      *version = internal_value_record.version();
      return true;
    }
    return false;
}

bool PrimaryStoreIterator::Tag(std::string *tag) {
    InternalValueRecord internal_value_record;
    if (getInternalValueRecord(internal_value_record, "TAG")) {
       *tag = internal_value_record.tag();
        return true;
    }
    return false;
}

bool PrimaryStoreIterator::Algorithm(int32_t *algorithm) {
    InternalValueRecord internal_value_record;
    if (getInternalValueRecord(internal_value_record, "ALGORITHM")) {
        *algorithm = internal_value_record.algorithm();
        return true;
    }
    return false;
}

StoreOperationStatus PrimaryStoreIterator::Value(NullableOutgoingValue *value) {
    InternalValueRecord internal_value_record;
    if (!getInternalValueRecord(internal_value_record, "VALUE")) {
        return StoreOperationStatus_INTERNAL_ERROR;
    }
    // Check the internal_value_record if the key has not been flagged
    if (internal_value_record.has_baddata()) {
        LOG(ERROR) << "Key has bad data";
        return StoreOperationStatus_DATA_CORRUPT;
    }

    if (internal_value_record.has_value()) {
        LevelDBData myData;
        const LevelDBData* pData = (LevelDBData*)it_->Value();

        if (isSerialized(pData->type)) {
            myData.deserialize(it_->Value());
            pData = &myData;
        }
        if (pData->type == LevelDBDataType::MEM_INTERNAL) {
             // NULL check as metadataOnly=True passes NULL for value in GetNext and GetPrevios (as in case of GET)
            if (value != NULL) value->set_value(new OutgoingStringValue(std::string(pData->data, pData->dataSize)));
            return StoreOperationStatus_SUCCESS;
        }
        if (pData->type == LevelDBDataType::MEM_EXTERNAL) {
            char* buff = allocate_getvalue_buffer();
            if (buff) {
                ExternalValueInfo external;
                if (external.deserialize(pData->data)) {
                    std::shared_ptr<RandomAccessFile> file;
                    leveldb::Status s = smr::CacheManager::cache(key_value_store_name_)->getReadable(external.file_number, file);
                    if (s.ok()) {
                        Slice not_needed;
                        s = file->Read(external.offset, external.size, &not_needed, buff);
                        if (s.ok()) {
                            if (value != NULL) value->set_value(new OutgoingBuffValue(buff, buff, external.size));
                            return StoreOperationStatus_SUCCESS;
                        }
                    }
                }
                deallocate_getvalue_buffer(buff);
            }
        }
    }
    return StoreOperationStatus_INTERNAL_ERROR;
}

////////////////////////////////////////////////////////
//MediaScan Centric: Access File Store Values every iteration
//Author: James DeVore
//-----------------------
//Hooks up to File Store MediaScan methods.
//Please see MScanGet() in file_system_store.cc
StoreOperationStatus PrimaryStoreIterator::MScanValue(NullableOutgoingValue *value) {
    InternalValueRecord internal_value_record;
    if (!getInternalValueRecord(internal_value_record, "MSSCAN")) {
        return StoreOperationStatus_INTERNAL_ERROR;
    }
    // Check the internal_value_record if the key has not been flagged
    if (internal_value_record.has_baddata()) {
        LOG(ERROR) << "Key has bad data";
        return StoreOperationStatus_DATA_CORRUPT;
    }
    LevelDBData myData;
    const LevelDBData* pData = (LevelDBData*)it_->Value();

    if (isSerialized(pData->type)) {
        myData.deserialize(it_->Value());
        pData = &myData;
    }
    if (pData->type == LevelDBDataType::MEM_INTERNAL) {
        value->set_value(new OutgoingStringValue(std::string(pData->data, pData->dataSize)));
        return StoreOperationStatus_SUCCESS;
    }
    if (pData->type == LevelDBDataType::MEM_EXTERNAL) {
        char* buff = allocate_getvalue_buffer();
        if (buff) {
            ExternalValueInfo external;
            if (external.deserialize(pData->data)) {
                std::shared_ptr<RandomAccessFile> file;
                leveldb::Status s = smr::CacheManager::cache(key_value_store_name_)->getReadable(external.file_number, file);
                if (s.ok()) {
                    Slice not_needed;
                    s = file->Read(external.offset, external.size, &not_needed, buff);
                    if (s.ok()) {
                        value->set_value(new OutgoingBuffValue(buff, buff, external.size));
                        return StoreOperationStatus_SUCCESS;
                    }
                }
            }
            deallocate_getvalue_buffer(buff);
        }
    }
    return StoreOperationStatus_INTERNAL_ERROR;
}

PrimaryStore::PrimaryStore(FileSystemStoreInterface &file_system_store,
        KeyValueStoreInterface& key_value_store,
        ClusterVersionStoreInterface& cluster_version_store,
        DeviceInformationInterface& device_information,
        Profiler& profiler,
        size_t file_system_store_minimum_size,
        InstantSecureEraserInterface& instant_secure_eraser,
        const std::string &preused_file_path)
    : file_system_store_(file_system_store),
    key_value_store_(key_value_store),
    cluster_version_store_(cluster_version_store),
    device_information_(device_information),
    profiler_(profiler),
    file_system_store_minimum_size_(file_system_store_minimum_size),
    instant_secure_eraser_(instant_secure_eraser),
    preused_file_path_(preused_file_path),
    corrupt_(false) {}

bool PrimaryStore::InitUserDataStore(bool create_if_missing) {
    if (!key_value_store_.Init(create_if_missing)) {
            LOG(ERROR) << "Unable to initialize primary store";
            return false;
        } else {
            VLOG(1) << "Initialized primary store";
        }

    return true;
}

StoreOperationStatus PrimaryStore::Get(
        const std::string& key,
        PrimaryStoreValue* primary_store_value,
        NullableOutgoingValue *value, char* buff) {
    Event e;
    profiler_.BeginAutoScoped(kPrimaryStoreGet, &e);
    char* packed_value;
    packed_value =  new char[sizeof(packed_value)];

    if (corrupt_) {
        delete[] packed_value;
        return StoreOperationStatus_STORE_CORRUPT;
    }
    bool ignore_value = false;
    if (value == NULL) {
        ignore_value = true;
    }
    switch (key_value_store_.Get(key, packed_value, ignore_value, true, buff)) {
        case StoreOperationStatus_SUCCESS:
            break;
        case StoreOperationStatus_NOT_FOUND:
            delete[] packed_value;
            return StoreOperationStatus_NOT_FOUND;
        case StoreOperationStatus_STORE_CORRUPT:
            delete[] packed_value;
            corrupt_ = true;
            return StoreOperationStatus_STORE_CORRUPT;
        default:
            delete[] packed_value;
            LOG(ERROR) << "IE Store Status";
            return StoreOperationStatus_INTERNAL_ERROR;
    }
    //Sequence:count:kType:key:Options[Version, Tag, Algo]:Value
    CHECK(primary_store_value);

    char* value_pointer;
    memcpy((char*)&value_pointer, packed_value, sizeof(void*));

    LevelDBData myData;
    if (!myData.deserialize(value_pointer)) {
      LOG(ERROR) << "Corrupt LevelDBData structure";
      deallocate_getvalue_buffer(value_pointer);
      delete[] packed_value;
      return StoreOperationStatus_DATA_CORRUPT;
    }

    InternalValueRecord internal_value_record;
    if (!internal_value_record.ParseFromArray(myData.header, myData.headerSize)) {
        LOG(ERROR) << "IE Store Status";
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__
           << ": failed to parse header, headersize = " << myData.headerSize << endl;
        leveldb::Status::IOError(ss.str());
        deallocate_getvalue_buffer(value_pointer);
        delete[] packed_value;
        return StoreOperationStatus_INTERNAL_ERROR;
    }

    // Check the internal_value_record if the key has not been flagged
    if (internal_value_record.has_baddata()) {
        LOG(ERROR) << "Key has bad data";
        deallocate_getvalue_buffer(value_pointer);
        delete[] packed_value;
        return StoreOperationStatus_DATA_CORRUPT;
    }
    primary_store_value->version = internal_value_record.version();
    primary_store_value->tag = internal_value_record.tag();
    primary_store_value->algorithm = internal_value_record.algorithm();

    if (value != NULL) {
        if (myData.type != LevelDBDataType::MEM_INTERNAL) {
            LOG(ERROR) << "Failed reading external value: " << myData;
            deallocate_getvalue_buffer(value_pointer);
            delete[] packed_value;
            return StoreOperationStatus_INTERNAL_ERROR;
        }
        value->set_value(new OutgoingBuffValue(value_pointer, myData.data, myData.dataSize));
    } else {
        deallocate_getvalue_buffer(value_pointer);
    }
    delete[] packed_value;
    return StoreOperationStatus_SUCCESS;
}
bool PrimaryStore::HasDiskSpace(BatchSet* batchSet, Command& commandResponse) {
    uint64_t total_bytes, used_bytes;
    commandResponse.mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    if (!device_information_.GetCapacity(&total_bytes, &used_bytes)) {
        commandResponse.mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
        commandResponse.mutable_status()->set_statusmessage("Failed to get capacity");
        return false;;
    }
    if (batchSet->hasPutCmd() && (smr::Disk::isNoSpace())) {
        commandResponse.mutable_status()->set_code(Command_Status_StatusCode_NO_SPACE);
        commandResponse.mutable_status()->set_statusmessage("There is no space");
    } else if (batchSet->hasDelCmd() && !key_value_store_.HasSpaceForDelCommand()) {
        commandResponse.mutable_status()->set_code(Command_Status_StatusCode_NO_SPACE);
        commandResponse.mutable_status()->set_statusmessage("Drive is full.  DELETE command is temporarily not accepted.");
    } else if (total_bytes - used_bytes < kMinFreeSpace) {
        smr::Disk::noSpace(smr::Disk::DiskStatus::NO_SPACE);
        cout << "SET NO NOSPACE" << endl;
    }
    return (commandResponse.status().code() == Command_Status_StatusCode_SUCCESS);
}
bool PrimaryStore::Write(BatchSet* batchSet, Command& commandResponse, const std::tuple<int64_t, int64_t> token,
    int64_t user_id, RequestContext& request_context) {
    if (!HasDiskSpace(batchSet, commandResponse)) {
//        cout << "NO SPACE CANNOT WRITE" << endl;
        //batchSet->freeBuf();
        return false;
    }

    string msg;
    WriteBatch writeBatch;
    if (batchSet->isValid(*commandValidator_, commandResponse, user_id, request_context)) {
        batchSet->createWriteBatch(writeBatch, commandResponse);
    }
    if (commandResponse.status().code() == Command_Status_StatusCode_INTERNAL_ERROR) {
        batchSet->freeBuf();
    }

    if (commandResponse.status().code() == Command_Status_StatusCode_SUCCESS) {
        WriteOptions writeOptions;
        writeOptions.sync = false;
        writeOptions.token = token;
        StoreOperationStatus opStatus = this->key_value_store_.WriteBat(writeOptions,
                &writeBatch, batchSet->getMemory());
        Command_Status_StatusCode cmdStatus = commandValidator_->toCommandStatus(opStatus);
        commandResponse.mutable_status()->set_code(cmdStatus);
        commandResponse.mutable_status()->set_statusmessage(
                commandResponse.status().StatusCode_Name(cmdStatus));
        batchSet->setDoNotFreeBuf();
    }
    if (commandResponse.status().code() != Command_Status_StatusCode_SUCCESS &&
        commandResponse.status().code() != Command_Status_StatusCode_NO_SPACE ) {
        Flush(false, true);
    }
    batchSet->complete();
    return (commandResponse.status().code() == Command_Status_StatusCode_SUCCESS);
}

StoreOperationStatus PrimaryStore::Put(
        const std::string& key,
        const PrimaryStoreValue& primary_store_value,
        IncomingValueInterface* value,
        bool guarantee_durable,
        const std::tuple<int64_t, int64_t> token) {
    Event e;
    profiler_.BeginAutoScoped(kPrimaryStorePut, &e);

    if (corrupt_) {
        return StoreOperationStatus_STORE_CORRUPT;
    }
    static StoreOperationStatus diskSpaceStatus = StoreOperationStatus_SUCCESS;
    uint64_t total_bytes, used_bytes;
    if (!device_information_.GetCapacity(&total_bytes, &used_bytes)) {
        LOG(ERROR) << "IE Store Status";
        return StoreOperationStatus_INTERNAL_ERROR;
    }
    if (diskSpaceStatus == StoreOperationStatus_SUCCESS &&
        total_bytes - used_bytes < kMinFreeSpace) {
        smr::Disk::noSpace(smr::Disk::DiskStatus::NO_SPACE);
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": >>>>>>>>>> Disk space is at no space threshold" << endl;
    }
    InternalValueRecord internal_value_record;

    //Always has empty value;
    std::string value_str = "";
    internal_value_record.set_value(value_str);
    internal_value_record.set_version(primary_store_value.version);
    internal_value_record.set_tag(primary_store_value.tag);
    internal_value_record.set_algorithm(primary_store_value.algorithm);
    std::string packed_value;

    if (!internal_value_record.SerializeToString(&packed_value)) {
        LOG(ERROR) << "Failed to serialize internal value record ";
        return StoreOperationStatus_INTERNAL_ERROR;
    };

    LevelDBData* myValue = new LevelDBData();
    myValue->type = LevelDBDataType::MEM_INTERNAL;
    myValue->headerSize = packed_value.size();
    myValue->header = new char[myValue->headerSize];

    memcpy(myValue->header, packed_value.data(), packed_value.size());
    myValue->dataSize = value->size();
    myValue->data = value->GetUserValue();
    myValue->memType = MEMORYType::MEM_FOR_CLIENT;


    switch (key_value_store_.Put(key, (char*) myValue, guarantee_durable, token)) {
        case StoreOperationStatus_SUCCESS:
            return StoreOperationStatus_SUCCESS;
        case StoreOperationStatus_NO_SPACE:
            delete [] myValue->header;
            delete myValue;
            LOG(ERROR) << "Failed to persist key/value in dbase. No space available.";//NO_SPELL
            return StoreOperationStatus_NO_SPACE;
        case StoreOperationStatus_FROZEN:
            delete [] myValue->header;
            delete myValue;
            LOG(ERROR) << "Failed to persist key/value in dbase. No space available.";//NO_SPELL
            return StoreOperationStatus_FROZEN;
        case StoreOperationStatus_SUPERBLOCK_IO:
            delete [] myValue->header;
            delete myValue;
            return StoreOperationStatus_SUPERBLOCK_IO;
        default:
            delete [] myValue->header;
            delete myValue;
            LOG(ERROR) << "Failed to persist key/value in dbase ";//NO_SPELL
            return StoreOperationStatus_INTERNAL_ERROR;
    }
}

StoreOperationStatus PrimaryStore::Delete(const std::string& key,
                                          bool guarantee_durable,
                                          const std::tuple<int64_t, int64_t> token) {
    if (corrupt_) {
        return StoreOperationStatus_STORE_CORRUPT;
    }
    StoreOperationStatus status = key_value_store_.Delete(key, guarantee_durable, token);
    if (status != StoreOperationStatus_SUCCESS) {
        LOG(ERROR) << "FAILED TO DELETE KEY " << key << endl;
        return status;
    }
    return status;
}

PrimaryStoreIterator* PrimaryStore::Find(const std::string& key) {
    return new PrimaryStoreIterator(corrupt_, file_system_store_, key_value_store_.Find(key), key_value_store_.GetName());
}

bool PrimaryStore::Close() {
    key_value_store_.Close();
    return true;
}

StoreOperationStatus PrimaryStore::Clear(std::string pin) {
    StoreOperationStatus store_operation_status = StoreOperationStatus_SUCCESS;
    // Close the underlying KV store so that there isn't any activity on the partition.
    // We don't have to do anything to the file store because it has no background tasks
    // and the MessageProcessor guarantees no operations happen in parallel with ISE
    key_value_store_.Close();

    PinStatus status = instant_secure_eraser_.Erase(pin);
    switch (status) {
        case PinStatus::PIN_SUCCESS:
            break;
        case PinStatus::AUTH_FAILURE:
            return StoreOperationStatus_AUTHORIZATION_FAILURE;
            break;
        default:
            LOG(ERROR) << "IE Store Status";
            return StoreOperationStatus_INTERNAL_ERROR;
            break;
    }

    if (!key_value_store_.Init(false)) {
        LOG(ERROR) << "IE Unable to create or reopen store";
    }

    bool create_partition;
    if (smr::DriveEnv::getInstance()->checkDiskInfoValid()) {
        store_operation_status = StoreOperationStatus_ISE_FAILED_VAILD_DB;
        create_partition = false;
    } else {
        key_value_store_.Close();

        if (!key_value_store_.Init(true)) {
            LOG(ERROR) << "IE Unable to create or reopen store";
            corrupt_ = true;
            return StoreOperationStatus_INTERNAL_ERROR;
        }

        if (!cluster_version_store_.Erase()) {
            LOG(ERROR) << "IE Unable to clear cluster version";
            corrupt_ = true;
            return StoreOperationStatus_INTERNAL_ERROR;
        }
        create_partition = true;
    }

    if (!instant_secure_eraser_.MountCreateFileSystem(create_partition)) {
        LOG(ERROR) << "IE Unable to mount file system";
        return StoreOperationStatus_INTERNAL_ERROR;
    }
    return store_operation_status;
}

void PrimaryStore::SetPreUsedBytes() {
    // Get the amount of space used
    uint64_t total_bytes;
    uint64_t used_bytes;
    device_information_.GetCapacity(&total_bytes, &used_bytes);

    // Store amount of space used in file
    VLOG(1) << "Used bytes = " << used_bytes;
    std::ofstream outfile(preused_file_path_, std::ofstream::binary);
    outfile << used_bytes;
    outfile.close();
}

bool PrimaryStore::SetRecordStatus(const std::string& key, bool bad) {
    VLOG(1) << "Updating internal value record";
    Event e;
    profiler_.BeginAutoScoped(kPrimaryStoreGet, &e);
    char* packed_value = NULL;

    if (corrupt_) {
        return false;
    }
    switch (key_value_store_.Get(key, packed_value)) {
        case StoreOperationStatus_SUCCESS:
            break;
        case StoreOperationStatus_NOT_FOUND:
            LOG(ERROR) << "StoreOperationStatus_NOT_FOUND";//NO_SPELL
            return false;
        case StoreOperationStatus_STORE_CORRUPT:
            corrupt_ = true;
            LOG(ERROR) << "StoreOperationStatus_STORE_CORRUPT";//NO_SPELL
            return false;
        default:
            LOG(ERROR) << "StoreOperationStatus_UNKOWN";//NO_SPELL
            return false;
    }

    InternalValueRecord internal_value_record;
    if (!internal_value_record.ParseFromString(std::string(packed_value))) {
        LOG(ERROR) << "Failed to parse from string";
        return false;
    }

    // set interal_value_record baddata to true
    internal_value_record.set_baddata(bad);

    std::string packed_value2;
    if (!internal_value_record.SerializeToString(&packed_value2)) {
        LOG(ERROR) << "Failed to serialize to string";
        return false;
    }

    // TODO(Gonzalo): Need to avoid adding this put to the token list in smrdb
    // TODO(Gonzalo): Set sync to false
    std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
    switch (key_value_store_.Put(key, (char*) packed_value2.data(),
                                 true, token)) {
        case StoreOperationStatus_SUCCESS:
            return true;
        default:
            LOG(ERROR) << "Failed to store new packed_value";//NO_SPELL
            return false;
    }
    return true;
}

PrimaryStore::BooleanOrError PrimaryStore::FileExists(const std::string &key,
    std::string* packed_value) {
    bool using_bloom_filter = true;
    char* tmpPackedVal = new char[sizeof(tmpPackedVal)];
    switch (key_value_store_.Get(key, tmpPackedVal, using_bloom_filter)) {
        case StoreOperationStatus_SUCCESS:
            break;
        case StoreOperationStatus_NOT_FOUND:
            *packed_value = "";
            delete[] tmpPackedVal;
            return kFalse;
        default:
            delete[] tmpPackedVal;
            return kError;
    }

    char* value_pointer;
    memcpy((char*)&value_pointer, tmpPackedVal, sizeof(void*));

    LevelDBData myData;
    if (!myData.deserialize(value_pointer)) {
      LOG(ERROR) << "Corrupt LevelDBData structure";
      deallocate_getvalue_buffer(value_pointer);
      delete[] tmpPackedVal;
      return kError;
    }

    InternalValueRecord internal_value_record;
    //Point to uservalue size
    if (!internal_value_record.ParseFromArray(myData.header, myData.headerSize)) {
        LOG(ERROR) << "IE Store Status";
        deallocate_getvalue_buffer(value_pointer);
        delete[] tmpPackedVal;
        return kError;
    }

    // Check the internal_value_record if the key has not been flagged
    if (internal_value_record.has_baddata()) {
        LOG(ERROR) << "Key has bad data";
        deallocate_getvalue_buffer(value_pointer);
        delete[] tmpPackedVal;
        return kError;
    }

    deallocate_getvalue_buffer(value_pointer);
    delete[] tmpPackedVal;

    return internal_value_record.has_value() ? kFalse : kTrue;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
