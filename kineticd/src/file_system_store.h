#ifndef KINETIC_FILE_SYSTEM_STORE_H_
#define KINETIC_FILE_SYSTEM_STORE_H_

#include <string>

#include "gmock/gmock.h"
#include "kinetic/common.h"
#include "kinetic/incoming_value.h"

#include "outgoing_value.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingValueInterface;

class FileSystemStoreInterface {
    public:
    virtual ~FileSystemStoreInterface() {}
    virtual bool Init(bool create_if_missing) = 0;
    virtual bool Get(const std::string& key, NullableOutgoingValue* value) = 0;
    virtual bool Put(const std::string& key, IncomingValueInterface* value,
            bool guarantee_durable) = 0;
    virtual bool Delete(const std::string &key) = 0;
    virtual bool MScanGet(const std::string &key, NullableOutgoingValue *value) = 0;
    virtual bool RenameFile(const std::string& srcName, const std::string& key) = 0;
    virtual bool TemporarilyPut(const std::string& key, IncomingValueInterface* value,
            bool guarantee_durable, std::string& tmp_name) = 0;
    virtual void UnlinkFile(const std::string& srcName) = 0;
};

class FileSystemStore : public FileSystemStoreInterface {
    private:
    static size_t MAX_SUB_DIR;
    static size_t SUB_DIR_NAME_LEN;

    public:
    explicit FileSystemStore(const std::string &directory);
    bool Init(bool create_if_missing);
    bool Get(const std::string& key, NullableOutgoingValue* value);
    bool Put(const std::string& key, IncomingValueInterface* value, bool guarantee_durable);
    bool Delete(const std::string &key);
    bool MScanGet(const std::string &key, NullableOutgoingValue *value);
    bool RenameFile(const std::string& srcName, const std::string& key);
    bool TemporarilyPut(const std::string& key, IncomingValueInterface* value,
            bool guarantee_durable, std::string& tmp_name);
    void UnlinkFile(const std::string& srcName);

    private:
    OutgoingValueInterface* ms_file_value_;
    const std::string directory_;
    void FileName(const std::string &key, std::string *result, std::string* subDir = NULL);
    bool MakeTemporaryFile(std::string *name, int *fd, const std::string* subDir,
                           bool guarantee_durable);
    void Sha1(const std::string &key, unsigned char *hash);
    char *Hex(const unsigned char *in, size_t n);
    bool SyncContainingDir(const std::string& file);
    DISALLOW_COPY_AND_ASSIGN(FileSystemStore);
};

class MockFileSystemStore : public FileSystemStoreInterface {
    public:
    MockFileSystemStore() {}
    virtual bool RenameFile(const std::string& srcName, const std::string& key) { return true; }
    virtual bool TemporarilyPut(const std::string& key, IncomingValueInterface* value,
            bool guarantee_durable, std::string& tmp_name) { return true; }

    MOCK_METHOD1(Init, bool(bool create_if_missing));
    MOCK_METHOD2(Get, bool(const std::string& key, NullableOutgoingValue* value));
    MOCK_METHOD3(Put, bool(const std::string& key, IncomingValueInterface* value,
        bool guarantee_durable));
    MOCK_METHOD1(Delete, bool(const std::string& key));
    MOCK_METHOD2(MScanGet, bool(const std::string &key, NullableOutgoingValue *value));
    MOCK_METHOD1(UnlinkFile, void(const std::string& srcName));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_FILE_SYSTEM_STORE_H_
