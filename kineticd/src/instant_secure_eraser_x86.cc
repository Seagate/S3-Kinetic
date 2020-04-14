#include "instant_secure_eraser.h"

#include "popen_wrapper.h"
#include "command_line_flags.h"
#include "smrdisk/Disk.h"

namespace com {
namespace seagate {
namespace kinetic {

InstantSecureEraserX86::InstantSecureEraserX86(
        const string primary_db_path,
        const string file_store_path)
    : primary_db_path_(primary_db_path), file_store_path_(file_store_path) {}

PinStatus InstantSecureEraserX86::Erase(std::string pin) {
    if (InstantSecureEraserX86::ClearSuperblocks(primary_db_path_)) {
        return PinStatus::PIN_SUCCESS;
    } else {
        return PinStatus::INTERNAL_ERROR;
    }
}

bool InstantSecureEraserX86::ClearSuperblocks(std::string device_path) {
    std::stringstream command;
    uint64_t seek_to;
    seek_to = smr::Disk::SUPERBLOCK_0_ADDR/1048576;
    command << "dd if=/dev/zero of=" << device_path << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    std::string system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
        return false;
    }
    command.str("");
    seek_to = smr::Disk::SUPERBLOCK_1_ADDR/1048576;
    command << "dd if=/dev/zero of=" << device_path << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
        return false;
    }

    command.str("");
    seek_to = smr::Disk::SUPERBLOCK_2_ADDR/1048576;
    command << "dd if=/dev/zero of=" << FLAGS_store_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
        return false; //PinStatus::INTERNAL_ERROR;
    }

    return true;
}

bool InstantSecureEraserX86::MountCreateFileSystem(bool create_if_missing) {
    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com

