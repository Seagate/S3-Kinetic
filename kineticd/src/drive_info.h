#ifndef KINETIC_DRIVE_INFO_H_
#define KINETIC_DRIVE_INFO_H_

#include <stdlib.h>
#include <string>

extern "C" {
#include "scsi_helper_func.h"
}

using std::string;


namespace com {
namespace seagate {
namespace kinetic {


typedef struct _STATIC_DRIVE_INFO {
    // drive descriptions and revisions
    std::string drive_wwn;
    std::string drive_sn;
    std::string drive_vendor;
    std::string drive_model;
    std::string drive_fw;
    std::string storage_device;
    // drive capabilities
    bool is_present;
    bool is_SSD;
    bool supports_SED;
    bool supports_ZAC;
    // drive attributes
    int sector_size;
    int logical_sectors_per_physical_sector;
    uint64_t drive_capacity_in_bytes;
    uint64_t sectors_read_at_poweron;
} STATIC_DRIVE_INFO;

int populate_identify_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo);
int populate_device_capabilities_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo);
int populate_device_capacities_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo);
int populate_general_statistics_info(DEVICE* device, STATIC_DRIVE_INFO* sdinfo);
STATIC_DRIVE_INFO PopulateStaticDriveAttributes(string store_device);

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DRIVE_INFO_H_
