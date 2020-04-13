#include <iostream>
#include <unistd.h>
#include <string.h>
#include "zoned_ata_standards.h"
#include "includes/drive_helper.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;

namespace qual_kin {

uint8_t* allocate_buffer(size_t data_size, int value) {
    uint8_t* buff = nullptr;
    if (posix_memalign((void**) &buff, kALLIGN, data_size) < 0) {
        std::cout << "error allocating src memory" << std::endl;
        return nullptr;
    }

    memset((void*) buff, value, data_size);
    return buff;
}

size_t read(int fd, uint64_t lba, size_t count, void* buffer) {
    if (lseek(fd, lba << 9, SEEK_SET) == -1) {
        std::cout << "Seek error: " << strerror(errno) << std::endl;
        return -1;
    }

    size_t to_read = count;
    uint8_t* buffer_ptr = static_cast<uint8_t*>(buffer);

    while (to_read > 0) {
        int res = ::read(fd, (void*) buffer_ptr, to_read);
        if (res < 0) {
            std::cout << "Read " << lba << " " << to_read << " failed" << std::endl;
            return 0;
        }
        buffer_ptr += res;
        to_read -= res;
    }

    return count;
}

size_t write(ZacMediator& zac_kin, uint64_t lba, size_t count, void* data) {
    size_t to_write = count;
    uint8_t* data_ptr = static_cast<uint8_t*>(data);

    while (to_write > 0) {
        int res = zac_kin.WriteZone(lba, (void*) data_ptr, to_write);
        if (res < 0) {
            std::cout  << "WriteZone " << lba << " " << to_write << " failed" << std::endl;
            return 0;
        }
        lba += res/512;
        data_ptr += res;
        to_write -= res;
    }

    return count;
}

size_t write_zone(ZacMediator& zac_kin, const uint32_t& zone, size_t data_size, void* data) {
    uint64_t lba;
    size_t to_write = data_size;
    uint8_t* data_ptr = static_cast<uint8_t*>(data);

    if (zac_kin.AllocateZone(zone, &lba) != 0) {
        std::cout << "AllocateZone " << zone << " failed" << std::endl;
        return 0;
    }

    while (to_write > 0) {
        int res = zac_kin.WriteZone(lba, (void*) data_ptr, to_write);
        if (res < 0) {
            std::cout  << "WriteZone " << lba << " " << to_write << " failed" << std::endl;
            return 0;
        }

        lba += res/512;
        data_ptr += res;
        to_write -= res;
    }

    if (zac_kin.FlushCacheAta() != 0) {
        std::cout << "Flush cache failed" << std::endl;
        return 0;
    }

    return data_size;
}

bool corrupt_zone(ZacMediator& zac_kin, const uint32_t& zone) {
    // Arbitrarily using 1 MiB to overwrite the superblocks
    size_t data_size = 1048576;
    uint64_t lba = zone_to_lba(zone);

    void* data = nullptr;
    if (posix_memalign(&data, kALLIGN, data_size) < 0) {
        std::cout << "Error allocating src memory" << std::endl;
        return false;
    }
    memset(data, 69, data_size);

    if (write(zac_kin, lba, data_size, data) <= 0) {
        std::cout << "Corrupt Zone " << zone << " failed" << std::endl;
        return false;
    }

    if (data) {
        free(data);
    }

    return true;
}

bool corrupt_superblocks(ZacMediator& zac_kin) {
    if (!corrupt_zone(zac_kin, kSUPERBLOCK0) || !corrupt_zone(zac_kin, kSUPERBLOCK1)) {
        std::cout << "Corrupt superblock failure!" << std::endl;
        return false;
    }

    return true;
}

size_t read_verify(int& fd, AtaCmdHandler& ata_cmd_handler, uint64_t lba, uint32_t count) {
    sg_io_hdr_t io_hdr;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));

    // Maximum number of LBAs that can be verified at a time is 65,536
    if (count >= 65536) {
        count = 0;
    }

    cdb[0]  |= 0x85;                                // OPERATION_CODE
    cdb[1]  |= (0x3 << 1) | 1;                      // PROTOCOL
    cdb[4]  |= 0x00;                                // FEATURES
    cdb[5]  |= count >> 8;                          // SECTOR_COUNT 16:8
    cdb[6]  |= count & 0xff;                        // SECTOR_COUNT 7:0
    cdb[7]  |= (lba >> 24) & 0xff;                  // LBA_LOW  16:8
    cdb[8]  |= lba & 0xff;                          // LBA_LOW  7:0
    cdb[9]  |= (lba >> 32) & 0xff;                  // LBA_MID  16:8
    cdb[10] |= (lba >>  8) & 0xff;                  // LBA_MID  7:0
    cdb[11] |= (lba >> 40) & 0xff;                  // LBA_HIGH 16:8
    cdb[12] |= (lba >> 16) & 0xff;                  // LBA_HIGH 7:0
    cdb[13] |= 0x40;                                // DEVICE
    cdb[14] |= 0x42;                                // COMMAND

    ata_cmd_handler.ClearDataBuf();
    ata_cmd_handler.ConstructIoHeader(SG_DXFER_NONE, &io_hdr);

    int ret = ata_cmd_handler.ExecuteSgCmd(fd, cdb, sizeof(cdb), io_hdr);

    if (ret != 0) {
        std::cout << "Read verify " << lba << " " << count << " failed" << std::endl;
        print_sensebuf(ata_cmd_handler);
        return 0;
    }

    // The ATA spec states that when count is 0, 65,536 LBAs will be verified
    return count == 0 ? 65536 : count;
}

bool smart_read_data(int& fd, AtaCmdHandler& ata_cmd_handler) {
    sg_io_hdr_t io_hdr;
    size_t buff_size = kSMART_DATA_SIZE;
    uint8_t cdb[16];
    memset(&cdb, 0, sizeof(cdb));
    memset(&io_hdr, 0, sizeof(io_hdr));

    cdb[0]  |= 0x85;            // OPERATION_CODE
    cdb[1]  |= 0x6 << 1;        // PROTOCOL
    cdb[4]  |= 0xD0;            // FEATURES
    cdb[6]  |= 0x01;            // SECTOR_COUNT
    cdb[8]  |= 0x00;            // LBA_LOW
    cdb[10] |= 0x4F;            // LBA_MID
    cdb[12] |= 0xC2;            // LBA_HIGH
    cdb[13] |= 0x00;            // DEVICE
    cdb[14] |= 0xB0;            // COMMAND

    ata_cmd_handler.ClearDataBuf();
    ata_cmd_handler.AllocateDataBuf(buff_size);
    ata_cmd_handler.ConstructIoHeader(SG_DXFER_FROM_DEV, &io_hdr);

    int ret = ata_cmd_handler.ExecuteSgCmd(fd, cdb, sizeof(cdb), io_hdr);

    if (ret != 0) {
        print_sensebuf(ata_cmd_handler);
        return false;
    }

    // Calculate sum
    std::vector<uint8_t>* buff_ptr = ata_cmd_handler.GetDataBufp();
    int8_t sum = 0;
    for (size_t i = 0; i < buff_size - 1; ++i) {
        sum += static_cast<int8_t>((*buff_ptr)[i]);
    }

    // Compare against checksum
    int8_t received_sum = static_cast<int8_t>((*buff_ptr)[buff_size - 1]);
    if (sum + received_sum != 0) {
        printf("Checksum does not match! Calculated %i, Received %i\n", sum, received_sum);
        return false;
    }

    return true;
}

void test_read_write(int fd, ZacMediator& zac_kin) {
    std::cout << "==== Test read write ====" << std::endl;

    uint64_t zone = 20;
    uint64_t lba = zone_to_lba(zone);
    size_t data_size = kZONE_SIZE;

    // Allocate memory for write buffer
    uint8_t* src = allocate_buffer(data_size, 69);
    if (src == nullptr) {
        return;
    }

    // Allocate memory for read buffer
    uint8_t* dst = allocate_buffer(data_size, 0);
    if (dst == nullptr) {
        return;
    }

    // Do write
    if (write(zac_kin, lba, data_size, src) <= 0) {
        return;
    }

    // Do read
    if (read(fd, lba, data_size, (void*) dst) <= 0) {
        std::cout << "Read failed!";
    }

    // Compare write and read buffers
    if (memcmp(src, dst, data_size) != 0) {
        std::cout << "Memory did not match!" << std::endl;
    } else {
        std::cout << "Read matches data written" << std::endl;
    }

    // Clean up memory
    free(src);
    free(dst);
}

void test_write_zone(int fd, ZacMediator& zac_kin) {
    std::cout << "==== Test write zone ====" << std::endl;

    uint32_t zone = 100;
    uint64_t lba = zone_to_lba(zone);
    size_t data_size = kZONE_SIZE;

    // Allocate memory for write buffer
    uint8_t* src = allocate_buffer(data_size, 69);
    if (src == nullptr) {
        return;
    }

    // Allocate memory for read buffer
    uint8_t* dst = allocate_buffer(data_size, 0);
    if (dst == nullptr) {
        return;
    }

    // Do write
    if (write_zone(zac_kin, zone, data_size, src) <= 0) {
        return;
    }

    // Do read
    if (read(fd, lba, data_size, (void*) dst) <= 0) {
        std::cout << "Read failed!" << std::endl;
    }

    // Compare write and read buffers
    if (memcmp(src, dst, data_size) != 0) {
        std::cout << "Memory did not match!" << std::endl;
    } else {
        std::cout << "Read matches data written" << std::endl;
    }

    // Clean up memory
    free(src);
    free(dst);
}

void print_databuf(AtaCmdHandler& ata_cmd_handler, size_t buff_size) {
    std::cout << "Data Buffer:" << std::endl;
    std::vector<uint8_t> *buff_ptr = ata_cmd_handler.GetDataBufp();

    for (size_t i = 0; i < buff_size; ++i) {
        if (i > 0 && i % 32 == 0) {
            printf("\n");
        }
        printf("%02x ", (*buff_ptr)[i]);
    }
    std::cout << std::endl;
}

void print_sensebuf(AtaCmdHandler& ata_cmd_handler) {
    uint8_t* sense_key = ata_cmd_handler.GetSenseBufp();
    std::cout << "Sense buffer: ";
    for (int i = 0; i < 32; ++i) {
        printf("%02x ", *(sense_key + i));
    }
    std::cout << std::endl;
}

}  // namespace qual_kin
