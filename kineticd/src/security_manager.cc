#include "command_line_flags.h"
#include "product_flags.h"
#include "security_manager.h"
#include "manlocking_processor.h"

#include "glog/logging.h"
#include "ring_buffer_log_sink.h"

#include "ks_globals.h"
#include "ks_setup.h"
#include "ks_setpin.h"
#include "ks_erase.h"

#ifdef SED_SUPPORTED
/* Kinetic API includes */
#include "ks_debug.h"
#include "ks_manlocking.h"

/*TCG API includes */
#include "high_level.h"
#include "transport.h"
#include "transport_locator.h"
#endif

#include "util/crc32c.h"
using ::leveldb::crc32c::Value;

namespace com {
namespace seagate {
namespace kinetic {

const int SecurityManager::MAX_RETRIES = 5;
const int SecurityManager::SLEEP_TIME = 10000;

#ifdef SED_SUPPORTED

SecurityManager::SecurityManager() {}

SecurityManager::~SecurityManager() {}

#ifdef ISE_AND_LOCK_DISABLED
/* will this call eventually have band array and count in its parameter list? CNA */
PinStatus SecurityManager::Erase(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}

PinStatus SecurityManager::ManLockingCommand(std::string pin, LockRequest lock_request) {
    return PinStatus::PIN_SUCCESS;
} /* end manlocking command */

PinStatus SecurityManager::Lock(std::string pin) {
    return PinStatus::AUTH_FAILURE;
}

PinStatus SecurityManager::Enable(std::string pin) {
    return PinStatus::AUTH_FAILURE;
}

PinStatus SecurityManager::Disable(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}

PinStatus SecurityManager::Unlock(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}
#else

PinStatus SecurityManager::RetryManLockingCommand(std::string pin, LockRequest lock_request) {
    PinStatus status;
    int number_retries = 0;
    while ((status = ManLockingCommand(pin, lock_request)) == PinStatus::INTERNAL_ERROR) {
        // Retry Lock command unless we reached max retry count
        if (number_retries == MAX_RETRIES) {
            LOG(ERROR) << "PIN command returning INTERNAL_ERROR";//NO_SPELL
            LogRingBuffer::Instance()->makePersistent();
            break;
        } else {
            // Delay for a given amount of time
            VLOG(1) << "Retrying ManLockingCommand";//NO_SPELL
            usleep(SLEEP_TIME);
            // increment retries
            number_retries++;
        }
    }
    return status;
}

PinStatus SecurityManager::Lock(std::string pin) {
    PinStatus status;
    status = RetryManLockingCommand(pin, LockRequest::LOCK);
    return status;
}

PinStatus SecurityManager::Enable(std::string pin) {
    PinStatus status = RetryManLockingCommand(pin, LockRequest::LOPC);
    if (status != PinStatus::PIN_SUCCESS) {
        return status;
    }
    return RetryManLockingCommand(pin, LockRequest::ENABLE);
}

PinStatus SecurityManager::Disable(std::string pin) {
    PinStatus status = RetryManLockingCommand(pin, LockRequest::NO_LOPC);
    if (status != PinStatus::PIN_SUCCESS) {
        return status;
    }
    return RetryManLockingCommand(pin, LockRequest::DISABLE);
}

PinStatus SecurityManager::Unlock(std::string pin) {
    return RetryManLockingCommand(pin, LockRequest::UNLOCK);
}

PinStatus SecurityManager::ManLockingCommand(std::string pin, LockRequest lock_request) {
    /* CNA code added 10/5/2014 for direct calls to Kinetic API library functions */
    char ise_device[32];  /* Holds device id string */
    ks_status  ise_result;
    uint8_t lock_status;
    int Band_Number;

    strcpy(ise_device, FLAGS_store_device.c_str());
    VLOG(1) << "Calling ManLocking code Ver 2";//NO_SPELL
    if (pin.length() == 0) {   /* use NULL as the pin */
        pin = "";
     }
    /* need to initialize our call to the drive */
    ise_result = ks_setup(ise_device);
    /* need to error check to make sure we can talk to this device */
    if (ise_result != KS_SUCCESS) {  /* should return a KS_DISCOVERY_ERROR [9] upon a failure */
        /* This will mean the drive is not SED compatible or
        bands have not been setup on the drive {not initialized] */
        /* use ise_device string here so we save on allocation space */
        LOG(ERROR) << "IE " << ise_result;
        VLOG(1) << "ks_manlocking:ks_setup returned error " << ise_result;
        /* Using this error message since we only have 2 to choose from.... */
        return PinStatus::INTERNAL_ERROR;
    }
    /* So far so good.... make the manlocking call */
    /* parms.. band number = 1, lock type request, pin entered,  get lock_status*/
    /* locking_operation enum is located in ks_manlocking.h
    right now it matches the enum in security_manager.h */
    Band_Number = 1;
    ise_result = ks_manlocking(Band_Number, (locking_operation)lock_request, &pin[0], &lock_status);
    if (ise_result == KS_SUCCESS) {
        VLOG(1) << "ks_manlocking returned Success";//NO_SPELL
    } else if (ise_result == 131) {
        return PinStatus::AUTH_FAILURE;
    } else {
        /* use ise_device string here so we save on allocation space */
        LOG(ERROR) << "IE " << ise_result;
        VLOG(1) << "ks_manlocking: returned error " << ise_result;
        /* Using manlocking return error. Is this ok? these are defined in ks_globals.h CNA */
        return PinStatus::INTERNAL_ERROR;
    }

    // Lock Request
    // lock_status contains valid bits if it gets down here
    switch (lock_request) {
    case LockRequest::LOCK:
        if (((lock_status & 0x04) == 0x04)) {
            return PinStatus::PIN_SUCCESS;
        }
        break;
    case LockRequest::UNLOCK:
        if ((lock_status & 0x04) == 0x0) {
            return PinStatus::PIN_SUCCESS;
        }
        break;
    case LockRequest::ENABLE:
        if ((lock_status & 0x02) == 0x02) {
            return PinStatus::PIN_SUCCESS;
        }
        break;
    case LockRequest::DISABLE:
        if ((lock_status & 0x02) == 0x0) {
            return PinStatus::PIN_SUCCESS;
        }
        break;
    case LockRequest::LOPC:
        if ((lock_status & 0x08) == 0x08) {
            return PinStatus::PIN_SUCCESS;
        }
        break;
    case LockRequest::NO_LOPC:
        if ((lock_status & 0x08) == 0x0) {
            return PinStatus::PIN_SUCCESS;
        }
        break;
    default:
        return PinStatus::INTERNAL_ERROR;
    }
    return PinStatus::INTERNAL_ERROR;
} /* end manlocking command */
#endif

BitStatus SecurityManager::LockQueryCommand() {
    /* CNA code added 10/5/2014 for direct calls to Kinetic API library functions */
    char   ise_device[32];  /* Holds device id string */
    ks_status  ise_result;
    uint8_t  lock_status;
    int      Band_Number;
    strcpy(ise_device, FLAGS_store_device.c_str());
    VLOG(1) << "Calling ManLocking code Ver 2";//NO_SPELL

    /* need to initialize our call to the drive */
    ise_result = ks_setup(ise_device);
    /* need to error check to make sure we can talk to this device */
    if (ise_result != KS_SUCCESS) {
      /* should return a KS_DISCOVERY_ERROR [9] upon a failure */
      /* This will mean the drive is not SED compatible or
      bands have not been setup on the drive {not initialized] */
      /* use ise_device string here so we save on allocation space */
        LOG(ERROR) << "IE " << ise_result;
        VLOG(1) << "ks_manlocking:ks_setup returned error " << ise_result;
        return BitStatus::INTERNAL_ERROR;
      }
    /* So far so good.... make the manlocking call */
    /* parms.. band number = 1, lock_request, pin entered,  get lock_status*/
    Band_Number = 1;
    std::string pin;
    /* use NULL as the pin  getting status only*/
    pin = "";
    ise_result = ks_manlocking(Band_Number, (locking_operation)LockRequest::LOCKQUERY,
      &pin[0], &lock_status);
    if (ise_result == KS_SUCCESS) {
      VLOG(1) << "ks_manlocking returned Success";//NO_SPELL
    } else {
      /* use ise_device string here so we save on allocation space */
      LOG(ERROR) << "IE " << ise_result;
      VLOG(1) << "ks_manlocking: returned error " << ise_result;
      return BitStatus::INTERNAL_ERROR;
    }

    BitStatus status = CheckStatusBits(lock_status);
    switch (status) {
        case BitStatus::LOCKED:
            return status;
        case BitStatus::UNLOCKED:
            return status;
        case BitStatus::INVALID:
            Disable("");
            Unlock("");
            return BitStatus::UNLOCKED;
        default:
            return BitStatus::INTERNAL_ERROR;
    }
}

BitStatus SecurityManager::GetLockStatus() {
    BitStatus status;
    int number_retries = 0;

    // If we encounter an error delay for 1/2 a second and retry untile we recover
    while ((status = LockQueryCommand()) == BitStatus::INTERNAL_ERROR) {
        // If this the fifth failure log and make persistent
        if (number_retries == MAX_RETRIES) {
            LOG(ERROR) << "LockQueryCommand retuning INTERNAL_ERROR";//NO_SPELL
            log_handler_->LogLatency(LATENCY_EVENT_LOG_UPDATE);
            LogRingBuffer::Instance()->makePersistent();
        }
        // sleep for half a second
        usleep(SLEEP_TIME);
        // increment retries
        number_retries++;
    }
    return status;
}

BitStatus SecurityManager::CheckStatusBits(int lock_status) {
    bool lock = ((lock_status & 0x04) == 0x04);
    bool enable = ((lock_status & 0x02) == 0x02);
    bool lopc = ((lock_status & 0x08) == 0x08);

    if (enable && lock && lopc) {
        return BitStatus::LOCKED;
    } else if (!(enable || lock || lopc)) {
        return BitStatus::UNLOCKED;
    } else if (enable && lopc && (!lock)) {
        return BitStatus::UNLOCKED;
    } else {
        return BitStatus::INVALID;
    }
}

void SecurityManager::SetLogHandlerInterface(LogHandlerInterface* log_handler) {
    log_handler_ = log_handler;
}
#else

SecurityManager::SecurityManager() {}

SecurityManager::~SecurityManager() {}

PinStatus SecurityManager::Lock(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}

PinStatus SecurityManager::Enable(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}

PinStatus SecurityManager::Disable(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}

PinStatus SecurityManager::Unlock(std::string pin) {
    return PinStatus::PIN_SUCCESS;
}

BitStatus SecurityManager::GetLockStatus() {
    return BitStatus::UNLOCKED;
}

void SecurityManager::SetLogHandlerInterface(LogHandlerInterface* log_handler) {
    log_handler_ = log_handler;
}
#endif

PinStatus SecurityManager::SetPin(
        std::string new_pin,
        std::string old_pin,
        PinIndex pin_index,
        std::string serial_num,
        bool sed_supported,
        int sector_size,
        uint64_t non_sed_pin_info_sector_num) {
    if (sed_supported) {
        PinStatus status;
        int number_retries = 0;
        while ((status = SetPinTCG(new_pin, old_pin, pin_index)) == PinStatus::INTERNAL_ERROR) {
            // Retry Lock command unless we reached max retry count
            if (number_retries == MAX_RETRIES) {
                LOG(ERROR) << "Set Pin command returning INTERNAL_ERROR";//NO_SPELL
                LogRingBuffer::Instance()->makePersistent();
                break;
            } else {
                // Delay for a given amount of time
                VLOG(1) << "Retrying SetPin command";//NO_SPELL
                usleep(SLEEP_TIME);
                // increment retries
                number_retries++;
            }
        }
        return status;
    } else {
        return SetPinNonSED(new_pin, old_pin, pin_index, serial_num, sector_size, non_sed_pin_info_sector_num);
    }
}

PinStatus SecurityManager::SetPinNonSED(
        std::string new_pin,
        std::string old_pin,
        PinIndex pin_index,
        std::string serial_num,
        int sector_size,
        uint64_t non_sed_pin_info_sector_num) {
    if (pin_index == PinIndex::LOCKPIN) {
        return PinStatus::PIN_SUCCESS;
    }

    unsigned int i;
    int iter = 10000;
    bool unintialized_pin = false;
    NonSedPinInfo drive_pin_info;
    const char non_sed_pin_info_header[] = "01";
    int salt_len = strlen(serial_num.c_str());
    unsigned char *salt = (unsigned char *)serial_num.c_str();
    unsigned char pbkdf2_result[NON_SED_PIN_INFO_PBKDF2_SIZE];
    uint32_t non_sed_pin_info_packet_size = 4096;
    unsigned char non_sed_pin_info_packet[non_sed_pin_info_packet_size];

    memset(non_sed_pin_info_packet, 0, non_sed_pin_info_packet_size);
    //Read pin info packet from the drive
    int fd = open(FLAGS_store_partition.data(), O_RDWR);
    if (fd == -1) {
        VLOG(1) << "Failed to open the store device";
        return PinStatus::INTERNAL_ERROR;
    }
    if (lseek(fd, non_sed_pin_info_sector_num*sector_size, SEEK_SET) == -1) {
        VLOG(1) << "Failed lseek command";
        return PinStatus::INTERNAL_ERROR;
    }
    if (read(fd, non_sed_pin_info_packet, non_sed_pin_info_packet_size) != non_sed_pin_info_packet_size) {
        VLOG(1) << "Failed to read the pin info packet from drive";
        return PinStatus::INTERNAL_ERROR;
    }

    //Fill NonSedPinInfo struct from packet read from the drive
    i = 0;
    memcpy(drive_pin_info.header, non_sed_pin_info_packet, NON_SED_PIN_INFO_HEADER_SIZE);
    i += NON_SED_PIN_INFO_HEADER_SIZE;
    memcpy(drive_pin_info.pbkdf2_hash, &non_sed_pin_info_packet[i], NON_SED_PIN_INFO_PBKDF2_SIZE);
    i += NON_SED_PIN_INFO_PBKDF2_SIZE;
    memcpy(&drive_pin_info.crc32c_hash, &non_sed_pin_info_packet[i], sizeof(drive_pin_info.crc32c_hash));

    /*
    If either the pin info read from the drive does not the have the right header value or
    the crc32c(drive_pin_info.pbkdf2) and drive_pin_info.crc32c_hash does not match,
    pbkdf2(empty_pin) is calculated and used to check against the old pin.
    */
    if (strncmp((const char *)drive_pin_info.header, non_sed_pin_info_header, NON_SED_PIN_INFO_HEADER_SIZE) != 0) {
        VLOG(1) << "Pin info read from the drive has unknown header version";
        unintialized_pin = true;
    }
    if (Value((const char *)drive_pin_info.pbkdf2_hash, NON_SED_PIN_INFO_PBKDF2_SIZE) != drive_pin_info.crc32c_hash) {
        VLOG(1) << "Mismatch in crc32c values";
        unintialized_pin = true;
    }
    if (unintialized_pin) {
        VLOG(1) << "NON-SED erase pin was not initialized on the drive";
        if (PKCS5_PBKDF2_HMAC("", 0, salt, salt_len, iter, EVP_sha512(), NON_SED_PIN_INFO_PBKDF2_SIZE, drive_pin_info.pbkdf2_hash) != 1) {
            VLOG(1) << "Failed to calculate the PBKDF2 of empty pin";
            return PinStatus::INTERNAL_ERROR;
        }
    }

    //pbkdf2(old_pin)
    if (PKCS5_PBKDF2_HMAC(
            old_pin.c_str(),
            strlen(old_pin.c_str()),
            salt,
            salt_len,
            iter,
            EVP_sha512(),
            NON_SED_PIN_INFO_PBKDF2_SIZE,
            pbkdf2_result) != 1) {
        VLOG(1) << "Failed to calculate the PBKDF2 of old pin";
        return PinStatus::INTERNAL_ERROR;
    }
    //Check if pbkdf2(old_pin) matches the content read from the drive
    if (strncmp((const char *)drive_pin_info.pbkdf2_hash, (const char *)pbkdf2_result, NON_SED_PIN_INFO_PBKDF2_SIZE) != 0) {
        return PinStatus::AUTH_FAILURE;
    }
    //pbkdf2(new_pin)
    if (PKCS5_PBKDF2_HMAC(
        new_pin.c_str(),
        strlen(new_pin.c_str()),
        salt,
        salt_len,
        iter,
        EVP_sha512(),
        NON_SED_PIN_INFO_PBKDF2_SIZE,
        pbkdf2_result) != 1) {
        VLOG(1) << "Failed to calculate the PBKDF2 of new pin";
        return PinStatus::INTERNAL_ERROR;
    }

    //Fill NonSedPinInfo struct with new calculated values
    memcpy(drive_pin_info.header, non_sed_pin_info_header, NON_SED_PIN_INFO_HEADER_SIZE);
    memcpy(drive_pin_info.pbkdf2_hash, (const void *)pbkdf2_result, NON_SED_PIN_INFO_PBKDF2_SIZE);
    drive_pin_info.crc32c_hash = Value((const char *)pbkdf2_result, NON_SED_PIN_INFO_PBKDF2_SIZE);

    //Fill non-sed pin info packet
    memset(non_sed_pin_info_packet, 0, non_sed_pin_info_packet_size);
    i = 0;
    memcpy(non_sed_pin_info_packet, drive_pin_info.header, NON_SED_PIN_INFO_HEADER_SIZE);
    i += NON_SED_PIN_INFO_HEADER_SIZE;
    memcpy(&non_sed_pin_info_packet[i], drive_pin_info.pbkdf2_hash, NON_SED_PIN_INFO_PBKDF2_SIZE);
    i += NON_SED_PIN_INFO_PBKDF2_SIZE;
    memcpy(&non_sed_pin_info_packet[i], &drive_pin_info.crc32c_hash, sizeof(drive_pin_info.crc32c_hash));

    //write non-sed pin info packet to the drive
    if (lseek(fd, non_sed_pin_info_sector_num*sector_size, SEEK_SET) == -1) {
        VLOG(1) << "Failed lseek command";
        return PinStatus::INTERNAL_ERROR;
    }
    if (write(fd, non_sed_pin_info_packet, non_sed_pin_info_packet_size) != non_sed_pin_info_packet_size) {
        VLOG(1) << "Failed to write the non-sed pin info packet to the drive";
        return PinStatus::INTERNAL_ERROR;
    }
    /*Failure of fdatasync doesn't necessarily imply that the drive failed to get the erase pin changes.
    It is still possible for the write to have succeeded anyway*/
    if (fdatasync(fd) != 0) {
        VLOG(1) << "Failed to flush cache to the drive to complete erase pin update.";
        return PinStatus::INTERNAL_ERROR;
    } else {
        return PinStatus::PIN_SUCCESS;
    }
}

PinStatus SecurityManager::SetPinTCG(std::string new_pin, std::string old_pin, PinIndex pin_index) {
    /* CNA code added 10/5/2014 for direct calls to Kinetic API library functions */
    char   ise_device[32];  /* Holds device id string */
    ks_status  ise_result;

    strcpy(ise_device, FLAGS_store_device.c_str());
    VLOG(1) << "Calling ks_setpin code Ver 2";//NO_SPELL

    /* need to initialize our call to the drive */
    ise_result = ks_setup(ise_device);
    /* need to error check to make sure we can talk to this device */
    if (ise_result != KS_SUCCESS) {
        /* should return a KS_DISCOVERY_ERROR [9] upon a failure */
        /* This will mean the drive is not SED compatible or
        bands have not been setup on the drive {not initialized] */
        /* use ise_device string here so we save on allocation space */
        LOG(ERROR) << "IE " << ise_result;
        VLOG(1) << "ks_setpin:ks_setup returned error " << ise_result;
        /* Using this error message since we only have 2 to choose from.... */
        return PinStatus::INTERNAL_ERROR;
      }
    /* So far so good.... make the setpin call */
    /* parms.. new pin, old pin, which_pin_to_change[0,1]*/

    ise_result = ks_setpin(&new_pin[0], &old_pin[0], (ks_pin_types)pin_index);
    if (ise_result == KS_SUCCESS) {
      VLOG(1) << "ks_setpin returned Success";//NO_SPELL
      return PinStatus::PIN_SUCCESS;
    } else if (ise_result == 131) {
      return PinStatus::AUTH_FAILURE;
    } else {
      /* use ise_device string here so we save on allocation space */
      LOG(ERROR) << "IE " << ise_result;
      VLOG(1) << "ks_setpin: returned error " << ise_result;
      return PinStatus::INTERNAL_ERROR;  /* Using setpin actual return error.  Is this ok? CNA */
    }
}

// Note: SecurityManager::Erase is invoked only when the drive supports SED
PinStatus SecurityManager::Erase(std::string pin) {
    PinStatus status;
    int number_retries = 0;
    while ((status = EraseTCG(pin)) == PinStatus::INTERNAL_ERROR) {
        // Retry Lock command unless we reached max retry count
        if (number_retries == MAX_RETRIES) {
            LOG(ERROR) << "Erase command returning INTERNAL_ERROR";//NO_SPELL
            LogRingBuffer::Instance()->makePersistent();
            break;
        } else {
            // Delay for a given amount of time
            VLOG(1) << "Retrying Erase command";
            usleep(SLEEP_TIME);
            // increment retries
            number_retries++;
        }
    }
    return status;
}

/* will this call eventually have band array and count in its parameter list? CNA */
PinStatus SecurityManager::EraseTCG(std::string pin) {
    /* CNA code added 9/23/2014 for direct calls to Kinetic API library functions */
    char  ise_device[32];  /* Holds device id string */
    ks_status ise_result;
    int Band_Number[16]; /* we can have up to 16 bands */
    int Band_Count;

    strcpy(ise_device, FLAGS_store_device.c_str());
    VLOG(1) << "Calling erase code Ver 2";//NO_SPELL
    if (pin.length() == 0) {   /* use NULL as the pin */
        pin = "";
    }
    /* need to initialize our call to the drive */
    ise_result = ks_setup(ise_device);
    /* need to error check to make sure we can talk to this device */
    if (ise_result != KS_SUCCESS) {
        /* should return a KS_DISCOVERY_ERROR [9] upon a failure */
        /* This will mean the drive is not SED compatible or
        bands have not been setup on the drive {not initialized] */
        /* use ise_device string here so we save on allocation space */
        LOG(ERROR) << "IE " << ise_result;
        VLOG(1) << "ks_erase: ks_setup returned error " << ise_result;
        /* Using this error message since we only have 2 to choose from.... */
        return PinStatus::INTERNAL_ERROR;
    }
    /* So far so good.... make the erase call */
    Band_Number[0] = 1;  /* At this time we only use band 1 */
    Band_Count = 1;  /* This version only has one band */
    /* parms.. band number = 1, band count = 1, erase_pin entered */
    ise_result = ks_erase(Band_Number, Band_Count, &pin[0]);
    /* check for returned status */
    if (ise_result == KS_SUCCESS) {
        VLOG(1) << "Erase returned Success";
        return PinStatus::PIN_SUCCESS;
    } else if (ise_result == 131) {
        return PinStatus::AUTH_FAILURE;
    } else {
        LOG(ERROR) << "IE " << ise_result;
        VLOG(1) << "ks_erase: returned error " << ise_result;
        return PinStatus::INTERNAL_ERROR;
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
