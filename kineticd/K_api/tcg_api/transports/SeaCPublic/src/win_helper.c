//
// win_helper.c   Implementation for win_helper functions
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
#include <stdlib.h> // for mbstowcs_s
#include <stddef.h> // offsetof
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wchar.h>
#include <string.h>
#include "scsi_helper_func.h"
#include "win_helper.h"
#include "storelib_helper.h"

void print_bus_type (BYTE type)
{
	switch(type)
	{
	case BusTypeScsi:
		printf("SCSI");
		break;
	case BusTypeAtapi:
		printf("ATAPI");
		break;
	case BusTypeAta:
		printf("ATA");
		break;
	case BusType1394:
		printf("1394");
		break;
	case BusTypeSsa:
		printf("SSA");
		break;
	case BusTypeFibre:
		printf("FIBER");
		break;
	case BusTypeUsb:
		printf("USB");
		break;
	case BusTypeRAID:
		printf("RAID");
		break;
	case BusTypeiScsi:
		printf("iSCSI");
		break;
	case BusTypeSas:
		printf("SAS");
		break;
	case BusTypeSata:
		printf("SATA");
		break;
	case BusTypeSd:
		printf("SD");
		break;
	case BusTypeMmc:
		printf("MMC");
		break;
	case BusTypeVirtual:
		printf("VIRTUAL");
		break;
	case BusTypeFileBackedVirtual:
		printf("FILEBACKEDVIRTUAL");
		break;
	default:
		printf("UNKNOWN" );
		break;
	}
}

int get_os_drive_number (char * filename)
{
	int drive_num = -1;
	char * pdev = NULL;
	//char * next_token = NULL;
	pdev = strrchr(filename, 'e');
	if(pdev != NULL)
		drive_num = atoi(pdev+1);
	return drive_num;
}

int get_device( char * filename, DEVICE * device)
{
    // DEVICE device = { 0 };
	wchar_t device_name[40];
	LPWSTR device_Ptr = device_name;
	size_t converted_count = 0;
	DWORD error_code = 0;
	int ret = 0;
	ULONG returned_data = 0;		
	PSTORAGE_DEVICE_DESCRIPTOR device_desc = NULL;
	PSTORAGE_ADAPTER_DESCRIPTOR adapter_desc = NULL; 
	STORAGE_PROPERTY_QUERY query;
	STORAGE_DESCRIPTOR_HEADER header;	
	
	//printf("%s -->\n Opening Device %s\n",__FUNCTION__, filename);		

	mbstowcs_s(NULL, device_name, strlen(filename)+1, filename, _TRUNCATE);//Plus null
	//lets try to open the device. 
	device->fd = CreateFile(device_name,
							FILE_ALL_ACCESS, //GENERIC_WRITE | GENERIC_READ,
							FILE_SHARE_READ | FILE_SHARE_WRITE,     
						    NULL,
						    OPEN_EXISTING,
						    0,
						    NULL);

	// Check if we get a valid handle back.
    if (device->fd == INVALID_HANDLE_VALUE)
	{
		error_code = GetLastError();
        printf("Error: opening dev %s. Error: %d\n",
               filename, error_code);
		ret = error_code;
	}
	else
	{
		device->os_drive_number = get_os_drive_number(filename);

		// Lets get the SCSI address 
		ret = DeviceIoControl(device->fd,
						IOCTL_SCSI_GET_ADDRESS,
						NULL,
						0,
						&device->scsi_addr,
						sizeof(device->scsi_addr),
						&returned_data,
						FALSE);
		if ( ret > 0 )
		{		
			// Lets get some properties.				
			memset(&query, 0, sizeof(STORAGE_PROPERTY_QUERY));
			memset(&header, 0, sizeof(STORAGE_DESCRIPTOR_HEADER));

			query.QueryType = PropertyStandardQuery;
			query.PropertyId = StorageAdapterProperty; 
        
			ret = DeviceIoControl(device->fd,
									IOCTL_STORAGE_QUERY_PROPERTY,
									&query,
									sizeof(STORAGE_PROPERTY_QUERY),
									&header,
									sizeof(STORAGE_DESCRIPTOR_HEADER),
									&returned_data,
									FALSE);
			if( ( ret > 0 ) && ( header.Size != 0 ) )
			{
				adapter_desc = (PSTORAGE_ADAPTER_DESCRIPTOR) LocalAlloc(LPTR, header.Size);
				if ( adapter_desc != NULL )
				{
					ret = DeviceIoControl(device->fd,
								IOCTL_STORAGE_QUERY_PROPERTY,
								&query,
								sizeof(STORAGE_PROPERTY_QUERY),
								adapter_desc,
								header.Size,
								&returned_data,
								FALSE);

					if( ret > 0 )
					{
						// TODO: Copy any of the adapter stuff.
#ifdef _DEBUG
						printf("Adapter BusType: ");
						print_bus_type(adapter_desc->BusType);
						printf(" \n");
#endif
						// Now lets get device stuff
						query.PropertyId = StorageDeviceProperty;
						memset(&header, 0, sizeof(STORAGE_DESCRIPTOR_HEADER));
						ret = DeviceIoControl(device->fd,
											IOCTL_STORAGE_QUERY_PROPERTY,
											&query,
											sizeof(STORAGE_PROPERTY_QUERY),
											&header,
											sizeof(STORAGE_DESCRIPTOR_HEADER),
											&returned_data,
											FALSE);

						if( ( ret > 0 ) && ( header.Size != 0 ) )
						{
							device_desc = (PSTORAGE_DEVICE_DESCRIPTOR) LocalAlloc(LPTR, header.Size);
							if ( device_desc != NULL )
							{
								ret = DeviceIoControl(device->fd,
														IOCTL_STORAGE_QUERY_PROPERTY,
														&query,
														sizeof(STORAGE_PROPERTY_QUERY),
														device_desc,
														header.Size,
														&returned_data,
														FALSE);
								if ( ret > 0 )
								{
#ifdef _DEBUG
									printf("Drive BusType: ");
									print_bus_type(device_desc->BusType);
									printf(" \n");
#endif
									if ( ( adapter_desc->BusType == BusTypeAta) ||
										 ( device_desc->BusType == BusTypeAta ) || 
										 ( device_desc->BusType == BusTypeSata ) )
									{
										device->drive_info.drive_type = ATA_DRIVE;
										device->drive_info.interface_type = IDE_INTERFACE;
									}
									else if ( ( device_desc->BusType == BusTypeSas ) || 
											( device_desc->BusType == BusTypeScsi ) )
									{										
										device->drive_info.interface_type = SCSI_INTERFACE;
										//This doesn NOT mean that drive_type is SCSI
									}
									else if ( ( adapter_desc->BusType == BusTypeRAID ) || 
											( device_desc->BusType == BusTypeRAID ) )
									{
										device->drive_info.interface_type = SCSI_INTERFACE;	
										device->drive_info.drive_type = RAID_DRIVE;									
#ifdef LSI_RAID_SUPPORTED
										//Now lets to figure out if we can do 
										if(init_storelib_lib() == EXIT_SUCCESS)
										{
											print_storelib_version();
											fill_raid_info(device);
										}
#endif //LSI_RAID_SUPPORTED
									}

									// Lets fill out rest of info 
									ret = fill_in_device_info(device);
								}
							} // else couldn't device desc allocate memory	
						}// either ret or size is zero					
					}		
				} // else couldn't adapter desc allocate memory			
			} // either ret or size is zero
		}
	}
	// Just in case we bailed out in any way. 
	device->last_error = GetLastError();
	if (adapter_desc != NULL)
	{
		LocalFree(adapter_desc);
	}
	if (device_desc != NULL) 
	{
		LocalFree(device_desc);
	}

    //printf("%s <--\n",__FUNCTION__);
    return !ret;
}


// 0 == FAILURE 
int convert_scsi_ctx_to_sptd(SCSI_IO_CTX * scsi_io_ctx, tSPTIoContext * psptd)
{
	int ret = 1;
	psptd->Sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	psptd->Sptd.PathId = scsi_io_ctx->device->scsi_addr.PathId;
	psptd->Sptd.TargetId = scsi_io_ctx->device->scsi_addr.TargetId;
	psptd->Sptd.Lun = scsi_io_ctx->device->scsi_addr.Lun;
	psptd->Sptd.CdbLength = scsi_io_ctx->cdb_len;
	//psptd->Sptd.SenseInfoLength = scsi_io_ctx->sense_sz;
	psptd->Sptd.SenseInfoLength = sizeof(psptd->SenseBuffer);
	switch(scsi_io_ctx->direction)
	{
	case XFER_DATA_IN:
		psptd->Sptd.DataIn = SCSI_IOCTL_DATA_IN;
		psptd->Sptd.DataTransferLength = scsi_io_ctx->data_len;
		psptd->Sptd.DataBuffer = scsi_io_ctx->pdata;
		break;
	case XFER_DATA_OUT:
		psptd->Sptd.DataIn = SCSI_IOCTL_DATA_OUT;
		psptd->Sptd.DataTransferLength = scsi_io_ctx->data_len;
		psptd->Sptd.DataBuffer = scsi_io_ctx->pdata;
		break;
	case XFER_NO_DATA:
		psptd->Sptd.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
		psptd->Sptd.DataTransferLength = 0;
		psptd->Sptd.DataBuffer = NULL;
		break;
	default:
		printf("\nData Direction Unspecified.\n");
		ret = 0;
		break;
	}
	if (scsi_io_ctx->timeout)
	{
		// Windows likes seconds
		psptd->Sptd.TimeOutValue = scsi_io_ctx->timeout/1000;
	}
	else
	{
		psptd->Sptd.TimeOutValue = 5;
	}
	psptd->Sptd.SenseInfoOffset = offsetof(tSPTIoContext, SenseBuffer);
	memcpy(psptd->Sptd.Cdb,scsi_io_ctx->pcdb,sizeof(psptd->Sptd.Cdb));
	return ret;
}

int send_spt_io(SCSI_IO_CTX * scsi_io_ctx)
{
    int ret = -2;
	BOOL success = FALSE;
	ULONG returned_data = 0;
	tSPTIoContext sptdio; 
    //printf("%s -->\n",__FUNCTION__);

	if ( convert_scsi_ctx_to_sptd(scsi_io_ctx, &sptdio) )
	{
		SetLastError( NO_ERROR );
		sptdio.SptBufLen = sizeof(tSPTIoContext);
		success = DeviceIoControl(scsi_io_ctx->device->fd,
						IOCTL_SCSI_PASS_THROUGH_DIRECT,
						&sptdio.Sptd,
						sptdio.SptBufLen,
						&sptdio.Sptd,
						sptdio.SptBufLen,
						&returned_data,
						FALSE);
		scsi_io_ctx->device->last_error = GetLastError();
		scsi_io_ctx->return_status.status_scsi = sptdio.Sptd.ScsiStatus;

		if (success)
		{
			// If the operation completes successfully, the return value is nonzero.
			// If the operation fails or is pending, the return value is zero. To get extended error information, call
			ret = 0; //setting to zero to be compatible with linux
			if(scsi_io_ctx->return_status.status_scsi == 0)
			{
				//For success we usually don't get any sense data back. 
				scsi_io_ctx->rtfrs.status = 0x50; // Fake it
			}
		}
		
		// Any sense data?
		if ( (scsi_io_ctx->psense) && (scsi_io_ctx->sense_sz) )
		{
			memcpy(scsi_io_ctx->psense, sptdio.SenseBuffer,scsi_io_ctx->sense_sz);
		}
	}
	else
	{
		ret = -1;
	}
    //printf("%s <--\n",__FUNCTION__);
    return ret;
}

// 0 == FAILURE 
int convert_scsi_ctx_to_ide(SCSI_IO_CTX * p_scsi_io_ctx, PATA_PASS_THROUGH_DIRECT p_t_ata_pt)
{
	int ret = 1;
	//printf("%s -->\n",__FUNCTION__);

	p_t_ata_pt->Length = sizeof(ATA_PASS_THROUGH_DIRECT);
	p_t_ata_pt->AtaFlags = ATA_FLAGS_DRDY_REQUIRED | ATA_FLAGS_NO_MULTIPLE ;		

	switch(p_scsi_io_ctx->direction)
	{
	case XFER_DATA_IN:
		p_t_ata_pt->AtaFlags |= ATA_FLAGS_DATA_IN ;
		p_t_ata_pt->DataTransferLength = p_scsi_io_ctx->data_len;		
		p_t_ata_pt->DataBuffer = p_scsi_io_ctx->pdata;		
		break;
	case XFER_DATA_OUT:
		p_t_ata_pt->AtaFlags |= ATA_FLAGS_DATA_OUT;		
		p_t_ata_pt->DataTransferLength = p_scsi_io_ctx->data_len;		
		p_t_ata_pt->DataBuffer = p_scsi_io_ctx->pdata;		
		break;
	case XFER_NO_DATA:		
		p_t_ata_pt->DataTransferLength = 0;		
		p_t_ata_pt->DataBuffer = NULL;		
		break;
	default:
		printf("\nData Direction Unspecified.\n");
		ret = 0;
		break;
	}
	// TODO: Flags for 48 bit & DMA 
	
	if (p_scsi_io_ctx->timeout)
	{
		// Windows likes seconds
		p_t_ata_pt->TimeOutValue = p_scsi_io_ctx->timeout/1000;
	}
	else
	{
		p_t_ata_pt->TimeOutValue = 5;
	}
	p_t_ata_pt->PathId = p_scsi_io_ctx->device->scsi_addr.PathId;
	p_t_ata_pt->TargetId = p_scsi_io_ctx->device->scsi_addr.TargetId;
	p_t_ata_pt->Lun = p_scsi_io_ctx->device->scsi_addr.Lun;
	// Task File
	if ( p_scsi_io_ctx->pcdb[0] == SAT_ATA_16 )
	{
		p_t_ata_pt->CurrentTaskFile[0] = p_scsi_io_ctx->pcdb[4]; // Features Register
		p_t_ata_pt->CurrentTaskFile[1] = p_scsi_io_ctx->pcdb[6]; // Sector Count Reg
		p_t_ata_pt->CurrentTaskFile[2] = p_scsi_io_ctx->pcdb[8]; // Sector Number ( or LBA Lo )
		p_t_ata_pt->CurrentTaskFile[3] = p_scsi_io_ctx->pcdb[10]; // Cylinder Low ( or LBA Mid )
		p_t_ata_pt->CurrentTaskFile[4] = p_scsi_io_ctx->pcdb[12]; // Cylinder High (or LBA Hi)
		p_t_ata_pt->CurrentTaskFile[5] = p_scsi_io_ctx->pcdb[13]; // Device/Head Register
		p_t_ata_pt->CurrentTaskFile[6] = p_scsi_io_ctx->pcdb[14]; // Command Register
		p_t_ata_pt->CurrentTaskFile[7] = 0; // Reserved
	}
	else if ( p_scsi_io_ctx->pcdb[0] == SAT_ATA_12 )
	{
		p_t_ata_pt->CurrentTaskFile[0] = p_scsi_io_ctx->pcdb[2]; // Features Register
		p_t_ata_pt->CurrentTaskFile[1] = p_scsi_io_ctx->pcdb[4]; // Sector Count Reg
		p_t_ata_pt->CurrentTaskFile[2] = p_scsi_io_ctx->pcdb[5]; // Sector Number ( or LBA Lo )
		p_t_ata_pt->CurrentTaskFile[3] = p_scsi_io_ctx->pcdb[6]; // Cylinder Low ( or LBA Mid )
		p_t_ata_pt->CurrentTaskFile[4] = p_scsi_io_ctx->pcdb[7]; // Cylinder High (or LBA Hi)
		p_t_ata_pt->CurrentTaskFile[5] = p_scsi_io_ctx->pcdb[8]; // Device/Head Register
		p_t_ata_pt->CurrentTaskFile[6] = p_scsi_io_ctx->pcdb[9]; // Command Register
		p_t_ata_pt->CurrentTaskFile[7] = 0; // Reserved
	}
	else
	{
		ret = 0; 
		// Something ain't right in cdb
	}
	//printf("%s <--\n",__FUNCTION__);
	return ret;
}

int send_apt_io(SCSI_IO_CTX * scsi_io_ctx)
{
	int ret = -2;
	BOOL success;
	ULONG returned_data = 0;
	ATA_PASS_THROUGH_DIRECT t_ata_pt;
	PATA_PASS_THROUGH_DIRECT p_t_ata_pt = & t_ata_pt;
		
	//printf("%s -->\n",__FUNCTION__);
	memset(p_t_ata_pt,0,sizeof(ATA_PASS_THROUGH_DIRECT));
	if ( convert_scsi_ctx_to_ide(scsi_io_ctx, p_t_ata_pt) )
	{
		scsi_io_ctx->device->last_error = 0;
		success = DeviceIoControl(scsi_io_ctx->device->fd,
					IOCTL_ATA_PASS_THROUGH_DIRECT,
					p_t_ata_pt,
					sizeof(ATA_PASS_THROUGH_DIRECT),
					p_t_ata_pt,
					sizeof(ATA_PASS_THROUGH_DIRECT),
					&returned_data,
					FALSE);

		scsi_io_ctx->device->last_error = GetLastError();
		if(success)
		{
			ret = 0;
		}
	}
	else
	{
		printf("Couldn't convert SCSI-To-IDE interface\n");		
	}

	//printf("%s <--\n",__FUNCTION__);
	return ret;
}

int send_io(SCSI_IO_CTX * scsi_io_ctx)
{
	int ret = -2;
	//printf("%s -->\n",__FUNCTION__);
	if (scsi_io_ctx->device->drive_info.interface_type == IDE_INTERFACE)
	{
		if ( (scsi_io_ctx->pcdb[0] == SAT_ATA_16 ) || 
		   (scsi_io_ctx->pcdb[0] == SAT_ATA_12 ) )
		{
			ret = send_apt_io(scsi_io_ctx);
		}
		else
		{
			ret = send_spt_io(scsi_io_ctx);
		}
	}
	else if (scsi_io_ctx->device->drive_info.interface_type == SCSI_INTERFACE)
	{
		if ( (scsi_io_ctx->device->drive_info.drive_type == RAID_DRIVE)
			&& (scsi_io_ctx->device->raid_info.current_pt_target_id >= 0) )
		{
			ret = raid_scsi_pt(scsi_io_ctx, scsi_io_ctx->device->raid_info.current_pt_target_id);
		}
		else
		{
			ret = send_spt_io(scsi_io_ctx);
		}
	}
	else
	{
		printf("Target Device does not have a valid interface\n");
	}
	// printf("%s <--\n",__FUNCTION__);
	return ret;
}