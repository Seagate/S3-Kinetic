#include "includes/ata_cmd_handler.h"
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/fs.h>
#include <string>
#include <memory>

namespace zac_ha_cmd {

AtaCmdHandler::AtaCmdHandler() {
    data_buf_.clear();
}

AtaCmdHandler::~AtaCmdHandler() {
    data_buf_.clear();
}

int AtaCmdHandler::ConstructIoHeader(int dxfer_direct, sg_io_hdr_t *io_hdr) {
    io_hdr->interface_id = 'S';
    io_hdr->dxfer_direction = dxfer_direct;
    io_hdr->mx_sb_len = kZAC_SG_SENSE_MAX_LENGTH; //sizeof(sense_buf_);
    io_hdr->dxfer_len = data_buf_.size();
    io_hdr->dxferp = static_cast<void*>(&data_buf_[0]);
    io_hdr->sbp = sense_buf_;
    io_hdr->timeout = 20000;
    io_hdr->flags = 0;
    memset(&sense_buf_, 0, sizeof(sense_buf_));
    return 0;
}

int AtaCmdHandler::ConstructIoHeader(int dxfer_direct, sg_io_hdr_t *io_hdr,
                                     uint8_t *data, size_t data_size) {
    io_hdr->interface_id = 'S';
    io_hdr->dxfer_direction = dxfer_direct;
    io_hdr->mx_sb_len = sizeof(sense_buf_);
    io_hdr->dxfer_len = data_size;
    io_hdr->dxferp = data;
    io_hdr->sbp = sense_buf_;
    io_hdr->timeout = 20000;
    io_hdr->flags = 0;
    memset(&sense_buf_, 0, sizeof(sense_buf_));
    return 0;
}

/// clean up verbose logging, tie to specific flag/option
int AtaCmdHandler::ExecuteSgCmd(int fd, uint8_t *cdb, size_t cdb_length, sg_io_hdr_t io_hdr) {
    int ret = 0;
    cdb[0] = kZAC_SG_ATA16_CDB_OPCODE_;
    io_hdr.cmd_len = cdb_length;
    io_hdr.cmdp = cdb;
#ifdef KDEBUG
    std::cout << "----EXECUTE_SG_CMD----" << std::endl;
    char msg[512];
    std::cout << "* +==================================\n";
    std::cout << "* |Byte |   0  |  1   |  2   |  3   |\n";
    std::cout << "* |=====+======+======+======+======+" << std::endl;
    int i = 0;
    while (i < 16) {
        int n = 0;
        for (int j = 0; j < 4; j++) {
            if ( i < 16 ) {
                n += sprintf(msg + n, " 0x%02x |", (int)io_hdr.cmdp[i]);
            } else {
                n += sprintf(msg + n, "      |");
            }
            i++;
        }
        printf("* | %3d |%s\n", i, msg);
        if ( i < (16 - 4) ) {
            std::cout << "* |=====+======+======+======+======+\n";
        } else {
            std::cout << "* +==================================" << std::endl;
        }
    }
#endif
    ret = ioctl(fd, SG_IO, &io_hdr);
    if (ret != 0) {
        std::cout << "SG_IO ioctl fail: " << errno << " strerror: " << strerror(errno) << std::endl;
        return -errno;
    }
    /* Check status */
    if ((cdb[2] & (1 << 5))) {
        /* ATA command status */
        if (io_hdr.status != kSG_CHECK_CONDITION_) {
            ret = -EIO;
            return ret;
        }
    }

    if (io_hdr.status || (io_hdr.host_status != kSG_DID_OK_)) {
        printf("Command failed with status 0x%02x (0x%02x), host status 0x%04x\n",
            (unsigned int)io_hdr.status,
            (unsigned int)io_hdr.masked_status,
            (unsigned int)io_hdr.host_status);
        ret = -EIO;
        return ret;
    }
    return 0;
}

std::vector<uint8_t>* AtaCmdHandler::GetDataBufp() {
    return &data_buf_;
}

uint8_t* AtaCmdHandler::GetSenseBufp() {
    return &sense_buf_[0];
}


void AtaCmdHandler::AllocateDataBuf(unsigned int size) {
    data_buf_.assign(size, 0);
}

void AtaCmdHandler::ClearDataBuf() {
    data_buf_.clear();
}

} // namespace zac_ha_cmd
