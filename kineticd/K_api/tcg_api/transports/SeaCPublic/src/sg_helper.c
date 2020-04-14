//
// sg_helper.c   Implementation for sg_helper functions
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
#include <time.h>
#include <unistd.h> // for close
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "sg_helper.h"
#include "scsi_helper_func.h"

void decipher_masked_status(unsigned char masked_status)
{
    if (CHECK_CONDITION == masked_status)
        printf("CHECK CONDITION\n");
    else if (BUSY == masked_status)
        printf("BUSY\n");
    else if (COMMAND_TERMINATED == masked_status)
        printf("COMMAND TERMINATED\n");
    else if (QUEUE_FULL == masked_status)
        printf("QUEUE FULL\n");
}

// Local helper functions for debugging
void print_io_hdr( sg_io_hdr_t * pIo)
{
    time_t time_now;
    time_now = time(NULL);
    printf("\n%s: %s---------------------------------\n", __FUNCTION__, ctime(&time_now));
    printf ("type int interface_id %d\n",pIo->interface_id);           /* [i] 'S' (required) */
    printf ("type int  dxfer_direction %d\n", pIo->dxfer_direction);        /* [i] */
    printf ("type unsigned char cmd_len 0x%x\n", pIo->cmd_len);      /* [i] */
    printf ("type unsigned char mx_sb_len 0x%x\n", pIo->mx_sb_len);    /* [i] */
    printf ("type unsigned short iovec_count 0x%x\n", pIo->iovec_count); /* [i] */
    printf ("type unsigned int dxfer_len %d\n", pIo->dxfer_len);     /* [i] */
    printf ("type void * dxferp %p\n", (unsigned int *) pIo->dxferp);              /* [i], [*io] */
    printf ("type unsigned char * cmdp %p\n", (unsigned int *) pIo->cmdp);       /* [i], [*i]  */
    printf ("type unsigned char * sbp %p\n", (unsigned int * ) pIo->sbp);        /* [i], [*o]  */
    printf ("type unsigned int timeout %d\n", pIo->timeout);       /* [i] unit: millisecs */
    printf ("type unsigned int flags 0x%x\n", pIo->flags);         /* [i] */
    printf ("type int pack_id %d\n", pIo->pack_id);                /* [i->o] */
    printf ("type void * usr_ptr %p\n", (unsigned int *) pIo->usr_ptr);             /* [i->o] */
    printf ("type unsigned char status 0x%x\n", pIo->status);       /* [o] */
    printf ("type unsigned char masked_status 0x%x\n", pIo->masked_status);/* [o] */
    printf ("type unsigned char msg_status 0x%x\n", pIo->msg_status);   /* [o] */
    printf ("type unsigned char sb_len_wr 0x%x\n", pIo->sb_len_wr);    /* [o] */
    printf ("type unsigned short host_status 0x%x\n", pIo->host_status); /* [o] */
    printf ("type unsigned short driver_status 0x%x\n", pIo->driver_status);/* [o] */
    printf ("type int resid %d\n", pIo->resid);                  /* [o] */
    printf ("type unsigned int duration %d\n", pIo->duration);      /* [o] */
    printf ("type unsigned int info 0x%x\n", pIo->info);          /* [o] */
    printf ("-----------------------------------------\n");
}

int get_device(char * filename, DEVICE * device)
{
    //DEVICE device = { 0 };
    int ret = 0, k = 0, this_drive_type=0;

    // Note: We are opening a READ/Write flag
    if ((device->fd = open(filename, O_RDWR)) < 0) 
    {
        perror("get_fd");
        device->fd = errno;
    }

    if (device->fd >= 0) 
    {
        // Check we have a valid device by trying an ioctl
        // From http://tldp.org/HOWTO/SCSI-Generic-HOWTO/pexample.html
        if ((ioctl(device->fd, SG_GET_VERSION_NUM, &k) < 0) || (k < 30000)) {
            printf("%s: SG_GET_VERSION_NUM on %s failed \n", __FUNCTION__, filename);
            perror("SG_GET_VERSION_NUM");
            close(device->fd);
        }
        else
        {
            // Fill in all the device info. 
            ret = fill_in_device_info(device);

            // 0 - means ATA. 1 - means SCSI
            this_drive_type = memcmp(device->drive_info.T10_vendor_ident, "ATA", 3);
            if(this_drive_type != 0)
                device->drive_info.drive_type = SCSI_DRIVE; // default 0 = ATA
        }
    }

    return ret;
}
void get70ATAtfrs(sg_io_hdr_t *io_hdr, SCSI_IO_CTX *scsi_io_ctx)
{
    if((io_hdr->sbp[8] & 0x80) != 0)
    {
#ifdef _DEBUG
        printf("Entended fields are unable to be returned\n");
#endif
        //cannot return the ext fields, but we might get some information about what they were.
        if((io_hdr->sbp[8] & 0x20) != 0)
        {
            //one of the lba ext fields was not equal to zero...lets set them to FF
            scsi_io_ctx->rtfrs.lbaLowExt = 0xFF;
            scsi_io_ctx->rtfrs.lbaMidExt = 0xFF;
            scsi_io_ctx->rtfrs.lbaHiExt  = 0xFF;
        }
        else
        {
            scsi_io_ctx->rtfrs.lbaLowExt = 0;
            scsi_io_ctx->rtfrs.lbaMidExt = 0;
            scsi_io_ctx->rtfrs.lbaHiExt  = 0;
        }
        if((io_hdr->sbp[8] & 0x40) != 0)
        {
           //the sector cnt ext field was not equal to zero...set to 0xFF
           scsi_io_ctx->rtfrs.secCntExt = 0xFF;
        }
        else
        {
           scsi_io_ctx->rtfrs.secCntExt = 0x00;
        }
    }
    else
    {
#ifdef _DEBUG
        printf("no extended data\n");
#endif
        scsi_io_ctx->rtfrs.secCntExt = 0x00;
        scsi_io_ctx->rtfrs.lbaLowExt = 0x00;
        scsi_io_ctx->rtfrs.lbaMidExt = 0x00;
        scsi_io_ctx->rtfrs.lbaHiExt  = 0x00;
    }
    scsi_io_ctx->rtfrs.error     = io_hdr->sbp[3];
    scsi_io_ctx->rtfrs.status    = io_hdr->sbp[4];
    scsi_io_ctx->rtfrs.device    = io_hdr->sbp[5];
    scsi_io_ctx->rtfrs.secCnt    = io_hdr->sbp[6];
    scsi_io_ctx->rtfrs.lbaHi     = io_hdr->sbp[9];
    scsi_io_ctx->rtfrs.lbaMid    = io_hdr->sbp[10];
    scsi_io_ctx->rtfrs.lbaLow    = io_hdr->sbp[11];
}
void getATA_TFRs(sg_io_hdr_t *io_hdr, SCSI_IO_CTX *scsi_io_ctx)
{
    //ATA return tfrs
    if( (io_hdr->sbp[0] & 0x7F) == 0x72 && (scsi_io_ctx->pcdb[0] == 0x85 || scsi_io_ctx->pcdb[0] == 0xA1) && io_hdr->sbp[8] == 0x09 && io_hdr->sbp[9] == 0x0C )
    {
#ifdef _DEBUG
        printf("0x72 checking ATA TFRS...\n");
#endif
        scsi_io_ctx->rtfrs.error     = io_hdr->sbp[11];
        scsi_io_ctx->rtfrs.secCntExt = io_hdr->sbp[12];
        scsi_io_ctx->rtfrs.secCnt    = io_hdr->sbp[13];
        scsi_io_ctx->rtfrs.lbaLowExt = io_hdr->sbp[14];
        scsi_io_ctx->rtfrs.lbaLow    = io_hdr->sbp[15];
        scsi_io_ctx->rtfrs.lbaMidExt = io_hdr->sbp[16];
        scsi_io_ctx->rtfrs.lbaMid    = io_hdr->sbp[17];
        scsi_io_ctx->rtfrs.lbaHiExt  = io_hdr->sbp[18];
        scsi_io_ctx->rtfrs.lbaHi     = io_hdr->sbp[19];
        scsi_io_ctx->rtfrs.device    = io_hdr->sbp[20];
        scsi_io_ctx->rtfrs.status    = io_hdr->sbp[21];
    }
    else if( (io_hdr->sbp[0] & 0x7F) == 0x70 && (scsi_io_ctx->pcdb[0] == 0x85 || scsi_io_ctx->pcdb[0] == 0xA1) )
    {
#ifdef _DEBUG
        printf("0x70 checking ATA TFRS...\n");
#endif
        if( (io_hdr->sbp[8] & 0x0F) != 0 )//a valid log will always be greater than 0 in value according to the spec
        {
            uint8_t logSenseATA[10] = { 0x4D, 0x00, 0x16, 0x00, 0x00, 0x00, (0x00 & (io_hdr->sbp[8] & 0x0F)), 0x00/*allocLen1*/, 0x0B/*allocLen2*/, 0x00 };
            int returnATA = 0;
            //read the log that contains the ATA rftr descriptor
#ifdef _DEBUG
            printf("Sending read log\n");
#endif
            scsi_io_ctx->pcdb = logSenseATA;
            returnATA = ioctl(scsi_io_ctx->device->fd, SG_IO, &io_hdr);
            if (returnATA < 0)
                perror("send_io");
            //print_io_hdr(&io_hdr);
            //now check the results and copy back the ATA descriptor
            if( io_hdr->sbp[8] == 0x16 )//make sure we were returned the correct log page and that we have valid information
            {
#ifdef _DEBUG
                printf("Copying back log information\n");
#endif
                scsi_io_ctx->rtfrs.error     = io_hdr->sbp[11];
                scsi_io_ctx->rtfrs.secCntExt = io_hdr->sbp[12];
                scsi_io_ctx->rtfrs.secCnt    = io_hdr->sbp[13];
                scsi_io_ctx->rtfrs.lbaLowExt = io_hdr->sbp[14];
                scsi_io_ctx->rtfrs.lbaLow    = io_hdr->sbp[15];
                scsi_io_ctx->rtfrs.lbaMidExt = io_hdr->sbp[16];
                scsi_io_ctx->rtfrs.lbaMid    = io_hdr->sbp[17];
                scsi_io_ctx->rtfrs.lbaHiExt  = io_hdr->sbp[18];
                scsi_io_ctx->rtfrs.lbaHi     = io_hdr->sbp[19];
                scsi_io_ctx->rtfrs.device    = io_hdr->sbp[20];
                scsi_io_ctx->rtfrs.status    = io_hdr->sbp[21];
            }
            else
            {
#ifdef _DEBUG
                printf("Invalid log or no log information\n");
#endif
                //we didn't get the correct information or the log doesn't contain any information...check the other method
                get70ATAtfrs(io_hdr, scsi_io_ctx);
            }
        }
        else
        {
            get70ATAtfrs(io_hdr, scsi_io_ctx);
        }
    }

    else
    {
#ifdef _DEBUG
        printf("No need to check for ATA TFRs\n");
#endif
        //Fake it
        scsi_io_ctx->rtfrs.status = 0x50;
    }

}
int send_io(SCSI_IO_CTX * scsi_io_ctx)
{
    sg_io_hdr_t io_hdr;
    uint8_t sense_buffer[32];
    int ret = 0;
    int idx = 0;
    memset(sense_buffer,0,32);
    // Start with zapping the io_hdr
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));

    // Set up the io_hdr 
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = scsi_io_ctx->cdb_len;
    // Use user's sense or local?
    if ( (scsi_io_ctx->sense_sz) && (scsi_io_ctx->psense != NULL) )
    {
        io_hdr.mx_sb_len = scsi_io_ctx->sense_sz;
        io_hdr.sbp = scsi_io_ctx->psense;
    }
    else
    {
        io_hdr.mx_sb_len = sizeof(sense_buffer);
        io_hdr.sbp = (unsigned char *) &sense_buffer;
    }

    switch(scsi_io_ctx->direction)
    {
        case XFER_NO_DATA:
        case SG_DXFER_NONE:
            io_hdr.dxfer_direction = SG_DXFER_NONE;
        break;
        case XFER_DATA_IN:
        case SG_DXFER_FROM_DEV:
            io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        break;
        case XFER_DATA_OUT:
        case SG_DXFER_TO_DEV:
            io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
        break;
        case SG_DXFER_TO_FROM_DEV:
            io_hdr.dxfer_direction = SG_DXFER_TO_FROM_DEV;
        break;
        //case SG_DXFER_UNKNOWN:
            //io_hdr.dxfer_direction = SG_DXFER_UNKNOWN;
        //break;
        default:
            printf("%s Didn't understand direction\n", __FUNCTION__);
            return -1;
    }

    io_hdr.dxfer_len = scsi_io_ctx->data_len;
    io_hdr.dxferp = scsi_io_ctx->pdata;
    io_hdr.cmdp = scsi_io_ctx->pcdb;
    io_hdr.timeout = 15000; // \todo: use the scsi_io_ctx timeout value. 

    if (scsi_io_ctx->verbose)
    {
        printf("cdb:\n");
        if (scsi_io_ctx->pcdb[0] == SAT_ATA_16) 
        {
            printf(" OP_CD ML/PRO FLAG XFEA  FEAT  XSC   SCT   XLBA   LBA  XMID  MID"
                   "   XHI    HI   DEV  CMD   CTRL\n");
        }
        for (idx = 0; idx < scsi_io_ctx->cdb_len; idx++)
        {
            printf(" 0x%02X ",scsi_io_ctx->pcdb[idx]);
        }
        printf("\n");
    }

    // \revisit: should this be FF or something invalid than 0?
    scsi_io_ctx->return_status.format = 0xFF;
    scsi_io_ctx->return_status.sense_key = 0;
    scsi_io_ctx->return_status.acq = 0;
    scsi_io_ctx->return_status.ascq = 0; 
    //print_io_hdr(&io_hdr);
    
    ret = ioctl(scsi_io_ctx->device->fd, SG_IO, &io_hdr);
    if (ret < 0)
    {
        perror("send_io");
    }

    //print_io_hdr(&io_hdr);

    if (io_hdr.sb_len_wr)
    {
        scsi_io_ctx->return_status.format  = io_hdr.sbp[0]; 
        scsi_io_ctx->return_status.sense_key = get_sense_key(io_hdr.sbp);
        get_acq_ascq(io_hdr.sbp,&scsi_io_ctx->return_status.acq, &scsi_io_ctx->return_status.ascq );
    }

    //ATA TFRs will only be copied on cdb[0] == 0x85 or cdb[0] == 0xA1
    getATA_TFRs(&io_hdr, scsi_io_ctx);

    // \todo shouldn't this be done at a higher level?
    if ( ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) || // check info
        (io_hdr.masked_status != 0x00) ||                  // check status(0 if ioctl success)
        (io_hdr.msg_status != 0x00) ||                     // check message status
        (io_hdr.host_status != 0x00) ||                    // check host status
        (io_hdr.driver_status != 0x00) )                   // check driver status
        {
            if (scsi_io_ctx->verbose)
            {
                printf(" info 0x%x\n masked_status 0x%x\n msg_status 0x%x\n host_status 0x%x\n driver_status 0x%x\n",\
                       io_hdr.info, io_hdr.masked_status, io_hdr.msg_status, io_hdr.host_status,\
                       io_hdr.driver_status);
        
            
                decipher_masked_status (io_hdr.masked_status);

                //if (io_hdr.driver_status & SG_ERR_DRIVER_SENSE)
                if ( (io_hdr.driver_status & 0x08) && (io_hdr.sb_len_wr) )
                {
                    decipher_and_print_sense_key(scsi_io_ctx->return_status.sense_key);
                    decipher_and_print_acq_ascq(scsi_io_ctx->return_status.acq,scsi_io_ctx->return_status.ascq);
                    print_sense_buffer((uint8_t *)io_hdr.sbp, io_hdr.sb_len_wr);
                }            
            }
        }

    //printf("%s: Final ret %d\n",__FUNCTION__, ret);

    return ret;
}
