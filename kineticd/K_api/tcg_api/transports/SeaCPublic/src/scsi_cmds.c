//
// seagate_cmds.c   Implementation for Seagate cmds functions
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

// \file seagate_cmds.c   Implementation for Seagate command functions
//                     The intention of the file is to be generic & not OS specific

#include <string.h>
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

int get_scsi_sanitize_device_feature(DEVICE * device, DRIVE_INFO * info)
{
	int ret = -1;
	uint32_t k = 0;
	uint8_t opt_buf[512];
	uint16_t service_action = 0; 
	SCSI_REPORT_OP_CODE_CMD_OPT report_cmd_opts = { 0 };
	report_cmd_opts.reporting_opts = 0; // Get all op codes
	report_cmd_opts.req_op_code = SCSI_SANITIZE_CMD;

	memset(opt_buf,0,sizeof(opt_buf));
	report_cmd_opts.pData = opt_buf;
	report_cmd_opts.data_size = sizeof(opt_buf);
	ret = scsi_report_supported_operation_codes(device, report_cmd_opts);
	if(ret >= 0) 
	{
/* 
          printf("REPORT OPERATION CODE: \n");
          for (k = 0; k < 512; ++k)
          {
              printf(" 0x%x ", opt_buf[k]);
              if ((k+1)%8 == 0)
              {
                  printf("\n");
              }
          } 
*/
		// First 4 bytes are data, then the descriptors start.
		// Ref. SPC-4 Table 293, 294
		uint32_t valid_data_len = 
			M_BytesTo4ByteValue(opt_buf[0],opt_buf[1],opt_buf[2],opt_buf[3]);
		k = 4; 
		while (k < valid_data_len)
		{
			if (opt_buf[k] == SCSI_SANITIZE_CMD) 
			{
				info->sanitize_opts.sanitize_cmd_enabled = 1;
				service_action = M_BytesTo2ByteValue(opt_buf[k+2],opt_buf[k+3]);
				switch (service_action) 
				{
				case SCSI_SANITIZE_CMD_OVERWRITE_MODE:
					info->sanitize_opts.over_write = 1;
					break;
				case SCSI_SANITIZE_CMD_BLOCK_ERASE_MODE:
					info->sanitize_opts.block_erase = 1;
					break;
				case SCSI_SANITIZE_CMD_CRYPTO_ERASE_MODE:
					info->sanitize_opts.crypto = 1;
					break;
				case SCSI_SANITIZE_CMD_EXIT_FAIL_MODE:
					info->sanitize_opts.exit_fail_mode = 1;
					break;
				default:
					printf("SCSI_REPORT_SUPPORTED_OP_CODES: cmd %Xh invalid SERVICE ACTION %Xh\n",\
							opt_buf[k], service_action);
					break;
				};
			}
			// Next supported op_code index
			if (opt_buf[k+5] & SCSI_CTDP_BIT_SET) 
			{
				// Timeout descripter is there. 
				k = k + 20;
			}
			else
			{
				k = k + 8;
			}
		}
	}

	return ret;
}

// \fn scsi_security_protocol_in(DEVICE device, SCSI_SECURITY_PROTOCOL_IN_OPT cmd_opts)
// \brief Function to send SECURITY PROTOCOL IN command
// \param device file descriptor
// \parm  cmd_opts command options to security protocol in cmd opts. 
int scsi_security_protocol_in(DEVICE * device, SCSI_SECURITY_PROTOCOL_IN_OPT cmd_opts)
{    
    uint8_t cdb[CDB_LEN_12] = { 0 };        
    SCSI_IO_CTX scsi_io_ctx;    
    int ret = 0;
    
    memset(&cdb, 0, sizeof(cdb));

    if ( (cmd_opts.pData != NULL) && (cmd_opts.data_size) )
    {
        cdb[OPERATION_CODE] = SCSI_SECURITY_PROTOCOL_IN;
        cdb[1] = cmd_opts.security_protocol; 
        cdb[2] = cmd_opts.spSpecificMSB;
        cdb[3] = cmd_opts.spSpecificLSB;
        cdb[4] = 0x80;     
        cdb[9] = 1;

        memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
        // Set up the CTX
        scsi_io_ctx.device = device;
        scsi_io_ctx.pcdb = cdb;
        scsi_io_ctx.cdb_len = sizeof(cdb);
        scsi_io_ctx.direction = XFER_DATA_IN;
        scsi_io_ctx.pdata = cmd_opts.pData;
        scsi_io_ctx.data_len = cmd_opts.data_size;
        scsi_io_ctx.verbose = 0; /* 1 to enable */

        ret = send_io(&scsi_io_ctx);

        // Check the return sense code 
    }
    else
    {
        ret = -2;
        printf("%s: ERROR Invalid cmd data parameters\n",__FUNCTION__);
    }

    /*printf("<-- %s\n",__FUNCTION__);*/
    return ret;
}

// \fn scsi_security_protocol_out(DEVICE device, SCSI_SECURITY_PROTOCOL_OUT_OPT cmd_opts)
// \brief Function to send SECURITY PROTOCOL OUT command
// \param device file descriptor
// \parm  cmd_opts command options to security protocol in cmd opts. 
int scsi_security_protocol_out(DEVICE * device, SCSI_SECURITY_PROTOCOL_OUT_OPT cmd_opts)
{    
    uint8_t cdb[CDB_LEN_12] = { 0 };        
    SCSI_IO_CTX scsi_io_ctx;    
    int ret = 0;

    memset(&cdb, 0, sizeof(cdb));
          
    if ( (cmd_opts.pData != NULL) && (cmd_opts.data_size) )
    {
        cdb[OPERATION_CODE] = SCSI_SECURITY_PROTOCOL_OUT;
        cdb[1] = cmd_opts.security_protocol; 
        cdb[2] = cmd_opts.spSpecificMSB;
        cdb[3] = cmd_opts.spSpecificLSB;
        cdb[4] = 0x80;
        cdb[9] = 1;

        memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
        // Set up the CTX
        scsi_io_ctx.device = device;
        scsi_io_ctx.pcdb = cdb;
        scsi_io_ctx.cdb_len = sizeof(cdb);
        scsi_io_ctx.direction = XFER_DATA_OUT;
        scsi_io_ctx.pdata = cmd_opts.pData;
        scsi_io_ctx.data_len = cmd_opts.data_size;
        scsi_io_ctx.verbose = 0; /* 1 to enable */

        ret = send_io(&scsi_io_ctx);

        // Check the return sense code 
    }
    else
    {
        ret = -2;
        printf("%s: ERROR Invalid cmd data parameters\n",__FUNCTION__);
    }

    /*printf("<-- %s\n",__FUNCTION__);*/
    return ret;
}

// \fn scsi_report_supported_operation_codes(DEVICE device, SCSI_REPORT_OP_CODE_CMD_OPT cmd_opts)
// \brief Function to send a
// \param device file descriptor
// \parm  cmd_opts command options to report scsi operation codes. 
int scsi_report_supported_operation_codes(DEVICE * device, SCSI_REPORT_OP_CODE_CMD_OPT cmd_opts)
{
//    printf("--> %s\n",__FUNCTION__);
    int ret=0;
    uint8_t cdb[CDB_LEN_12] = { 0 };
    SCSI_IO_CTX scsi_io_ctx;    
/*  
    printf("%s: reporting_opts=0x%x, req_op=0x%x, req_service=0x%x, pdata=0x%x, data_size=%d[0x%x]\n",\
        __FUNCTION__, cmd_opts.reporting_opts, cmd_opts.req_op_code, cmd_opts.req_service_action,\
        (unsigned int)cmd_opts.pData, cmd_opts.data_size, cmd_opts.data_size );
*/

    if ( (cmd_opts.pData != NULL) && (cmd_opts.data_size) )
    {
        cdb[OPERATION_CODE] = SCSI_REPORT_SUPPORTED_OP_CODES;
        cdb[1] = 0x0C; // This is always 0x0C per SPC spec 
        cdb[2] = cmd_opts.rctd ? SCSI_RCTD_BIT_SET : 0;
        cdb[2] |= (cmd_opts.reporting_opts & 0x07); //bit 0,1,2 only valid
        cdb[3] = cmd_opts.req_op_code;
        cdb[4] = M_Byte1(cmd_opts.req_service_action);
        cdb[5] = M_Byte0(cmd_opts.req_service_action);
        cdb[6] = M_Byte3(cmd_opts.data_size);
        cdb[7] = M_Byte2(cmd_opts.data_size);
        cdb[8] = M_Byte1(cmd_opts.data_size);
        cdb[9] = M_Byte0(cmd_opts.data_size);

        memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
        // Set up the CTX
        scsi_io_ctx.device = device;
        scsi_io_ctx.pcdb = cdb;
        scsi_io_ctx.cdb_len = sizeof(cdb);
        scsi_io_ctx.direction = XFER_DATA_IN;
        scsi_io_ctx.pdata = cmd_opts.pData;
        scsi_io_ctx.data_len = cmd_opts.data_size;
        scsi_io_ctx.verbose = 0;

        ret = send_io(&scsi_io_ctx);

        // Check the return sense code 
    }
    else
    {
        ret = -2;
        printf("%s: ERROR Invalid cmd data parameters\n",__FUNCTION__);
    }

//    printf("<-- %s\n",__FUNCTION__);
    return ret;
}


// \fn scsi_sanitize_cmd(DEVICE device, bool immed_bit, bool, ause_bit, unsigned char service_action)
// \brief Function to send a SCSI Sanitize command
// \param device file descriptor
// \param SCSI_SANITIZE_CMD_OPT struct containing the sanitize command options.  
int scsi_sanitize_cmd(DEVICE * device, SCSI_SANITIZE_CMD_OPT cmd_opts)
{
    int ret=0;
    int8_t mode = -1;
    uint8_t cdb[CDB_LEN_10] = { 0 };
    SCSI_IO_CTX scsi_io_ctx;
    memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));

/* 
    printf("--> %s\n",__FUNCTION__);
    printf("%s: immed=%d ause=%d service_action=%d\n",\
         __FUNCTION__,cmd_opts.immed,cmd_opts.ause,cmd_opts.service_action);
*/

    switch(cmd_opts.service_action)
    {
    case SCSI_SANITIZE_CMD_OVERWRITE_MODE:
    case SCSI_SANITIZE_CMD_EXIT_FAIL_MODE:
    case SCSI_SANITIZE_CMD_BLOCK_ERASE_MODE:
        printf("%s: Unsupported SERVICE ACTION=%d\n",__FUNCTION__,cmd_opts.service_action);
        ret = -2;
        break;
    case SCSI_SANITIZE_CMD_CRYPTO_ERASE_MODE:
        mode = SCSI_SANITIZE_CMD_CRYPTO_ERASE_MODE;
        scsi_io_ctx.direction = XFER_NO_DATA ;
        break;
    default:
        printf("%s: Invalid SERVICE ACTION=%d\n",__FUNCTION__,cmd_opts.service_action);
        ret = -2;
        break;
    }
    if(mode >= 0)
    {
        //Issue the Sanitize command
        cdb[OPERATION_CODE] = SCSI_SANITIZE_CMD;
        if(cmd_opts.immed)
        {
            cdb[1] |= SCSI_SANITIZE_CMD_IMMED_BIT_SET;
        }
        if(cmd_opts.ause)
        {
            cdb[1] |= SCSI_SANITIZE_CMD_AUSE_BIT_SET;
        }
        cdb[1] |= (uint8_t)mode; // Set the sanitize mode. 
        // Set up the CTX
        scsi_io_ctx.device = device;
        scsi_io_ctx.pcdb = cdb;
        scsi_io_ctx.cdb_len = sizeof(cdb);
        scsi_io_ctx.verbose = 0;
        ret = send_io(&scsi_io_ctx);
    }
//  printf("<-- %s\n",__FUNCTION__);
    return ret;
}

// \fn scsi_request_sense_cmd(DEVICE device, uint8_t * pdata, uint8_t data_size)
// \brief Function to send request scsi sense data
// \param device file descriptor
// \param uint8_t desc_bit Fixed or Descripter format. 
// \param pdata data in the .  
// \param data_size size of the data requested. 252 should be the default
int scsi_request_sense_cmd(DEVICE * device, uint8_t desc_bit, uint8_t * pdata, uint8_t data_size)
{
    int ret=0;
    uint8_t cdb[CDB_LEN_6] = { 0 };
    SCSI_IO_CTX scsi_io_ctx;
//  printf("--> %s\n",__FUNCTION__);
//  printf("%s: pdata=0x%x, data_size=%d\n",__FUNCTION__,(unsigned int)pdata,data_size);
    if ( (pdata != NULL) && (data_size) )
    {
        memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
        // Set up the CDB. 
        cdb[OPERATION_CODE] = 0x03; // REQUEST_SENSE;
        if (desc_bit)
        {
            cdb[1] |= SCSI_REQUEST_SENSE_DESC_BIT_SET;
        }
        cdb[4] = data_size;

        // Set up the CTX
        scsi_io_ctx.device = device;
        scsi_io_ctx.pcdb = cdb;
        scsi_io_ctx.cdb_len = sizeof(cdb);
        scsi_io_ctx.direction = XFER_DATA_IN;
        scsi_io_ctx.pdata = pdata;
        scsi_io_ctx.data_len = data_size;
        scsi_io_ctx.verbose = 0;

        ret = send_io(&scsi_io_ctx);

        // Check the return sense code 
    }
    else
    {
        ret = -2;
        printf("%s: ERROR Invalid cmd data parameters\n",__FUNCTION__);
    }

//  printf("<-- %s\n",__FUNCTION__);
    return ret;
}

