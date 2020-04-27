#include "mount_manager.h"
#include "popen_wrapper.h"

#if BUILD_FOR_ARM == 1
#include <mntent.h>
#endif
#include <fstream>

namespace com {
namespace seagate {
namespace kinetic {

#if BUILD_FOR_ARM == 1
const int MountManagerARM::MAX_ALLOWED_ERRORS = 5;

MountManagerARM::MountManagerARM() {}

bool MountManagerARM::Unmount(const string mountpoint) {
    if (umount(mountpoint.c_str())) {
        PLOG(ERROR) << "Unable to unmount " << mountpoint;
        return false;
    }
    return true;
}

bool MountManagerARM::IsMounted(string dev_path_store_partion, string dev_path_mountpoint) {
    bool is_mounted = false;

    std::ifstream infile("/proc/mounts", std::ifstream::binary);
    if (infile.is_open()) {
        string line;
        char store_mountpoint[50];
        char store_partition[50];
        while (getline(infile, line)) {
            if (sscanf(line.c_str(), "%s %s %*s %*s %*d %*d", store_partition,
                store_mountpoint) == 2) {
                if (store_mountpoint == dev_path_mountpoint &&
                    dev_path_store_partion == store_partition) {
                    is_mounted = true;
                }
            }
        }
        infile.close();
    }

    return is_mounted;
}

bool MountManagerARM::MountExt4(string part_path, string mountpoint) {
    if (mount(part_path.c_str(), mountpoint.c_str(), "ext4", MS_NOATIME, NULL)) {
        PLOG(ERROR) << "Unable to mount " << part_path << " on " << mountpoint;
        return false;
    }
    return true;
}

bool MountManagerARM::CheckFileSystemReadonly(int put_errors, string dev_path_mountpoint,
    string dev_path_store_partition) {
    if (put_errors >= MAX_ALLOWED_ERRORS) {
        return true;
    }

    string command("mount -t ext4 ");
    command.append(dev_path_store_partition);
    command.append(" ");
    command.append(dev_path_mountpoint);

    int status = system(command.c_str());

    if (status != 0) {
        int err = errno;
        PLOG(ERROR) << "mount failed: ";

        // Check if it is readonly
        if (err == EROFS || err == ENOENT) {
            return true;
        }
    }
    return false;
}
#else

MountManagerX86::MountManagerX86() {}

bool MountManagerX86::Unmount(const string mountpoint) {
    return true;
}

bool MountManagerX86::IsMounted(string dev_path_store_partion,
    string dev_path_mountpoint) {
    return true;
}

bool MountManagerX86::MountExt4(string part_path, string mountpoint) {
    return true;
}

bool MountManagerX86::CheckFileSystemReadonly(int put_errors, string dev_path_mountpoint,
    string dev_path_store_partion) {
    return true;
}
#endif

} // namespace kinetic
} // namespace seagate
} // namespace com
