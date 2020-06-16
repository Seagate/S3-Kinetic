#ifndef KINETIC_MOUNT_MANAGER_H_
#define KINETIC_MOUNT_MANAGER_H_

#include <string>
#include <sys/mount.h>

#include "glog/logging.h"
#include "gmock/gmock.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class MountManagerInterface {
    public:
    virtual bool MountExt4(string part_path, string mountpoint) = 0;
    virtual bool IsMounted(string dev_path_store_partion, string dev_path_mountpoint) = 0;
    virtual bool Unmount(string mountpoint) = 0;
    virtual bool CheckFileSystemReadonly(int put_errors, string dev_path_mountpoint,
        string dev_path_store_partion) = 0;
};

class MockMountManager : public MountManagerInterface {
    public:
    MOCK_METHOD2(MountExt4, bool(string part_path, string mountpoint));
    MOCK_METHOD2(IsMounted, bool(string dev_path_mountpoint, string dev_path_store_partion));
    MOCK_METHOD1(Unmount, bool(string mountpoint));
    MOCK_METHOD3(CheckFileSystemReadonly, bool(int put_errors, string dev_path_mountpoint,
        string dev_path_store_partion));
};

class MountManager : public MountManagerInterface {
    public:
    static const int MAX_ALLOWED_ERRORS;
    MountManager();
    bool MountExt4(string part_path, string mountpoint);
    bool IsMounted(string dev_path_store_partion, string dev_path_mountpoint);
    bool Unmount(const string mountpoint);
    bool CheckFileSystemReadonly(int put_errors, string dev_path_mountpoint,
        string dev_path_store_partion);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOUNT_MANAGER_H_
