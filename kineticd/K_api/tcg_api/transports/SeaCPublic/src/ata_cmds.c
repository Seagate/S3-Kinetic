//
// ata_cmds.c   Implementation for Public ATA cmds functions
//
// Do NOT modify or remove this copyright and confidentiality notice.
//
// Copyright 2012 - 2014 Seagate Technology LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology LLC 
// and may be covered under one or more Non-Disclosure Agreements. 
// All or portions are also trade secret. 
// Any use, modification, duplication, derivation, distribution or disclosure
// of this code, for any reason, not expressly authorized is prohibited. 
// All other rights are expressly reserved by Seagate Technology LLC.
//
// *****************************************************************************

// \file ata_cmds.c   Implementation for ATA Spec command functions
//                     The intention of the file is to be generic & not OS specific

#include "cmds.h"
#include "sat_helper_func.h"
#include "scsi_helper_func.h"
#include <time.h>
#include "common.h"

#ifdef __linux__
#include <inttypes.h>
#include "sg_helper.h"
#else
#include <stdint.h>
#include "win_helper.h"
#endif

// \fn ata_pt_cmd(DEVICE * device, ATA_PT_CMD_OPTS * cmd_opts)
// \brief Function to send a ATA Spec cmd as a passthrough
// \param cmd_opts ata command options
// \param device  file descriptor
// \return 0 == success, < 0 something went wrong (-2 unsupported opts)
int ata_pt_cmd(DEVICE * device, ATA_PT_CMD_OPTS  * ata_cmd_opts)
{
    //printf("--> %s\n",__FUNCTION__);
    int ret = 0;
    SAT_ATA_PASS_THROUGH ata_pt_cmd_opts = { 0 };
    SCSI_IO_CTX scsi_io_ctx; 
    uint8_t endTFRs[32];

	memset(&ata_pt_cmd_opts,0,sizeof(SAT_ATA_PASS_THROUGH));

    ata_pt_cmd_opts.byte_0_op_code = SAT_ATA_16;
    // Extended comand?
    if (ata_cmd_opts->cmd_type == ATA_CMD_TYPE_EXTENDED_TASKFILE) 
    {
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_EXTEND_BIT_SET;
    }
    // Set the protocol 
    switch (ata_cmd_opts->cmd_protocol) 
    {
    case ATA_PROTOCOL_PIO:
        if (ata_cmd_opts->cmd_direction == XFER_DATA_IN)
        {
            ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_PIO_DATA_IN;
        }
        else if (ata_cmd_opts->cmd_direction == XFER_DATA_OUT) 
        {
            ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_PIO_DATA_OUT;
        }
        else
        {
            return -2;
        }
        break;
    case ATA_PROTOCOL_DMA:
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_DMA;
        break;
    case ATA_PROTOCOL_NO_DATA:
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_NON_DATA;
        break;
    case ATA_PROTOCOL_DMA_QUE:
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_DMA_QUEUED;
        break;
    case ATA_PROTOCOL_DEV_DIAG:
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_EXE_DEV_DIAG;
        break;
    case ATA_PROTOCOL_DMA_FPDMA:
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_FPDMA;
        break;
    case ATA_PROTOCOL_SOFT_RESET:
        ata_pt_cmd_opts.byte_1_ext_proto_mc |= SAT_ATA_SW_RESET;
        break;
    case ATA_PROTOCOL_HARD_RESET:
        ata_pt_cmd_opts.byte_1_ext_proto_mc = SAT_ATA_HW_RESET;
        break;
    default:
        return -2;
        break;
    }
    // Transfer type related stuff
    ata_pt_cmd_opts.byte_2_bit_settings = SAT_BYTE_BLOCK_BIT_SET;
    switch (ata_cmd_opts->cmd_direction) 
    {
    case XFER_NO_DATA:
        ata_pt_cmd_opts.byte_2_bit_settings |= SAT_T_LEN_XFER_NO_DATA;
        ata_pt_cmd_opts.byte_2_bit_settings |= SAT_CK_COND_BIT_SET ;
        break;
    case XFER_DATA_IN:
        ata_pt_cmd_opts.byte_2_bit_settings |= SAT_T_DIR_DATA_IN;
        ata_pt_cmd_opts.byte_2_bit_settings |= SAT_T_LEN_XFER_SEC_CNT;
        break;
    case XFER_DATA_OUT:
        ata_pt_cmd_opts.byte_2_bit_settings |= SAT_T_DIR_DATA_OUT;
        ata_pt_cmd_opts.byte_2_bit_settings |= SAT_T_LEN_XFER_SEC_CNT;
        break;
    default:
        return -2;
        break;
    }
    
    //cmd specific
    ata_pt_cmd_opts.byte_3_feature_ext = ata_cmd_opts->tfr.Feature48;
    ata_pt_cmd_opts.byte_4_feature = ata_cmd_opts->tfr.ErrorFeature;
    ata_pt_cmd_opts.byte_5_sc_ext = ata_cmd_opts->tfr.SectorCount48;
    ata_pt_cmd_opts.byte_6_sc = ata_cmd_opts->tfr.SectorCount;
    // SPECIAL CASE:
    if ( (ata_cmd_opts->tfr.SectorCount == 0) && (ata_cmd_opts->cmd_direction == XFER_NO_DATA) )
    {
        // Consider it a single sector PIO command, e.g. IDENTIFY
        // TPSIU doesn't work, so we have to use the sector count. 
        ata_pt_cmd_opts.byte_6_sc = 1;
    }
    ata_pt_cmd_opts.byte_7_lba_low_ext = ata_cmd_opts->tfr.LbaLow48;
    ata_pt_cmd_opts.byte_8_lba_low = ata_cmd_opts->tfr.LbaLow;
    ata_pt_cmd_opts.byte_9_lba_mid_ext = ata_cmd_opts->tfr.LbaMid48;
    ata_pt_cmd_opts.byte_10_lba_mid = ata_cmd_opts->tfr.LbaMid;
    ata_pt_cmd_opts.byte_11_lba_high_ext = ata_cmd_opts->tfr.LbaHi48;
    ata_pt_cmd_opts.byte_12_lba_high = ata_cmd_opts->tfr.LbaHi;
    ata_pt_cmd_opts.byte_13_device = ata_cmd_opts->tfr.DeviceHead;
    ata_pt_cmd_opts.byte_14_cmd = ata_cmd_opts->tfr.CommandStatus;
    ata_pt_cmd_opts.byte_15_ctrl = ata_cmd_opts->tfr.DeviceControl;

    memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
    memset(endTFRs,0,sizeof(endTFRs));

    scsi_io_ctx.device = device;
    scsi_io_ctx.pcdb = (uint8_t *)&ata_pt_cmd_opts;
    scsi_io_ctx.cdb_len = sizeof(ata_pt_cmd_opts);
    scsi_io_ctx.direction = ata_cmd_opts->cmd_direction;
    scsi_io_ctx.pdata = ata_cmd_opts->pData;
    scsi_io_ctx.data_len = ata_cmd_opts->data_size;

    if (ata_cmd_opts->psense_data) 
    {        
        scsi_io_ctx.psense = ata_cmd_opts->psense_data;
        scsi_io_ctx.sense_sz = ata_cmd_opts->sense_size;
    }
    else
    {
        scsi_io_ctx.psense = endTFRs;
        scsi_io_ctx.sense_sz = sizeof(endTFRs);
    }

    scsi_io_ctx.verbose = 0;

    ret = send_io(&scsi_io_ctx);

	// TODO: handle IDE_INTERFACE & ATA_DRIVE TYPE return TFRs?
	if (device->drive_info.drive_type == SCSI_DRIVE) 
	{
        printf("here");
		if ( (ret >=0 ) && (scsi_io_ctx.psense) && (scsi_io_ctx.return_status.format != 0xFF) )
		{
			SAT_ATA_RETURN_DESC TFRs = { 0 };
			TFRs = get_ata_TFRs_from_sense(scsi_io_ctx.psense);

			if (TFRs.desc_code != 0)
			{
				if (TFRs.status_byte != ATA_GOOD_STATUS)
				{
					printf("%s, failed because bad satus 0x%X\n", __FUNCTION__, TFRs.status_byte);
					ret = -1;
				}
			}
			else
			{
				//Cmd Failed. 
				printf("%s, failed because bad desc code\n", __FUNCTION__);
				ret = -1;
			}
		}
	}

#ifdef _DEBUG
    printf("scsi rtfr 0x%x\n",scsi_io_ctx.rtfrs.status);
#endif
    // TODO: See if this makes the above redundunt in Windows
    memcpy(&ata_cmd_opts->rtfr,&scsi_io_ctx.rtfrs,sizeof(ATA_RETURN_TFRS));

    //printf("<-- %s\n",__FUNCTION__);
    return ret;
}

int get_ata_sanitize_device_feature(DEVICE * device, DRIVE_INFO * info)
{
	int ret = -1;
	ATA_PT_CMD_OPTS cmd_opts;
	uint8_t identify_buffer[DRIVE_SEC_SIZE];
	uint16_t * word_ptr = (uint16_t *)&identify_buffer[0];
	memset(identify_buffer,0,sizeof(identify_buffer));
	memset(&cmd_opts, 0, sizeof(ATA_PT_CMD_OPTS));
	cmd_opts.pData = identify_buffer;
	cmd_opts.data_size = sizeof(identify_buffer);
	cmd_opts.cmd_direction = XFER_DATA_IN;
	cmd_opts.cmd_protocol = ATA_PROTOCOL_PIO;
	cmd_opts.tfr.CommandStatus = 0xEC;
	cmd_opts.tfr.DeviceHead = 0x40;
	ret = ata_pt_cmd(device, &cmd_opts);
	if (ret >= 0) 
	{
		if (word_ptr[ATA_IDENTIFY_SANITIZE_INDEX] & ATA_IDENTIFY_SANITIZE_SUPPORTED) 
		{
			info->sanitize_opts.sanitize_cmd_enabled = 1;
			if (word_ptr[ATA_IDENTIFY_SANITIZE_INDEX] & ATA_IDENTIFY_CRYPTO_SUPPORTED)    
			{
				info->sanitize_opts.crypto = 1;
			}
			if (word_ptr[ATA_IDENTIFY_SANITIZE_INDEX] & ATA_IDENTIFY_OVERWRITE_SUPPORTED)
			{
				info->sanitize_opts.over_write = 1;
			}
			if (word_ptr[ATA_IDENTIFY_SANITIZE_INDEX] & ATA_IDENTIFY_BLOCK_ERASE_SUPPORTED)
			{
				info->sanitize_opts.block_erase = 1;
			}
		}
	}
	else
	{
		#ifdef _DEBUG
		printf("ATA IDENTIFY Failed %d\n", ret);
		#endif
	}

	return ret;
}

// \fn ata_secure_erase(DEVICE device)
// \brief Function to send a ATA Spec Secure Erase to the linux sg device
// \param DEVICE device  file descriptor
int ata_secure_erase(DEVICE * device)
{
    printf("--> %s\n",__FUNCTION__);

    printf("<-- %s\n",__FUNCTION__);
    return 0;
}

// \fn ata_sanitize_cmd(DEVICE device, ATA_SANITIZE_CMD_OPT cmd_opts)
// \brief Function to send a ATA Sanitize command
// \param device file descriptor
// \param ATA_SANITIZE_CMD_OPT struct containing the sanitize command options.  
int ata_sanitize_cmd(DEVICE * device, ATA_SANITIZE_CMD_OPT cmd_opts)
{
//    printf("--> %s\n",__FUNCTION__);
    int ret = 0;
    SAT_ATA_PASS_THROUGH ata_sanitize_cmd = { 0 };
    SCSI_IO_CTX scsi_io_ctx; 
    uint8_t endTFRs[32];

    ata_sanitize_cmd.byte_0_op_code = SAT_ATA_16;
    ata_sanitize_cmd.byte_1_ext_proto_mc = SAT_NON_DATA | SAT_EXTEND_BIT_SET;
    ata_sanitize_cmd.byte_2_bit_settings = SAT_CK_COND_BIT_SET | SAT_T_LEN_XFER_NO_DATA;
    //cmd specific
    ata_sanitize_cmd.byte_4_feature = ATA_SANITIZE_CRYPTO_FEAT;
    ata_sanitize_cmd.byte_7_lba_low_ext = (ATA_SANITIZE_CRYPTO_LBA >> 24) & 0xFF;; 
    ata_sanitize_cmd.byte_8_lba_low = ATA_SANITIZE_CRYPTO_LBA & 0xFF;
    ata_sanitize_cmd.byte_10_lba_mid = (ATA_SANITIZE_CRYPTO_LBA >> 8) & 0xFF;
    ata_sanitize_cmd.byte_12_lba_high = (ATA_SANITIZE_CRYPTO_LBA >> 16) & 0xFF;
    ata_sanitize_cmd.byte_13_device = 0x40;
    ata_sanitize_cmd.byte_14_cmd = ATA_SANITIZE_CMD;

    memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
    memset(endTFRs,0,sizeof(endTFRs));

    scsi_io_ctx.device = device;
    scsi_io_ctx.pcdb = (uint8_t *)&ata_sanitize_cmd;
    scsi_io_ctx.cdb_len = sizeof(ata_sanitize_cmd);
    scsi_io_ctx.direction = XFER_NO_DATA;
    if (cmd_opts.pData) 
    {        
        scsi_io_ctx.psense = cmd_opts.pData;
        scsi_io_ctx.sense_sz = cmd_opts.data_size;
    }
    else
    {
        scsi_io_ctx.psense = endTFRs;
        scsi_io_ctx.sense_sz = sizeof(endTFRs);
    }

    scsi_io_ctx.verbose = 0;
	scsi_io_ctx.return_status.status_scsi = 0x02; // Check Cond. 

    ret = send_io(&scsi_io_ctx);

    if ( ret >=0 )
    {

		// So the SCSI_STATUS should get filled correctly by the lower layer
		if (scsi_io_ctx.return_status.status_scsi != 0)
		{
			//something went wrong. 
			ret = -1;
			// Note: LSI STORELIB only gives back SENSE data with
			//       SAT DESCRIPTOR *if* something goes wrong
			//       in other words the SAT_CK_COND_BIT_SET doesn't work.
			if (scsi_io_ctx.psense)
			{
				SAT_ATA_RETURN_DESC TFRs = { 0 };
				TFRs = get_ata_TFRs_from_sense(scsi_io_ctx.psense);

				if (TFRs.desc_code != 0)
				{
					// TODO: Debug to figure out why the status byte is bogus in windows??
					// if (TFRs.status_byte != ATA_GOOD_STATUS)
					if (TFRs.error_byte)
					{
						ret = -1;
					}
				}
			}
		}
    }
//    printf("<-- %s\n",__FUNCTION__);
    return ret;
}

// \fn ata_get_sanitize_status(DEVICE device, uint8_t clear_failure, ATA_SANITIZE_STATUS status)
// \brief Function to send request scsi sense data
// \param device file descriptor
// \param ATA_SANITIZE_STATUS status return status to be filled in. 
int ata_get_sanitize_status(DEVICE * device, ATA_SANITIZE_CMD_OPT cmd_opts)
{
//    printf("--> %s\n",__FUNCTION__);
    int ret = 0;
    SAT_ATA_PASS_THROUGH ata_sanitize_cmd = { 0 };
    SCSI_IO_CTX scsi_io_ctx; 
    uint8_t endTFRs[32];

    ata_sanitize_cmd.byte_0_op_code = SAT_ATA_16;
    ata_sanitize_cmd.byte_1_ext_proto_mc = SAT_NON_DATA | SAT_EXTEND_BIT_SET;
    ata_sanitize_cmd.byte_2_bit_settings = SAT_CK_COND_BIT_SET | SAT_T_LEN_XFER_NO_DATA;
    if (cmd_opts.clear_failure)
    {
        ata_sanitize_cmd.byte_6_sc = ATA_SANITIZE_CLEAR_OPR_FAILED;
    }
    ata_sanitize_cmd.byte_13_device = 0x40;
    ata_sanitize_cmd.byte_14_cmd = ATA_SANITIZE_CMD;

    memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
    memset(endTFRs,0,sizeof(endTFRs));

    scsi_io_ctx.device = device;
    scsi_io_ctx.pcdb = (uint8_t *)&ata_sanitize_cmd;
    scsi_io_ctx.cdb_len = sizeof(ata_sanitize_cmd);
    scsi_io_ctx.direction = XFER_NO_DATA;
    if (cmd_opts.pData) 
    {        
        scsi_io_ctx.psense = cmd_opts.pData;
        scsi_io_ctx.sense_sz = cmd_opts.data_size;
    }
    else
    {
        scsi_io_ctx.psense = endTFRs;
        scsi_io_ctx.sense_sz = sizeof(endTFRs);
    }
    scsi_io_ctx.verbose = 0;

    ret = send_io(&scsi_io_ctx);

    if ( ret >=0 )
    {
		if (scsi_io_ctx.return_status.status_scsi != 0)
		{
			//something went wrong. 
			ret = -1;
			if (scsi_io_ctx.psense)
			{
				SAT_ATA_RETURN_DESC TFRs = { 0 };
				TFRs = get_ata_TFRs_from_sense(scsi_io_ctx.psense);

				if (TFRs.desc_code != 0)
				{
					// TODO: Debug to figure out why the status byte is bogus in windows??
					//       storelib doesn't like to return sense code for SAT commands
					// if (TFRs.status_byte != ATA_GOOD_STATUS)
					if (TFRs.status_byte != ATA_GOOD_STATUS)
					{
						ret = -1;
					}
				}
			}
		}
    }


//    printf("<-- %s\n",__FUNCTION__);
    return ret;
}

//add more outparameters to this if we need to know more info about an ATA device based off of an identify command.
int get_ATA_identify_info(DEVICE * device, int *isSSD, uint64_t *maxLBA)
{
//printf("identbuf: %d %d %d\n",sizeof(ident_buf),sizeof(*ident_buf),sizeof(&ident_buf));
    int ret = 0;
    uint8_t cdb[16] = { 0x85,0x08,0x0E,0,0,0,0x01,0,0,0,0,0,0,0xE0,0xEC,0 }; //this will send a SAT ident command to the ata drive and allow us to read the ident info
    uint8_t ident_buf[512];
    memset(ident_buf,0,sizeof(ident_buf));//clear the buffer
    ret = send_cdb(device, ident_buf, sizeof(ident_buf), cdb, 16, XFER_DATA_IN); //should make a function called send_cdb and then check a buffer to see if word 217 shows us that we have an SSD
    if(ret == 0)
    {        
        uint16_t * ident_word = (uint16_t * )&ident_buf[0];
		*maxLBA = 0;
        if(ident_word[217] == 0x0001)
        {
            *isSSD = 1;
        }
        //get the max lba from the identify device information. Check the 28bit address first, then check the 48bit address.
        //checked this against STX v03_12_06CORE and is returns the same value.
		*maxLBA = ((uint64_t)ident_word[61] << 16) | ident_word[60];
        if( *maxLBA == 0x0FFFFFFF && (ident_word[83] & 0x0200) > 0)//check if the 28bit address is filled with F's and that the drive supports the 48bit address feature set before we go adjust the max lba from the other location in the identify information
        {
            *maxLBA = ((uint64_t)ident_word[103]<<48) | ((uint64_t)ident_word[102]<<32) | ((uint64_t)ident_word[101]<<16) | ((uint64_t)ident_word[100]);
        }
        *maxLBA = *maxLBA - 1;
    }
    return ret;
}

// \fn ata_read_log_ext_cmd(DEVICE * device, uint8_t log_address, uint16_t page_number, uint8_t * pData, uint32_t data_size);
// \brief Function to send a ATA Spec read_log_ext. The function only get pData worth of data
// \param device file descriptor
// \param uint8_t log_address to be read as described in A.2 of ATA Spec. 
// \param uint16_t page_number specifies the first log page to be read from the log_address
// \param uint8_t * pData data buffer to be filled
// \param uint32_t data_size size of the buffer to be filled. 
// \return 0 == success, 
//         -2 == pData NULL
//         -3 == data_size is < 512
//         -4 == data_size is not 512 boundry
int ata_read_log_ext_cmd(DEVICE * device, uint8_t log_address, uint16_t page_number, \
                                                                uint8_t * pData, uint32_t data_size)
{
    int ret = 0;
    ATA_PT_CMD_OPTS ata_cmd_opts;
    
    //zap it
    memset(&ata_cmd_opts,0,sizeof(ata_cmd_opts));

    if(pData == NULL)
    {
        return -2;
    }
    else if (data_size < DRIVE_SEC_SIZE)
    {
        return -3;
    }
    // Must be at 512 boundry
    else if (data_size % DRIVE_SEC_SIZE)
    {
        return -4;
    }
    
    ata_cmd_opts.cmd_direction = XFER_DATA_IN; 
    ata_cmd_opts.cmd_protocol = ATA_PROTOCOL_PIO;
    ata_cmd_opts.cmd_type = ATA_CMD_TYPE_EXTENDED_TASKFILE;
    ata_cmd_opts.pData = pData;
    ata_cmd_opts.data_size = data_size;
    
    // Will the pData be the correct size for the log?
    ata_cmd_opts.tfr.CommandStatus = ATA_READ_LOG_EXT;
    ata_cmd_opts.tfr.LbaLow = log_address;
    ata_cmd_opts.tfr.LbaMid = (page_number & 0x00FF);
    ata_cmd_opts.tfr.LbaHi = 0; //Reserved
    ata_cmd_opts.tfr.LbaLow48 = 0; // Reserved
    ata_cmd_opts.tfr.LbaMid48 = page_number >> 8; 
    ata_cmd_opts.tfr.LbaHi48 = 0; //Reserved
    ata_cmd_opts.tfr.SectorCount = data_size / DRIVE_SEC_SIZE;

    ata_cmd_opts.tfr.ErrorFeature = 0; // ?? ATA Spec says Log spcecific 

    ata_cmd_opts.tfr.DeviceHead = 0x40;
    ata_cmd_opts.tfr.DeviceControl = 0;

    ret = ata_pt_cmd(device, &ata_cmd_opts);

    if (ret == 0)
    {
        if (ata_cmd_opts.rtfr.status != 0x50)
        {
            printf(" ATA Status 0x%x, ATA Error 0x%x\n",\
                   ata_cmd_opts.rtfr.status, ata_cmd_opts.rtfr.error);
            ret = -1;
        }
    }
    return ret;
}

// \fn ata_SMART_cmd(DEVICE * device, uint8_t feature, uint8_t * pData, uint32_t data_size);
// \brief Function to send a ATA Spec SMART commands other than SMART R/W Log commands
// \param device file descriptor
// \param uint8_t feature feature register
// \param uint8_t * pData data buffer to be filled
// \param uint32_t data_size size of the buffer to be filled. 
// \return 0 == success, < 0 something went wrong 
int ata_SMART_cmd(DEVICE * device, uint8_t feature, uint8_t * pData, uint32_t data_size)
{
    int ret = 0;
    ATA_PT_CMD_OPTS ata_cmd_opts;
    
    //zap it
    memset(&ata_cmd_opts,0,sizeof(ata_cmd_opts));

    switch (feature)
    {
    case ATA_SMART_RDATTR_THRESH:
    case ATA_SMART_READ_DATA:
        ata_cmd_opts.cmd_direction = XFER_DATA_IN;
        break;
    case ATA_SMART_SW_AUTOSVAE:
    case ATA_SMART_ENABLE:
    case ATA_SMART_EXEC_OFFLINE_IMM:
    case ATA_SMART_RTSMART:
    default:
        ata_cmd_opts.cmd_direction = XFER_NO_DATA;
        break;
    }

    // just sanity sake
    if (ata_cmd_opts.cmd_direction != XFER_NO_DATA)
    {
        if (pData == NULL) 
        {
            return -2;
        }
        else if (data_size < DRIVE_SEC_SIZE)
        {
            return -3;
        }
    }
     
    ata_cmd_opts.cmd_protocol = ATA_PROTOCOL_PIO;
    ata_cmd_opts.cmd_type = ATA_CMD_TYPE_TASKFILE;
    ata_cmd_opts.pData = pData;
    ata_cmd_opts.data_size = data_size;
    
    ata_cmd_opts.tfr.CommandStatus = ATA_CMD_SMART;
    ata_cmd_opts.tfr.LbaLow = 0;
    ata_cmd_opts.tfr.LbaMid = ATA_SMART_SIG_MID;
    ata_cmd_opts.tfr.LbaHi = ATA_SMART_SIG_HI; 

    ata_cmd_opts.tfr.SectorCount = data_size / DRIVE_SEC_SIZE;

    ata_cmd_opts.tfr.ErrorFeature = feature;

    ata_cmd_opts.tfr.DeviceHead = 0x40;
    ata_cmd_opts.tfr.DeviceControl = 0;

    ret = ata_pt_cmd(device, &ata_cmd_opts);

    if (ret == 0)
    {
        if (ata_cmd_opts.rtfr.status != 0x50)
        {
            printf(" ATA Status 0x%x, ATA Error 0x%x\n",\
                   ata_cmd_opts.rtfr.status, ata_cmd_opts.rtfr.error);
            ret = -1;
        }
    }
    return ret;


}
