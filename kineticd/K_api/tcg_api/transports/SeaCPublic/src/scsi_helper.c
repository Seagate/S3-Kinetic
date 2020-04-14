//
// scsi_helper.c   Implementation for helper functions for scsi
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

#include <stdio.h>
#include <stdint.h>
#include "scsi_helper_func.h"
#include "common.h"
#ifdef __linux__
#include "sg_helper.h"
#elif _WIN32
#include "win_helper.h"
#endif

void remove_spaces(char* source)
{
  char* i = source;
  char* j = source;
  while(*j != 0)
  {
    *i = *j++;
    if(*i != ' ')
      i++;
  }
  *i = 0;
}

void print_sense_buffer(uint8_t * pbuf, int size)
{
    int i = 0;
    if(size > 0) 
    {
        printf(" OFFSET\tVALUE\n");
        for (i=0; i<size; i++)
        {
            printf(" [0x%x]\t0x%x\n",i,pbuf[i]);
        }
    }
    else
    {
        printf(" NO VALID SENSE DATA [%d]\n", size);
    }
}

void decipher_and_print_sense_key(uint8_t sense_key)
{
    if (sense_key ==  0x00)
        printf("NO SENSE\n");
    else if (sense_key ==  0x01)
        printf("RECOVERED ERROR\n");
    else if (sense_key ==  0x02)
        printf("NOT READY\n");
    else if (sense_key ==  0x03)
        printf("MEDIUM ERROR\n");
    else if (sense_key ==  0x04)
        printf("HARDWARE ERROR\n");
    else if (sense_key ==  0x05)
        printf("ILLEGAL REQUEST\n");
    else if (sense_key ==  0x06)
        printf("UNIT ATTENTION\n");
    else if (sense_key ==  0x07)
        printf("DATA PROTECT\n");
    else if (sense_key ==  0x08)
        printf("BLACK CHECK\n");
    else if (sense_key ==  0x09)
        printf("VENDOR SPECIFIC\n");
    else if (sense_key ==  0x0A)
        printf("COPY ABORTED\n");
    else if (sense_key ==  0x0B)
        printf("ABORTED COMMAND\n");
    else
        printf("UNKNOWN SENSE KEY 0x%x\n",sense_key);
}

void decipher_and_print_acq_ascq(uint8_t acq, uint8_t ascq)
{
    printf("ACQ: 0x%x\tASCQ: 0x%x\n",acq,ascq);
    if ((acq == 0x20) && (ascq == 0x00))
        printf("INVALID COMMAND OPERATION CODE\n");
    else if ((acq == 0x24) && (ascq == 0x00))
        printf("INVALID FIELD IN CDB\n");
}

void get_acq_ascq(uint8_t * pbuf, uint8_t * acq, uint8_t * ascq)
{
    uint8_t format = pbuf[0]& 0x7F; //Stripping the last bit. 

    switch(format) 
    {
    case 0x70:
    case 0x71:
        *acq = pbuf[12];
        *ascq = pbuf[13];
        break;
    case 0x72:
    case 0x73:
        *acq = pbuf[2];
        *ascq = pbuf[3];
        break;
    case 0x7F:
        
        break;
    default:
        
        break;
    }
}

uint8_t get_sense_key(uint8_t * pbuf)
{
    uint8_t sense_key = 0xFF;
    uint8_t format = pbuf[0]& 0x7F; //Stripping the last bit.

    switch(format)
    {
    case 0x70:
    case 0x71:
        sense_key = pbuf[2] & 0x0F;
        break;
    case 0x72:
    case 0x73:
        sense_key = pbuf[1] & 0x0F;
        break;
    case 0x7F:
        printf("\n VENDOR SPECIFIC SENSE DATA\n");
        break;
    default:
        printf("\n UNKNOWN SENSE DATA \n");
        break;
    }

    return sense_key;
}

int send_inq(DEVICE * device, uint8_t * pdata, uint32_t data_len , uint8_t page_code, uint8_t evpd)
{
    int ret = 0;
								//INQUIRY
    uint8_t inq_cdb[CDB_LEN_6] = {0x12, 0, 0, 0, (uint8_t)data_len, 0};
    SCSI_IO_CTX scsi_io_ctx;

    if(data_len < INQ_RETURN_DATA_LENGTH) 
    {
        printf("%s: Error INQ command requires %d byte size return data buffer %d size given\n",\
               __FUNCTION__, INQ_RETURN_DATA_LENGTH, data_len);
        return -1;
    }

    if(evpd)
    {
        inq_cdb[1] |= 0x01; //Set the EVPD bit        
    }
	// Set the page code
	inq_cdb[2] = page_code;

    memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));

    scsi_io_ctx.device = device;
    scsi_io_ctx.pcdb = inq_cdb;
    scsi_io_ctx.cdb_len = CDB_LEN_6;
    scsi_io_ctx.direction = XFER_DATA_IN;
    scsi_io_ctx.pdata = pdata;
    scsi_io_ctx.data_len = data_len;
	 scsi_io_ctx.timeout = 5000;

    ret = send_io(&scsi_io_ctx);

	if(scsi_io_ctx.return_status.status_scsi)
	{
		// Most likly a check condition. 
		ret = -1;
	}

    return ret; 
}

int checkGetATATFRs(uint8_t * cdb)
{
    int ret = 0;
    if( cdb[0] == 0xA1 )
    {
        switch (cdb[9])
        {
        case ATA_SMART:
            ret = 1;
            break;
        default:
            break;
        }
    }
    else if( cdb[0] == 0x85 )
    {
        switch (cdb[14])
        {
        case ATA_SMART:
            ret = 1;
            break;
        default:
            break;
        }
    }
    return ret;
}
int send_cdb(DEVICE * device, uint8_t * pdata, uint32_t data_len , uint8_t * cdb, uint8_t cdb_len, uint8_t data_direction)
{
    int ret = 0;
#ifdef _DEBUG
    int i = 0;
#endif
    SCSI_IO_CTX scsi_io_ctx = { 0 };

    if(data_len < 512) //this may need to be changed to something else.
    {
        printf("%s: Error command requires %d byte size return data buffer %d size given\n",
               __FUNCTION__, INQ_RETURN_DATA_LENGTH, data_len);
        return -1;
    }

    memset(&scsi_io_ctx,0,sizeof(SCSI_IO_CTX));
#ifdef _DEBUG
    printf("Sending cdb: \n");
    for(i = 0; i<cdb_len; i++ )
    {
        printf("cdb[%d] : 0x%02x\n",i,cdb[i]);
    }
#endif

    scsi_io_ctx.device = device;
    scsi_io_ctx.pcdb = cdb;
    scsi_io_ctx.cdb_len = cdb_len;
    scsi_io_ctx.direction = data_direction;
    scsi_io_ctx.pdata = pdata;
    scsi_io_ctx.data_len = data_len;
	scsi_io_ctx.timeout = 5000;

    ret = send_io(&scsi_io_ctx);

    if( data_direction == XFER_NO_DATA && checkGetATATFRs(cdb) )
    {
        //now take the sense data, and put it into the ATA_RETURN_TFRS
        pdata[0]  = scsi_io_ctx.rtfrs.error;
        pdata[1]  = scsi_io_ctx.rtfrs.secCntExt;
        pdata[2]  = scsi_io_ctx.rtfrs.secCnt;
        pdata[3]  = scsi_io_ctx.rtfrs.lbaLowExt;
        pdata[4]  = scsi_io_ctx.rtfrs.lbaLow;
        pdata[5]  = scsi_io_ctx.rtfrs.lbaMidExt;
        pdata[6]  = scsi_io_ctx.rtfrs.lbaMid;
        pdata[7]  = scsi_io_ctx.rtfrs.lbaHiExt;
        pdata[8]  = scsi_io_ctx.rtfrs.lbaHi;
        pdata[9]  = scsi_io_ctx.rtfrs.device;
        pdata[10] = scsi_io_ctx.rtfrs.status;
#ifdef _DEBUG
        for(i = 0; i < 11; i++){
            printf("pdata[%d]: 0x%02x\n",i,pdata[i]);
        }
#endif
    }

	if(scsi_io_ctx.return_status.status_scsi)
	{
		// Most likly a check condition. 
		ret = -1;
	}

    return ret; 
}
// \fn copy_inquiry_data(unsigned char * pbuf, DRIVE_INFO * info)
// \brief copy in the necessary data to our struct from INQ data. 
void copy_inquiry_data(unsigned char * pbuf, DRIVE_INFO * info)
{
	// \todo: Create a macro to get various stuff out of the inq buffer
	// \todo: The 12 should be SEAGATE_SERIAL_NUM_LEN    
	memcpy(info->T10_vendor_ident,&pbuf[8],8);
	//T10_vendor_ident[8] = '\0';
	memcpy(info->product_identification,&pbuf[16],16);
	//product_identification[16] = '\0';
	memcpy(info->product_revision,&pbuf[32],4);  									
}

// \brief copy the serial number off of 0x80 VPD page data. 
void copy_serial_number(unsigned char * pbuf, DRIVE_INFO * info)
{
	unsigned int k = 0;
	// Fourth byte tells the return data len.
	for (k = 0; k < pbuf[3]; ++k)
	{
		if(pbuf[k+4] != 0x20)
		{
			memcpy(info->serial_number,&pbuf[k+4],12);
			break;
		}
	}
}

// /brief check if the device is sat compliant 
// \returns 1 if it is . 
int is_device_sat_compliant( DEVICE * device, DRIVE_INFO * info)
{
	int ret = -1;
	unsigned char inq_buf[INQ_RETURN_DATA_LENGTH];

	// Lets see if SAT compilent 
	memset(inq_buf,0,sizeof(inq_buf));
	// Geting Serial Number from VPD page 0x89
	ret = send_inq(device, inq_buf, INQ_RETURN_DATA_LENGTH, SAT_VPD_PAGE, 1);
	if(ret >= 0)
	{
		ret = 1;
		#ifdef _DEBUG
		printf(" SAT COMPLIENT \n");
		#endif
        //Moved the following line outside...since it is proven the interface is working
	    info->interface_type = SCSI_INTERFACE;
		if ( inq_buf[SAT_VPD_CMD_CODE_IDX] == 0xEC ) 
		{
			// Only ATA Targets are SAT complient
			info->drive_type = ATA_DRIVE;
		} // else it is an ATAPI target, we leave it to unknown
        else
        {
		#ifdef _DEBUG
		printf(" SAT COMPLIENT but seems like ATAPI\n");
		#endif
        }
	}
	else 
	{
		info->drive_type = SCSI_DRIVE;
	}

	return ret;
}

// \fn fill_in_device_info(DEVICE device)
// \brief Sends a set of INQUIRY commands & fills in the device information 
// \param device device struture
// \returns a negative number if fails
int fill_in_device_info(DEVICE * device)
{
	int ret = -1;
	unsigned char inq_buf[INQ_RETURN_DATA_LENGTH];
#ifdef _DEBUG
	int k = 0;
    int page_len = 0;
#endif 

	//printf("%s -->\n", __FUNCTION__);
	// Zero everything out.
	memset(inq_buf,0,sizeof(inq_buf));
	memset(device->drive_info.serial_number,0,sizeof(device->drive_info.serial_number));
	memset(device->drive_info.T10_vendor_ident,0,sizeof(device->drive_info.T10_vendor_ident));
	memset(device->drive_info.product_identification,0,sizeof(device->drive_info.product_identification));
	memset(device->drive_info.product_revision,0,sizeof(device->drive_info.product_revision));
	ret = send_inq(device, inq_buf, INQ_RETURN_DATA_LENGTH, 0, 0);
	if ( ret == 0 )
	{
		copy_inquiry_data(inq_buf, &device->drive_info);
		memset(inq_buf,0,sizeof(inq_buf));
		// Geting Serial Number from VPD page 0x80
		ret = send_inq(device, inq_buf, INQ_RETURN_DATA_LENGTH, 0x80, 1);
		if (ret < 0)
		{
			printf("Sending INQUIRY VPD 0x80 command to Failed\n");
			ret = -2;
		}
		else
		{
			copy_serial_number(inq_buf, &device->drive_info);
		}
		// Note: Remove this later 
		// Lets see which VPD pages are supported. 
		memset(inq_buf,0,sizeof(inq_buf));        
        ret = send_inq(device, inq_buf, INQ_RETURN_DATA_LENGTH, 0x00, 1);				
#ifdef _DEBUG
		page_len = M_BytesTo2ByteValue(inq_buf[2], inq_buf[3]);
		printf(" %d VPD pages SUPPORED:\n",page_len);		
		for (k = 0; k < page_len; k++)
		{
			printf(" 0x%x\n", inq_buf[4+k]);			
		}
#endif
		if ( ( device->drive_info.interface_type != IDE_INTERFACE ) && 
			 ( device->drive_info.drive_type != RAID_DRIVE ) )
		{
			is_device_sat_compliant(device, &device->drive_info);
		}
	}
	else
	{
		printf("Sending INQUIRY VPD 0x00 command to Failed\n");
		ret = -1;
	}
	
	//printf("%s <--\n", __FUNCTION__);
	return ret; 
}
