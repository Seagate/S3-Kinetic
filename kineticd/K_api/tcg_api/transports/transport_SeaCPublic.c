/*
 * Do NOT modify or remove this copyright and confidentiality notice.
 *
 * Copyright 2013 - 2014 Seagate Technology LLC.
 *
 * The code contained herein is CONFIDENTIAL to Seagate Technology LLC
 * and may be covered under one or more Non-Disclosure Agreements. All or
 * portions are also trade secret. Any use, modification, duplication,
 * derivation, distribution or disclosure of this code, for any reason,
 * not expressly authorized is prohibited. All other rights are expressly
 * reserved by Seagate Technology LLC.
 *
 */

#include <string.h>
#include "transport_SeaCPublic.h"
#include "debug.h"
#include "cmds.h"
#include "scsi_helper.h"


static transport scTransport;
DEVICE *scDevice = NULL;
static char scRecvErrorMsg[ 100 ];

status scRecv_SCSI(int protocolId, int comId) {
    int io_cmd_status = 0;
    SCSI_SECURITY_PROTOCOL_IN_OPT opts;
    CHECK_DEBUG;
    /*printf("scRecv_SCSI: %d, %d\n", protocolId, comId);*/
    memset(&opts, 0, sizeof(opts));
    opts.security_protocol = protocolId;
    opts.spSpecificLSB = (0xFF & comId);
    opts.spSpecificMSB = ((comId >> 8) & 0xFF);
    opts.data_size = PAYLOAD_BUFFER_SIZE;
    opts.pData = scTransport.recvBuffer;
    if (scDevice == NULL) {
        return API_ERROR("scRecv_SCSI called with NULL device pointer");
    }
    /*printf("scRecv_SCSI: calling scsi_security_protocol_in\n");*/
    io_cmd_status = scsi_security_protocol_in(scDevice, opts);
    /*printf("scRecv_SCSI: ...result: %d\n", io_cmd_status);*/
    if (io_cmd_status == 0) {
        scTransport.recvBufferTail = scTransport.recvBuffer + PAYLOAD_BUFFER_SIZE;
        DEBUG_S(dumpRecvBuffer(&scTransport));
        return SUCCESS;
    }
    sprintf(scRecvErrorMsg, "scsi_security_protocol_in result = %d\n", io_cmd_status);
    return ERROR_(scRecvErrorMsg);
}

status scRecv_ATA(int protocolId, int comId) {
    int io_cmd_status;
    ATA_PT_CMD_OPTS opts;
    CHECK_DEBUG;
    DEBUG(("scRecv_ATA: %d, %d\n", protocolId, comId));
    memset(&opts, 0, sizeof(opts));
    opts.data_size = PAYLOAD_BUFFER_SIZE;
    DEBUG(("data_size: %d\n", opts.data_size));
    opts.pData = scTransport.recvBuffer;
    opts.cmd_type = ATA_CMD_TYPE_TASKFILE;
    opts.cmd_protocol = ATA_PROTOCOL_PIO;
    opts.cmd_direction = XFER_DATA_IN;
    opts.tfr.ErrorFeature = protocolId;
    opts.tfr.SectorCount = PAYLOAD_SECTOR_COUNT;
    opts.tfr.LbaLow = 0; //sizeof(scTransport.recvBuffer);
    opts.tfr.LbaMid = (0xFF & comId);
    opts.tfr.LbaHi = ((comId >> 8) & 0xFF);
    opts.tfr.CommandStatus = ATA_PIO_TRUSTED_RECIEVE;
    if (scDevice == NULL) {
        return API_ERROR("scRecv_ATA called with NULL device pointer");
    }
    DEBUG(("scRecv_ATA: calling ata_pt_cmd\n"));
    io_cmd_status = ata_pt_cmd(scDevice, &opts);
    DEBUG(("scRecv_ATA: ...result: %d\n", io_cmd_status));
    if (io_cmd_status != 0) {
        sprintf(scRecvErrorMsg, "ata_pt_cmd result = %d\n", io_cmd_status);
        return ERROR_(scRecvErrorMsg);
    }
    if (opts.rtfr.status != ATA_GOOD_STATUS) {
        sprintf(scRecvErrorMsg, "Cannot Talk to Drive (SED off or Drive invalid): ata_pt_cmd rtfr status = 0x%x, error = 0x%x\n", opts.rtfr.status, opts.rtfr.error);
        return ERROR_(scRecvErrorMsg);
    }
    scTransport.recvBufferTail = scTransport.recvBuffer + PAYLOAD_BUFFER_SIZE;
    DEBUG_S(dumpRecvBuffer(&scTransport));
    return SUCCESS;
}


static char scSendErrorMsg[ 100 ];

status scSend_SCSI(int protocolId, int comId) {
    int io_cmd_status = 0;
    SCSI_SECURITY_PROTOCOL_OUT_OPT opts;
    uint32_t sector_count = 0;
    CHECK_DEBUG;
    /*printf("scSend_SCSI(%d, 0x%0x) called\n", protocolId, comId);*/
    memset(&opts, 0, sizeof(opts));
    DEBUG_S(dumpSendBuffer(&scTransport));
    opts.data_size = scTransport.sendBufferTail - scTransport.sendBuffer;
    opts.pData = scTransport.sendBuffer;
    sector_count = (opts.data_size + 511 ) / 512;
    sector_count = sector_count * 512; // Normalize to sector boundary
    opts.data_size = sector_count;
    opts.security_protocol = protocolId;
    opts.spSpecificLSB = (0xFF & comId);
    opts.spSpecificMSB = ((comId >> 8) & 0xFF);
    /*printf("scSend_SCSI: calling scsi_security_protocol_out\n");*/
    io_cmd_status = scsi_security_protocol_out(scDevice, opts);
    /*printf("scSend_SCSI: ... result: %d\n", io_cmd_status);*/
    if (io_cmd_status == 0) {
        return SUCCESS;
    }
    sprintf(scSendErrorMsg, "scsi_security_protocol_out result = %d\n", io_cmd_status);
    return ERROR_(scSendErrorMsg);
}

status scSend_ATA(int protocolId, int comId) {
    int io_cmd_status;
    ATA_PT_CMD_OPTS opts;
    CHECK_DEBUG;
    memset(&opts, 0, sizeof(opts));
    DEBUG(("scSend_ATA(%d, 0x%0x) called\n", protocolId, comId));
    DEBUG_S(dumpSendBuffer(&scTransport));
    opts.data_size = scTransport.sendBufferTail - scTransport.sendBuffer;
    opts.pData = scTransport.sendBuffer;
    opts.cmd_type = ATA_CMD_TYPE_TASKFILE;
    opts.cmd_protocol = ATA_PROTOCOL_PIO;
    opts.cmd_direction = XFER_DATA_OUT;
    opts.tfr.ErrorFeature = protocolId;
    opts.tfr.SectorCount = (opts.data_size + 511 ) / 512;
    opts.data_size = opts.tfr.SectorCount * 512; // Normalize to sector boundary for ATA.
    opts.tfr.LbaLow = 0;
    opts.tfr.LbaMid = (0xFF & comId);
    opts.tfr.LbaHi = ((comId >> 8) & 0xFF);
    opts.tfr.CommandStatus = ATA_PIO_TRUSTED_SEND;
    DEBUG(("scSend_ATA: calling ata_pt_cmd\n"));
    io_cmd_status = ata_pt_cmd(scDevice, &opts);
    DEBUG(("scSend_ATA: ... result: %d\n", io_cmd_status));
    if (io_cmd_status != 0) {
        sprintf(scSendErrorMsg, "ata_pt_cmd result = %d\n", io_cmd_status);
        return ERROR_(scSendErrorMsg);
    } else if (opts.rtfr.status != ATA_GOOD_STATUS) {
        sprintf(scSendErrorMsg, "ata_pt_cmd rtfr status = 0x%x, error = 0x%x\n", opts.rtfr.status, opts.rtfr.error);
        return ERROR_(scSendErrorMsg);
    }
    return SUCCESS;
}


transport *transportFromSeaCLibDevice(DEVICE *device) {
    if (device == NULL) {
        return NULL;
    }

    CHECK_DEBUG;
    if (device->drive_info.drive_type == ATA_DRIVE) {
        scDevice = device;
        DEBUG(("ATA DRIVE\n"));
        scTransport.sendFunction = scSend_ATA;
        scTransport.recvFunction = scRecv_ATA;
        return &scTransport;
    }

    if (device->drive_info.drive_type == SCSI_DRIVE) {
        scDevice = device;
        DEBUG(("SCSI DRIVE\n"));
        scTransport.sendFunction = scSend_SCSI;
        scTransport.recvFunction = scRecv_SCSI;
        return &scTransport;
    }

    printf("Drive is NOT ATA or SCSI: %d, and is not supported at this time.\n", device->drive_info.drive_type);
    return NULL;
}
