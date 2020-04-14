//
// cmds.c   Implementation for generic ATA/SCSI cmds functions
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

// \file cmds.c   Implementation for generic ATA/SCSI functions
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

// \fn get_sanitize_device_feature(DEVICE device)
// \brief Function to find out which of the sanitize feature options are supported, if any
// \param DEVICE device  file descriptor
// \return 0 == success, < 0 something went wrong 
int get_sanitize_device_feature(DEVICE * device)
{    
    int ret = -1;
	unsigned int d = 0;
	//printf("--> %s\n",__FUNCTION__);
    // Lets find out the security modes. 
    if ( device->drive_info.drive_type == SCSI_DRIVE ) 
    {
		ret = get_scsi_sanitize_device_feature(device, &device->drive_info);
    }
    else if ( device->drive_info.drive_type == ATA_DRIVE ) 
    {
		ret = get_ata_sanitize_device_feature(device, &device->drive_info);
    }
	else if ( device->drive_info.drive_type == RAID_DRIVE ) 
	{
		for ( d = 0; d < device->raid_info.number_of_pds; d++ )
		{
			device->raid_info.current_pt_target_id = device->raid_info.physical_drive[d].device_id;
			if ( device->raid_info.physical_drive[d].drive_type == SCSI_DRIVE ) 
			{
				ret = get_scsi_sanitize_device_feature(device, \
												 &device->raid_info.physical_drive[d]);
			}
			else if ( device->raid_info.physical_drive[d].drive_type == ATA_DRIVE ) 
			{
				ret = get_ata_sanitize_device_feature(device, \
												&device->raid_info.physical_drive[d]);
			}
			else
			{
				#ifdef _DEBUG
				printf("For RAID LD Target ID, invalid Drive type for PD Device ID %d\n",\
					   device->drive_info.device_id, device->raid_info.physical_drive[d].device_id);
				#endif 
				ret = -1;
				break;
			}
		}
		device->raid_info.current_pt_target_id = -1;
	}
    //printf("<-- %s\n",__FUNCTION__);
    return ret;
}

//delay for x seconds. I wrote it this way to be C99 compliant...not relying on any special linux, or windows funtions (like sleep())
void delay_sec(unsigned int seconds)
{
    unsigned int endTime = time(0) + seconds;
    while( time(0) < endTime );//loop until the end time.
}


time_t current_time;
char current_time_st[64];
char * current_time_st_ptr = current_time_st;
#ifdef SELFTEST
//return values: 0 = pass, -1 = fail, -2 = DST already running, -3 = Abort? (Don't know if we should add in the ability to abort the DST yet or not)
int run_DST(DEVICE * device, char * pserial_number, int isSATA, int DSTType)
{
    int result = -1;
    uint32_t percentComplete = 0;
    uint32_t status = 0xFF;
    int ret = 0;
    char filename[128]; /* some file names get really long */
    FILE * fp_DST;
    current_time = time(NULL);
	strftime(current_time_st, 64, "%Y-%m-%d__%H_%M_%S", localtime(&current_time));
    sprintf(filename, "%s_DST_%s.log", pserial_number,current_time_st_ptr); 
    if( (fp_DST = fopen(filename, "w")) == NULL )
    {
        printf("%s: Couldn't open file %s\n",__FUNCTION__, filename);
        perror("fopen");
        ret = -1;
    }
    else
    {
        if(isSATA)
        {
            uint8_t DSTSATA[16] = { 0x85,0x08,0x0E,0x00,0xD4,0x00,0x00,0x00,0x00,0x00,0x4F,0x00,0xC2,0x00,0xB0,0x00 };
            uint8_t DSTSTATUS[16] = { 0x85,0x08,0x0E,0x00,0xD0,0x00,0x01,0x00,0x00,0x00,0x4F,0x00,0xC2,0x00,0xB0,0x00 };//used to get the polling time for the self tests and check the status
            //add this in if we end up needing a way to abort a running DST
            //uint8_t DSTABORT[16] = {0x85,0x08,0x0E,0x00,0xD4,0x00,0x00,0x00,0x7F,0x00,0x4F,0x00,0xC2,0x00,0xB0,0x00};
            uint8_t temp_buf[512];
            unsigned int delayTime = 0;
            memset(temp_buf,0,sizeof(temp_buf));//clear the buffer
            if( DSTType == 1 )//short DST
            {
                DSTSATA[8] = 0x01;
                delayTime = 5;//seconds
            }
            else if ( DSTType == 2 )//long DST
            {
                DSTSATA[8] = 0x02;
                delayTime = 15;//seconds
            }
            else
            {
                //unknown test type, so lets just fail out.
                fclose(fp_DST);
                return result;
            }
            //check for running DST. If DST is already running, return -2
            ret = send_cdb(device, temp_buf, sizeof(temp_buf), DSTSTATUS, 16, XFER_DATA_IN);
            if( ret == 0 )
            {
                if(temp_buf[363] != 0)
                {
                    result = -2;
                    fclose(fp_DST);
                    return result;
                }
            }
            else
            {
                fclose(fp_DST);
                return result;            
            }
            //send the DST command
            ret = send_cdb(device, temp_buf, sizeof(temp_buf), DSTSATA, 16, XFER_DATA_IN);
            if( ret == 0 )
            {
                while(status > 0x08)
                {
                    delay_sec(delayTime);
                    //send a CDB that will check the status of the running self test
                    ret = send_cdb(device, temp_buf, sizeof(temp_buf), DSTSTATUS, 16, XFER_DATA_IN);
                    if( ret == 0 )
                    {
                        //get the progress
                        status = (uint8_t*)temp_buf[363];
                        percentComplete = ( ( status << 28 ) >> 28) * 0x0a;//get the status before we shift it by 4 for the while condition check.
                        status = status >> 4;
                        printf("\n    Test progress: %d    ",100-percentComplete);
                        fprintf(fp_DST,"\n    Test progress: %d    ",100-percentComplete);
                    }
                    else
                    {
                        break;
                    }
                }
                printf("\n\n");
                if(status == 0)
                {
                    result = 0;//we passed.
                    fprintf(fp_DST,"\n\nDST Passed!\n");
                }
                else
                {
                    result = -1;//failed the test
                    fprintf(fp_DST,"\n\nDST Failed!\n");
                }
            }
        }
        else //SCSI,SAS, etc
        {
            uint8_t DSTSCSI[6] = { 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00 };
            uint8_t logSense[10] = { 0x4D, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00 };//used to get the result of the DST on SCSI...or just used to see if DST is already running
            uint8_t DSTPROGRESS[6] = { 0x03, 0x00, 0x00, 0x00, 0xFF, 0x00 };
            //add this in if we need to support aborting a running DST
            //uint8_t DSTABORT[6] = { 0x1D, 0x80, 0x00, 0x00, 0x00, 0x00 };
            uint8_t temp_buf[512];
            int16_t pageCode = 0;
            unsigned int delayTime = 0;
            if( DSTType == 1 )//short DST
            {
                DSTSCSI[1] = 0x20;
                delayTime = 5;//seconds
            }
            else if ( DSTType == 2 )//long DST
            {
                DSTSCSI[1] = 0x40;
                delayTime = 15;//seconds
            }
            else
            {
                //unknown test type, so lets just fail out.
                fclose(fp_DST);
                return result;
            }

            memset(temp_buf,0,sizeof(temp_buf));//clear the buffer
            //check if DST is already running
            ret = send_cdb(device, temp_buf, sizeof(temp_buf), logSense, 10, XFER_DATA_IN);
            if( ret != 0 )
            {
                fclose(fp_DST);
                return result;
            }
            pageCode = temp_buf[7];
            if(pageCode == 0x10)
            {
                if( (temp_buf[8] & 0x0F) == 0x0F )
                {
                    result = -2;
                    fclose(fp_DST);
                    return result;
                }
            }
            //DST not already running, so lets start one.
            ret = send_cdb(device, temp_buf, sizeof(temp_buf), DSTSCSI, 6, XFER_DATA_IN);
            if( ret == 0 )
            {
                while(status > 0x08)
                {
                    delay_sec(delayTime);
                    //send a CDB that will check the status of the running self test
                    ret = send_cdb(device, temp_buf, sizeof(temp_buf), logSense, 10, XFER_DATA_IN);
                    if( ret == 0 )
                    {
                        status = (uint8_t*)temp_buf[8];
                        status &= 0x0F;
                        //check the progress since the test is still running
                        send_cdb(device, temp_buf, sizeof(temp_buf), DSTPROGRESS, 6, XFER_DATA_IN);
                        percentComplete = (temp_buf[16] << 8) + temp_buf[17];
                        percentComplete *= 100;
                        percentComplete /= 65536;
                        printf("\n    Test progress: %d    ",percentComplete);
                        fprintf(fp_DST,"\n    Test progress: %d    ",percentComplete);
                    }
                    else
                    {
                        break;
                    }
                }
                printf("\n\n");
                if(status == 0)
                {
                    result = 0;//we passed.
                    fprintf(fp_DST,"\n\nDST Passed!\n");
                }
                else
                {
                    result = -1;//failed the test
                    fprintf(fp_DST,"\n\nDST Failed!\n");
                }
            }
            
        }
    }
    fclose(fp_DST);
    return result;
}
#endif
#ifdef SECURE_ERASE
/*
returns: 
    0-pass
    -1-failed
    -2-failed because drive is frozen
    -3-failed because security isn't enabled
*/
int run_secureerase(DEVICE * device, char * pserial_number, int isSATA, int enhanced)
{
    int result = -1;//test result
    int ret = 0;//command return value
    char filename[128]; /* some file names get really long */
    FILE * fp_SecureErase;
    current_time = time(NULL);
	strftime(current_time_st, 64, "%Y-%m-%d__%H_%M_%S", localtime(&current_time));
    sprintf(filename, "%s_Secure_Erase_%s.log", pserial_number,current_time_st_ptr); 
    if( (fp_SecureErase = fopen(filename, "w")) == NULL )
    {
        printf("%s: Couldn't open file %s\n",__FUNCTION__, filename);
        perror("fopen");
        ret = -1;
    }
    else
    {
        if(isSATA)//SATA secure erase
        {
            //commands
            uint8_t rv28[16];
            uint8_t isFrozen[16];
            uint8_t isSecurityEnabled[16];
            uint8_t getEraseTime[16];
            uint8_t erasePrep[16];
            uint8_t eraseMaster[16];
            uint8_t temp_buf[512];//use this for checking command returns or for writing something
            unsigned int delayTime = 2;
            unsigned int eraseTime = 0;
            unsigned int additionalDelay = 8;
            unsigned int endTime = 0;//when the test should be complete
            memset(temp_buf,0,sizeof(temp_buf));//clear the buffer

            //Check if we are frozen. if we are, fail

            //make sure security is enabled. if it isn't, fail.

            //get the erase time...make sure this is in SECONDS!

            //set the password in the buffer

            //check if we are doing an enhanced erase, and change any command parameters necessary
            if( enhanced )
            {
                //make changes to any commands we need to for this to work
            }
            //send erase prep

            //send master password erase            
            
            if( ret != 0 )
            {
            }
            //now lets start a loop with delays that will update the time remaining or show a progress indicator
            endTime = time(0) + eraseTime;
            printf("\nErasing drive");
            while( time(0) < endTime )
            {
                delay_sec(delayTime);
                printf(".");
            }
            printf("\n");
            //send a rv28() command to see if we are done now...if we are, pass, else fail
            ret = send_cdb(device, temp_buf, sizeof(temp_buf), rv28, 16, XFER_DATA_IN);
            if( ret == 0 )
            {
                result = 0;
            }
            else
            {
                printf("Waiting another %d seconds for command to complete",additionalDelay);
                endTime = time(0) + eraseTime + additionalDelay;
                while( time(0) < endTime )
                {
                    delay_sec(delayTime);
                    printf(".");
                }
                printf("\n");
                //send a rv28() command to see if we are done now...if we are, pass, else fail
                ret = send_cdb(device, temp_buf, sizeof(temp_buf), rv28, 16, XFER_DATA_IN);
                if( ret == 0 )
                {
                    result = 0;
                }
                else
                {
                    result = -1;
                }
            }
            if( result == 0 )
            {
                //check if drive is still locked...if it is time to fail

                //check if security is enabled...if it is time to fail

                //set the password to the master password
            }
                
        }
        else //SCSI secure erase
        {
        }
    }
    fclose(fp_SecureErase);
    return result;
}
#endif
#ifdef SMART_CHECK
int run_SMART_Check(DEVICE * device, char * pserial_number)
{
    int result = -1;//test result
    int ret = 0;//command return value
    char filename[128]; /* some file names get really long */
    FILE * fp_SMARTCheck;
    current_time = time(NULL);
	strftime(current_time_st, 64, "%Y-%m-%d__%H_%M_%S", localtime(&current_time));
    sprintf(filename, "%s_SMART_Check_%s.log", pserial_number,current_time_st_ptr); 
    if( (fp_SMARTCheck = fopen(filename, "w")) == NULL )
    {
        printf("%s: Couldn't open file %s\n",__FUNCTION__, filename);
        perror("fopen");
        ret = -1;
    }
    else
    {
        //set the check condition bit in this command to get the return tfrs
        uint8_t SMARTCheck[16] = { 0x85,0x06,0x20,0x00,0xDA,0x00,0x00,0x00,0x00,0x00,0x4F,0x00,0xC2,0xE0,0xB0,0x00 };
        uint8_t temp_buf[512];
        memset(temp_buf,0,sizeof(temp_buf));//clear the buffer
        ret = send_cdb(device, temp_buf, sizeof(temp_buf), SMARTCheck, 16, XFER_NO_DATA);
        if( ret == 0 )
        {
            //need to check the sense data/return tfrs for the command result.
            if(temp_buf[6] == 0x4f)
            {
                result = 0;
            }
        }
    }
    fclose(fp_SMARTCheck);
    return result;
}
#endif
