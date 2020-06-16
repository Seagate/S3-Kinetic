#include "product_flags.h"
#include "command_line_flags.h"
#include "instant_secure_eraser.h"
#include "mount_manager.h"
#include "security_manager.h"
#include "popen_wrapper.h"
#include "smrdisk/Disk.h"


namespace com {
namespace seagate {
namespace kinetic {
using com::seagate::kinetic::SecurityManager;

InstantSecureEraser::InstantSecureEraser(
    const string store_mountpoint,
    const string store_partition,
    const string store_device,
    const string metadata_mountpoint,
    const string metadata_partition,
    const string primary_db_path,
    const string file_store_path)
    : store_mountpoint_(store_mountpoint),
      store_partition_(store_partition),
      store_device_(store_device),
      metadata_mountpoint_(metadata_mountpoint),
      metadata_partition_(metadata_partition),
      primary_db_path_(primary_db_path),
      file_store_path_(file_store_path) {}

PinStatus InstantSecureEraser::SecureErase(std::string pin) {
    PinStatus ise_completed = PinStatus::INTERNAL_ERROR;
    SecurityManager sed_manager;

    MountManager mount_manager = MountManager();
    mount_manager.Unmount(metadata_mountpoint_);
    if (mount_manager.IsMounted(metadata_partition_, metadata_mountpoint_)) {
        LOG(ERROR) << metadata_mountpoint_ << " still mounted";
        return PinStatus::INTERNAL_ERROR;
    }

    // Partition is not mounted, clear to erase
    PinStatus status = sed_manager.Erase(pin);
    switch (status) {
        case PinStatus::PIN_SUCCESS:
            ise_completed = PinStatus::PIN_SUCCESS;
            break;
        default:
            // SED Drive failed ISE, Remount and return error.
            LOG(ERROR) << store_device_ << " failed ISE command.";//NO_SPELL
            return status;
    }
    return ise_completed;
}

bool InstantSecureEraser::MountCreateFileSystem(bool create_if_missing) {
    MountManager mount_manager = MountManager();
    if (create_if_missing) {
        // Partition erased, create new file system
        int mkfs_result;
        std::string mkfs_log;
        RawStringProcessor mkfs_processor(&mkfs_log, &mkfs_result);
        std::stringstream command;
        command << "mkfs.ext4 -E lazy_itable_init=0 " << metadata_partition_ << " 2>&1";
        std::string system_command = command.str();
        if (!execute_command(system_command, mkfs_processor)) {
            LOG(ERROR) << "Unable to create ext4 fs on " << metadata_partition_;//NO_SPELL
            LOG(ERROR) << mkfs_result;
            return false;
        }

        if (!mount_manager.MountExt4(metadata_partition_, metadata_mountpoint_)) {
            return false;
        }
    } else if (!mount_manager.MountExt4(metadata_partition_, metadata_mountpoint_)) {
        return false;
    }

    return true;
}

PinStatus InstantSecureEraser::Erase(std::string pin) {
    MountManager mount_manager = MountManager();
    mount_manager.Unmount(metadata_mountpoint_);
    if (mount_manager.IsMounted(metadata_partition_, metadata_mountpoint_)) {
        LOG(ERROR) << metadata_mountpoint_ << " still mounted";
        return PinStatus::INTERNAL_ERROR;
    }

    if (InstantSecureEraser::ClearSuperblocks(primary_db_path_)) {
        return PinStatus::PIN_SUCCESS;
    } else {
        return PinStatus::INTERNAL_ERROR;
    }
}

bool InstantSecureEraser::ClearSuperblocks(std::string device_path) {
    std::stringstream command;
    uint64_t seek_to;
    seek_to = smr::Disk::SUPERBLOCK_0_ADDR/1048576;
    command << "dd if=/dev/zero of=" << device_path << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    std::string system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed Erase on ";//NO_SPELL
        return false;
    }
    command.str("");
    seek_to = smr::Disk::SUPERBLOCK_1_ADDR/1048576;
    command << "dd if=/dev/zero of=" << device_path << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed Erase on ";//NO_SPELL
        return false;
    }

    command.str("");
    seek_to = smr::Disk::SUPERBLOCK_2_ADDR/1048576;
    command << "dd if=/dev/zero of=" << FLAGS_store_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed Erase on ";//NO_SPELL
        return false; //PinStatus::INTERNAL_ERROR;
    }

    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com


