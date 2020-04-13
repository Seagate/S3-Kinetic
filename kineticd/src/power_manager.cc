#include "power_manager.h"
#include "server.h"
#include "glog/logging.h"
#include "ata_cmd_handler.h"

using ::zac_ha_cmd::AtaCmdHandler;

namespace com {
namespace seagate {
namespace kinetic {

PowerManager::PowerManager(SkinnyWaistInterface& skinny_waist, std::string device_path)
                           :skinny_waist_(skinny_waist),
                           device_path_(device_path) {
    curr_power_level_ = proto::Command::OPERATIONAL;
}

void PowerManager::ProcessRequest(const proto::Command &command, proto::Command *command_response) {
    switch (command.body().power().level()) {
        case proto::Command::OPERATIONAL:
            SetOperational(command_response);
            break;
        case proto::Command::HIBERNATE:
            SetHibernate(command_response);
            break;
        case proto::Command::SHUTDOWN:
            SetShutdown(command_response);
            break;
        case proto::Command::FAIL:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_NOT_ATTEMPTED);
            command_response->mutable_status()->set_statusmessage("PowerLevel is not supported!");
            break;
        default:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
            break;
    }
}

void PowerManager::SetOperational(proto::Command *command_response) {
    if (curr_power_level_ != proto::Command::OPERATIONAL) {
        OpenDatabase();
        curr_power_level_ = proto::Command::OPERATIONAL;
    }
    command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
}

void PowerManager::SetHibernate(proto::Command *command_response) {
    // Default to internal error; overwritten if successful
    command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);

    // Check if already in hibernate mode
    if (curr_power_level_ == proto::Command::HIBERNATE) {
        command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
        return;
    }

    int state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::HIBERNATE,
                                                        proto::Message_AuthType_HMACAUTH,
                                                        proto::Command_MessageType_SET_POWER_LEVEL,
                                                        command_response);
    if (state_result < 0) {
        return;
    } else if (state_result == 0) {
        command_response->mutable_status()->set_statusmessage("Unable to transition to HIBERNATE state");
        return;
    }

    // Close database to ensure that the drive is not prematurely brought out of idle state
    if (!CloseDatabase()) {
        command_response->mutable_status()->set_statusmessage("Unable to close database");
        server_->StateChanged(com::seagate::kinetic::StateEvent::RESTORED);
        return;
    }

    // Open device to send go idle ata command
    int fd = OpenDevice();

    // Check if file descriptor is valid
    if (fd < 0) {
        OpenDatabase();
        command_response->mutable_status()->set_statusmessage("Unable to open device");
        return;
    }

    // Send go idle command
    bool go_idle = ATAGoIdle2(fd);

    // Close device
    if (close(fd)) {
        LOG(ERROR) << "Close fd for device '" << device_path_ << "' failed " << strerror(errno) << " (" << errno << ")";
    }

    // Check if go idle command was successful
    if (!go_idle) {
        OpenDatabase();
        command_response->mutable_status()->set_statusmessage("Drive failed to hibernate");
        return;
    }

    command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    curr_power_level_ = proto::Command::HIBERNATE;
}

void PowerManager::SetShutdown(proto::Command *command_response) {
    int state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::SHUTDOWN,
                                                        proto::Message_AuthType_HMACAUTH,
                                                        proto::Command_MessageType_SET_POWER_LEVEL,
                                                        command_response);
    if (state_result == 0) {
        command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
        command_response->mutable_status()->set_statusmessage("Unable to transition to SHUTDOWN state");
    } else if (state_result > 0) {
        command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
        curr_power_level_ = proto::Command::SHUTDOWN;
    }
}

void PowerManager::OpenDatabase() {
    switch (skinny_waist_.InitUserDataStore()) {
        case UserDataStatus::SUCCESSFUL_LOAD:
            server_->StateChanged(com::seagate::kinetic::StateEvent::RESTORED);
            break;
        case UserDataStatus::STORE_CORRUPT:
            server_->StateChanged(com::seagate::kinetic::StateEvent::STORE_CORRUPT);
            break;
        default:
            server_->StateChanged(com::seagate::kinetic::StateEvent::STORE_INACCESSIBLE);
            break;
    }
}

bool PowerManager::CloseDatabase() {
    if (!skinny_waist_.IsDBOpen()) {
        return true;
    }

    skinny_waist_.CloseDB();
    return true;
}

int PowerManager::OpenDevice() {
    int ret;
    struct stat st;

    int device_fd = open(device_path_.c_str(), O_RDONLY);
    if (device_fd < 0) {
        LOG(ERROR) << "Close fd for device '" << device_path_ << "' failed " << strerror(errno) << " (" << errno << ")";
        ret = -errno;
        close(device_fd);
        return ret;
    }

    if (fstat(device_fd, &st) != 0) {
        LOG(ERROR) << "Close fd for device '" << device_path_ << "' failed "
                   << strerror(errno) << " (" << errno << ")";
        ret = -errno;
        close(device_fd);
        return ret;
    }

    if ((!S_ISCHR(st.st_mode)) && (!S_ISBLK(st.st_mode))) {
        ret = -ENXIO;
        close(device_fd);
        return ret;
    }
    return device_fd;
}

bool PowerManager::ATAGoIdle2(int fd) {
    AtaCmdHandler ata_cmd_handler;
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));

    cdb[0]  |= 0x85;            // OPERATION_CODE
    cdb[1]  |= 0x3 << 1;        // PROTOCOL
    cdb[4]  |= 0x4a;            // FEATURES
    cdb[6]  |= 0x82;            // SECTOR_COUNT
    cdb[8]  |= 0x1;             // LBA_LOW
    cdb[13] |= 0x10;            // DEVICE
    cdb[14] |= 0xef;            // COMMAND

    ata_cmd_handler.ClearDataBuf();
    ata_cmd_handler.ConstructIoHeader(SG_DXFER_NONE, &io_hdr);

    int ret = ata_cmd_handler.ExecuteSgCmd(fd, cdb, sizeof(cdb), io_hdr);

    if (ret != 0) {
        return false;
    }

    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
