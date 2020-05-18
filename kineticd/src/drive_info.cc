#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "glog/logging.h"

extern "C" {
#include "scsi_helper_func.h"
}

#include "drive_info.h"

using com::seagate::kinetic::STATIC_DRIVE_INFO;
using std::string;

//add more outparameters to this if we need to know more info about an ATA device based off of an identify command.
int com::seagate::kinetic::populate_identify_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo) {
    int ret = 0;
    //this will send a SAT ident command to the ata drive and allow us to read the ident info
    uint8_t cdb[16] = { 0x85, 0x08, 0x0E, 0, 0, 0, 0x01, 0, 0, 0, 0, 0, 0, 0xE0, 0xEC, 0 };
    uint8_t ident_buf[512];
    sdinfo->is_present = false;
    memset(ident_buf, 0, sizeof(ident_buf));//clear the buffer
    //should make a function called send_cdb and then check a buffer to see if word 217 shows us that we have an SSD
    ret = send_cdb(device, ident_buf, sizeof(ident_buf), cdb, 16, XFER_DATA_IN);
    if (ret == 0)
    {   sdinfo->is_present = true;
        // SED Support in Word 48
        uint16_t * ident_word = (uint16_t *)&ident_buf[0];
        if ((ident_word[48]&0x0001) == 0x0001) {
            sdinfo->supports_SED = true;
        } else {
            sdinfo->supports_SED = false;
        }
        // SSD has spindle rate of 1
        if (ident_word[217] == 0x0001) {
            sdinfo->is_SSD = true;
        } else {
            sdinfo->is_SSD = false;
        }
        // SN in Words 10-19
        sdinfo->drive_sn = "";
        for (int p = 10; p < 20; p++) {
            sdinfo->drive_sn += (char)((ident_word[p]&0xFF00)>>8);
            sdinfo->drive_sn += (char)(ident_word[p]&0x00FF);
        }
        // FW_REV in Words 23-26
        sdinfo->drive_fw = "";
        for (int p = 23; p < 27; p++) {
            sdinfo->drive_fw += (char)((ident_word[p]&0xFF00)>>8);
            sdinfo->drive_fw += (char)(ident_word[p]&0x00FF);
        }
        // Model in Words 27-46
        sdinfo->drive_model = "";
        for (int p = 27; p < 47; p++) {
            sdinfo->drive_model += (char)((ident_word[p]&0xFF00)>>8);
            sdinfo->drive_model += (char)(ident_word[p]&0x00FF);
        }
        // Physical/Logical Sector Size Word 106
        if ((ident_word[106] & 0x2000) == 0x2000) {
            int sectorsize = (int)((ident_word[106]&0x0F));
            sectorsize = pow(2, sectorsize);
            sdinfo->logical_sectors_per_physical_sector = sectorsize;
        } else {
            sdinfo->logical_sectors_per_physical_sector = 1;
        }
        if (sdinfo->logical_sectors_per_physical_sector != 1) {
            //Get the next alligned sector
            sdinfo->non_sed_pin_info_sector_num = sdinfo->logical_sectors_per_physical_sector - \
            (FIRST_POSSIBLE_NON_SED_PIN_INFO_SECTOR % sdinfo->logical_sectors_per_physical_sector) + FIRST_POSSIBLE_NON_SED_PIN_INFO_SECTOR;
        } else {
            sdinfo->non_sed_pin_info_sector_num = FIRST_POSSIBLE_NON_SED_PIN_INFO_SECTOR;
        }
        // World Wide Name Words 108-111
        sdinfo->drive_wwn = "";
        char wwn[16];
        for (int p = 108; p < 112; p++) {
            sprintf(wwn, "%04x", ident_word[p]);
            sdinfo->drive_wwn += wwn;
        }
        uint64_t maxL = 0;
        // Get Capacity from ident
        if ((ident_word[69] & 0x0008) == 0) {
            // Use 48 bit Cap
            maxL = ((uint64_t*)(&(ident_word[100])))[0];
        } else {
            // Use 64 bit
            maxL = ((uint64_t*)(&(ident_word[230])))[0];
        }
        sdinfo->drive_capacity_in_bytes = maxL * 512;
    }
    return ret;
}

//add more outparameters to this if we need to know more info about an ATA device based off of a device capabilities log
int com::seagate::kinetic::populate_device_capabilities_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo) {
    int ret = 0;
    //this will send a log query command to the ata drive
    uint8_t cdb[16] = { 0x85, 0x08, 0x0E, 0, 0, 0, 0x01, 0, 0x30, 0, 0x03, 0, 0, 0xE0, 0x2F, 0 };
    uint8_t cap_buf[512];
    memset(cap_buf, 0, sizeof(cap_buf));//clear the buffer
    ret = send_cdb(device, cap_buf, sizeof(cap_buf), cdb, 16, XFER_DATA_IN);
    if (ret == 0) {
        uint64_t * cap_qword = (uint64_t *)&cap_buf[0];
        if (cap_qword[0] == 0x8000000000030001) {
            // Look here for ZAC support
            if (((cap_qword[13] & 0x8000000000000001) == 0x8000000000000001) || \
                ((cap_qword[14] & 0x800000000000001F) > 0x8000000000000000)) {
                    sdinfo->supports_ZAC = true;
                } else {
                    sdinfo->supports_ZAC = false;
                }
        } else {
            sdinfo->supports_ZAC = false;
        }
    }
    return ret;
}

// Log Address 0x30, Log Page 2
//add more outparameters to this if we need to know more info about an ATA device based off of a device capacities log
int com::seagate::kinetic::populate_device_capacities_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo) {
    int ret = 0;
    //this will send a log query command to the ata drive
    uint8_t cdb[16] = { 0x85, 0x08, 0x0E, 0, 0, 0, 0x01, 0, 0x30, 0, 0x02, 0, 0, 0xE0, 0x2F, 0 };
    uint8_t cap_buf[512];
    memset(cap_buf, 0, sizeof(cap_buf));//clear the buffer
    ret = send_cdb(device, cap_buf, sizeof(cap_buf), cdb, 16, XFER_DATA_IN);
    if (ret == 0) {
        uint64_t * cap_qword = (uint64_t *)&cap_buf[0];
        if (cap_qword[0] == 0x8000000000020001) {
            // Look here for sector size
            if ((cap_qword[3] & 0x8000000000000000) == 0) {
                sdinfo->sector_size = 512; //Doesn't support Logical Sector Size
            } else {
                sdinfo->sector_size = (cap_qword[3] & 0x00000000FFFFFFFF) * 2;
            }
            sdinfo->drive_capacity_in_bytes = (cap_qword[1] & 0x0000FFFFFFFFFFFF)*sdinfo->sector_size;
        } else {
            sdinfo->sector_size = 512; // Assume legacy sector size
            // Leave capacity set at whatever was found in identify
        }
    }
    return ret;
}

// Log Address 0x04, Log Page 0x01
//add more outparameters to this if we need to know more info about an ATA device based off of a general statistics log
int com::seagate::kinetic::populate_general_statistics_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo) {
    int ret = 0;
    uint8_t cdb[16] = { 0x85, 0x08, 0x0E, 0, 0, 0, 0x01, 0, 0x04, 0, 0x01, 0, 0, 0xE0, 0x2F, 0 };
    uint8_t cap_buf[512];
    memset(cap_buf, 0, sizeof(cap_buf));//clear the buffer
    ret = send_cdb(device, cap_buf, sizeof(cap_buf), cdb, 16, XFER_DATA_IN);
    if (ret == 0) {
        uint64_t * cap_qword = (uint64_t *)&cap_buf[0];
        if (cap_qword[0] == 0x0000000000010001) {
            // Look here for sectors read
            sdinfo->sectors_read_at_poweron = (cap_qword[5] & 0x0000FFFFFFFFFFFF);
        } else {
            sdinfo->sectors_read_at_poweron = 0;
        }
    }
    return ret;
}


STATIC_DRIVE_INFO com::seagate::kinetic::PopulateStaticDriveAttributes(string store_device) {
    STATIC_DRIVE_INFO sdinfo;
    sdinfo.storage_device = store_device;
    DEVICE device;
    char* devString = (char*)(store_device.c_str());
    get_device(devString, &device);

    // Read info from Identify Device

    sdinfo.drive_wwn = "";
    sdinfo.drive_sn = "";
    sdinfo.drive_vendor = "Seagate";
    sdinfo.drive_model = "";
    sdinfo.drive_fw = "";
    sdinfo.is_present = false;
    sdinfo.supports_SED = false;
    sdinfo.supports_ZAC = false;
    sdinfo.drive_capacity_in_bytes = 0;
    sdinfo.sector_size = 512;
    sdinfo.logical_sectors_per_physical_sector = 0;
    sdinfo.non_sed_pin_info_sector_num = 0;
    // Read info from SMART Request
    sdinfo.sectors_read_at_poweron = 1000;
    populate_identify_info(&device, &sdinfo);
    if (sdinfo.is_present) {
        populate_device_capabilities_info(&device, &sdinfo);
        populate_general_statistics_info(&device, &sdinfo);
    }
    return sdinfo;
}
