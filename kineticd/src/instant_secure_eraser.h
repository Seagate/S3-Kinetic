#ifndef KINETIC_INSTANT_SECURE_ERASER_H_
#define KINETIC_INSTANT_SECURE_ERASER_H_

#include <string>
#include <sys/mount.h>

#include "security_manager.h"

#include "glog/logging.h"
#include "gmock/gmock.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class Server;

class InstantSecureEraserInterface {
    public:
    virtual PinStatus SecureErase(std::string pin) = 0;
    virtual PinStatus Erase(std::string pin) = 0;
    virtual bool MountCreateFileSystem(bool create_if_missing) = 0;
};

class MockInstantSecureEraser : public InstantSecureEraserInterface {
    public:
    MOCK_METHOD1(SecureErase, PinStatus(std::string));
    MOCK_METHOD1(Erase, PinStatus(std::string));
    MOCK_METHOD1(MountCreateFileSystem, bool(bool create_if_missing));
};

class InstantSecureEraser : public InstantSecureEraserInterface {
    public:
    InstantSecureEraser(const string store_mountpoint,
                           const string store_partition,
                           const string store_device,
                           const string metadata_mountpoint,
                           const string metadata_partition,
                           const string primary_db_path,
                           const string file_store_path);

    virtual PinStatus SecureErase(std::string pin);
    virtual bool MountCreateFileSystem(bool create_if_missing);
    virtual PinStatus Erase(std::string pin);
    static bool ClearSuperblocks(std::string device_path);

    private:
    const string store_mountpoint_;
    const string store_partition_;
    const string store_device_;
    const string metadata_mountpoint_;
    const string metadata_partition_;
    const string primary_db_path_;
    const string file_store_path_;
};


} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_INSTANT_SECURE_ERASER_H_
