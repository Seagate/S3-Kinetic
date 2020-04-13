#include  <cstring>
#include <memory>

#include "glog/logging.h"
#include "smrdb_test_helpers.h"
#include "util/random.h"
#include "kinetic/common.h"
#include "internal_value_record.pb.h"
#include "kernel_mem_mgr.h"
#include "smrdisk/ValueFileCache.h"
#include "mem/DynamicMemory.h"

namespace com {
namespace seagate {
namespace kinetic {

char* PackValueMem(std::string value) {
    char* perm_val = (char*) smr::DynamicMemory::getInstance()->allocate(value.size());
    strcpy(perm_val, value.c_str());
    proto::InternalValueRecord internal_value_record;

    //Always has empty value;
    std::string value_str = "";
    internal_value_record.set_value(value_str);
    internal_value_record.set_version("version");
    internal_value_record.set_tag("tag");
    internal_value_record.set_algorithm(1);
    std::string packed_value;

    if (!internal_value_record.SerializeToString(&packed_value)) {
        std::cout << "Serialization failed!" << endl;
    }

    LevelDBData* myValue = new LevelDBData();
    myValue->type = LevelDBDataType::MEM_INTERNAL;
    myValue->headerSize = packed_value.size();
    myValue->header = new char[myValue->headerSize];
    myValue->memType = MEMORYType::MEM_FOR_CLIENT;

    memcpy(myValue->header, packed_value.data(), packed_value.size());
    myValue->dataSize = (int)strlen(perm_val);
    myValue->data = perm_val;

    return reinterpret_cast<char*>(myValue);
}

// std::string PackValueString(std::string value) {
//     return std::string(PackValueMem(value), sizeof(internalValue));
// }

char* PackValueMem(std::string value, int* size) {
    *size = sizeof(LevelDBData);
    return PackValueMem(value);
}

char* PackValue(char* value) {
    return PackValueMem(std::string(value));
}

std::string UnpackValue(const char* packed_value) {
    char* value_pointer;
    memcpy((char*)&value_pointer, packed_value, sizeof(void*));

    LevelDBData myData;
    myData.deserialize(value_pointer);

    return std::string(myData.data, myData.dataSize);
}

std::string ExtractValue(const char *packed_value, const std::string key_value_store_name) {
    std::string result;
    LevelDBData myData;
    const LevelDBData* pData = (LevelDBData*)packed_value;

    if (isSerialized(pData->type)) {
            myData.deserialize(packed_value);
            pData = &myData;
    }
    if (pData->type == LevelDBDataType::MEM_INTERNAL) {
        // value->set_value(new OutgoingStringValue(std::string(pData->data, pData->dataSize)));
        result = std::string(pData->data, pData->dataSize);
    }
    if (pData->type == LevelDBDataType::MEM_EXTERNAL) {
        // char* buff = allocate_getvalue_buffer();
        char* buff = (char*) KernelMemMgr::pInstance_->AllocMem();
        if (buff) {
            ExternalValueInfo external;
            if (external.deserialize(pData->data)) {
                std::shared_ptr<RandomAccessFile> file;
                Status s = smr::CacheManager::cache(key_value_store_name)->getReadable(external.file_number, file);
                if (s.ok()) {
                    Slice not_needed;
                    s = file->Read(external.offset, external.size, &not_needed, buff);
                    if (s.ok()) {
                        // value->set_value(new OutgoingBuffValue(buff, buff, external.size));
                        result = std::string(buff, external.size);
                    }
                }
            }
            // deallocate_getvalue_buffer(buff);
            KernelMemMgr::pInstance_->FreeMem((void*) buff);
        }
    }
    return result;
}

void deallocate_getvalue_buffer(char* buff) {
    KernelMemMgr::pInstance_->FreeMem((void*) buff);
}

// std::string RandomString(Random* rnd, int len, std::string* dst) {
//     dst->resize(len);
//     for (int i = 0; i < len; i++) {
//         (*dst)[i] = static_cast<char>(' ' + rnd->Uniform(95));   // ' ' .. '~'
//     }
//     return *dst;
// }

// int RandomSeed() {
//     const char* env = getenv("TEST_RANDOM_SEED");
//     int result = (env != NULL ? atoi(env) : 301);
//     if (result <= 0) {
//         result = 301;
//     }
//     return result;
// }

// std::string RandomKey(Random* rnd, int len) {
//     // Make sure to generate a wide variety of characters so we
//     // test the boundary conditions for short-key optimizations.
//     static const char kTestChars[] = {
//         '\0', '\1', 'a', 'b', 'c', 'd', 'e', '\xfd', '\xfe', '\xff'
//     };
//     std::string result;
//     for (int i = 0; i < len; i++) {
//         result += kTestChars[rnd->Uniform(sizeof(kTestChars))];
//     }
//     return result;
// }

} // namespace kinetic
} // namespace seagate
} // namespace com
