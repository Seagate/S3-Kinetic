#include "gtest/gtest.h"
#include "glog/logging.h"
#include <memory>
#include <fcntl.h>
#include "zac_mediator.h"
#include "command_line_flags.h"

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using ::zac_ha_cmd::ZacZone;

DEFINE_uint64(total_zones, 29809, "Total number of zones that should be on the drive");
DEFINE_uint64(total_conventional_zones, 64, "Total number of conventional on the drive");

void ResetDriveState(ZacMediator *zac_kin) {
    zac_kin->ResetAllZones();
}

uint64_t OpenNumZones(ZacMediator *zac_kin, int number_of_zones, uint64_t start_lba) {
    for (int i = 0; i< number_of_zones; i++) {
        zac_kin->OpenZone(start_lba);
        start_lba += 524288;
    }
    return start_lba;
}

bool TestReportAllZones(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    uint64_t zones = zac_kin->ReportAllZones(0, 0);

    if (zones != FLAGS_total_zones) {
        return false;
    }
    return true;
}

bool TestReportOpenZones(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int number_open_zones;
    int number_expect_open_zones = 100;
    uint64_t start_lba = 64;
    start_lba = start_lba << 19;

    OpenNumZones(zac_kin, number_expect_open_zones, start_lba);

    number_open_zones = zac_kin->ReportOpenZones(0, 0);

    if (number_open_zones != number_expect_open_zones) {
        return false;
    }
    return true;
}

bool TestReportEmptyZones(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    uint64_t number_empty_zones;
    int number_open_zones = 100;
    uint64_t start_lba = 64;
    start_lba = start_lba << 19;

    start_lba = OpenNumZones(zac_kin, number_open_zones, start_lba);

    number_empty_zones = zac_kin->ReportEmptyZones(0, 0);

    if (number_empty_zones !=
        (FLAGS_total_zones - FLAGS_total_conventional_zones - number_open_zones)) {
        return false;
    }

    OpenNumZones(zac_kin, number_open_zones, start_lba);

    number_empty_zones = zac_kin->ReportEmptyZones(0, 0);

    if (number_empty_zones !=
        (FLAGS_total_zones - FLAGS_total_conventional_zones - (2*number_open_zones))) {
        return false;
    }

    return true;
}

bool TestReportClosedZones(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int ret;
    int number_close_zones = 0;
    int number_expected_closed_zones = 1;
    uint64_t start_lba;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    if (ret == -1) {
        return false;
    }

    memset(iobuf, 1, 4096);


    // Need to allocate zone 64 to write to, start of non-conventional zones
    if (zac_kin->AllocateZone(64, &start_lba) != 0) {
        return false;
    }

    if (zac_kin->WriteZone(start_lba, iobuf, iosize) != (int)iosize) {
        return false;
    }

    if (zac_kin->CloseAllZones() != 0) {
        return false;
    }

    number_close_zones = zac_kin->ReportClosedZones(0, 8);

    if (number_close_zones != number_expected_closed_zones) {
        return false;
    }

    if (iobuf) {
        free(iobuf);
    }

    return true;
}

bool TestWritePointerUpdatedAfterWrite(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        return false;
    }

    memset(iobuf, 1, 4096);

    // Write to zone
    // Need to allocate zone, 64 and above non-conventional zones
    if (zac_kin->AllocateZone(64, &wp_lba) != 0) {
        return false;
    }

    if (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 64);

    if (zone.write_pointer != (wp_lba + (iosize/512))) {
        return false;
    }

    if (iobuf) {
        free(iobuf);
    }

    return true;
}

bool TestWritePointerUpdatedAfterWriteEightk(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int ret;
    size_t iosize = 8192;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        return false;
    }

    memset(iobuf, 1, 8192);


    // Write to zone
    // Need to allocate zone, 64 and above non-conventional zones
    if (zac_kin->AllocateZone(64, &wp_lba) != 0) {
        return false;
    }

    if (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 64);

    if (zone.write_pointer != (wp_lba + (iosize/512))) {
        return false;
    }

    if (iobuf) {
        free(iobuf);
    }

    return true;
}

bool TestWritePointerUpdatedAfterWriteForLastZone(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        return false;
    }

    memset(iobuf, 1, 4096);


    // Write to zone
    // Need to allocate zone
    if (zac_kin->AllocateZone(29808, &wp_lba) != 0) {
        return false;
    }

    if (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.write_pointer != (wp_lba + (iosize/512))) {
        return false;
    }

    if (iobuf) {
        free(iobuf);
    }

    return true;
}

bool TestOpenLastZone(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int number_open_zones;
    ZacZone zone;

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->OpenZone(zone.start_lba) != 0) {
        return false;
    }

    number_open_zones = zac_kin->ReportOpenZones(0, 0);

    if (number_open_zones != 1) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x03) {
        return false;
    }

    return true;
}

bool TestCloseLastZone(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int number_close_zones;
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;

    ZacZone zone;

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->CloseZone(zone.start_lba) != 0) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x01) {
        return false;
    }

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        return false;
    }

    memset(iobuf, 1, 4096);

    // Write to zone
    // Need to allocate zone
    if (zac_kin->AllocateZone(29808, &wp_lba) != 0) {
        return false;
    }

    if (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->CloseZone(zone.start_lba) != 0) {
        return false;
    }

    number_close_zones = zac_kin->ReportClosedZones(0, 0);

    if (number_close_zones != 1) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x04) {
        return false;
    }

    if (iobuf) {
        free(iobuf);
    }

    return true;
}

bool TestFinishLastZone(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int number_finish_zones;
    ZacZone zone;

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->OpenZone(zone.start_lba) != 0) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x03) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->FinishZone(zone.start_lba) != 0) {
        return false;
    }

    number_finish_zones = zac_kin->ReportFullZones(0, 0);

    if (number_finish_zones != 1) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x0E) {
        return false;
    }

    return true;
}

bool TestOpenLastZoneReportLastLBA(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int number_open_zones;
    ZacZone zone;

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->OpenZone(zone.start_lba) != 0) {
        return false;
    }

    number_open_zones = zac_kin->ReportOpenZones(zone.start_lba, 0);

    if (number_open_zones != 1) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x03) {
        return false;
    }

    return true;
}

bool TestCloseLastZoneReportLastLBA(ZacMediator *zac_kin) {
    ResetDriveState(zac_kin);
    int number_close_zones;
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;

    ZacZone zone;

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->CloseZone(zone.start_lba) != 0) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x01) {
        return false;
    }

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        return false;
    }

    memset(iobuf, 1, 4096);

    // Write to zone
    // Need to allocate zone
    if (zac_kin->AllocateZone(29808, &wp_lba) != 0) {
        return false;
    }

    if (zac_kin->WriteZone(wp_lba, iobuf, iosize) != (int)iosize) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->CloseZone(zone.start_lba) != 0) {
        return false;
    }

    number_close_zones = zac_kin->ReportClosedZones(zone.start_lba, 0);

    if (number_close_zones != 1) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x04) {
        return false;
    }

    if (iobuf) {
        free(iobuf);
    }

    return true;
}

bool TestFinishLastZoneReportLastLBA(ZacMediator *zac_kin) {
    int number_finish_zones;
    ZacZone zone;

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->OpenZone(zone.start_lba) != 0) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x03) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zac_kin->FinishZone(zone.start_lba) != 0) {
        return false;
    }

    number_finish_zones = zac_kin->ReportFullZones(zone.start_lba, 0);

    if (number_finish_zones != 1) {
        return false;
    }

    zac_kin->GetZoneInfo(&zone, 29808);

    if (zone.zone_condition != 0x0E) {
        return false;
    }

    return true;
}

void RunTests(ZacMediator *zac_kin) {
    int failures = 0;
    //Report all zones
    if (!TestReportAllZones(zac_kin)) {
        printf("Failed ReportAllZones\n");
        failures++;
    }
    if (!TestReportOpenZones(zac_kin)) {
        printf("Failed TestReportOpenZones\n");
        failures++;
    }
    if (!TestReportEmptyZones(zac_kin)) {
        printf("Failed TestReportEmptyZones\n");
        failures++;
    }
    if (!TestReportClosedZones(zac_kin)) {
        printf("Failed TestReportClosedZones\n");
        failures++;
    }
    if (!TestWritePointerUpdatedAfterWrite(zac_kin)) {
        printf("Failed TestWritePointerUpdatedAfterWrite\n");
        failures++;
    }
    if (!TestWritePointerUpdatedAfterWriteEightk(zac_kin)) {
        printf("Failed TestWritePointerUpdatedAfterWriteEightk\n");
        failures++;
    }
    if (!TestWritePointerUpdatedAfterWriteForLastZone(zac_kin)) {
        printf("Failed TestWritePointerUpdatedAfterWriteForLastZone\n");
        failures++;
    }
    if (!TestOpenLastZone(zac_kin)) {
        printf("Failed TestOpenLastZone\n");
        failures++;
    }
    if (!TestCloseLastZone(zac_kin)) {
        printf("Failed TestCloseLastZone\n");
        failures++;
    }
    if (!TestFinishLastZone(zac_kin)) {
        printf("Failed TestFinishLastZone\n");
        failures++;
    }
    if (!TestOpenLastZoneReportLastLBA(zac_kin)) {
        printf("Failed TestOpenLastZoneReportLastLBA\n");
        failures++;
    }
    if (!TestCloseLastZoneReportLastLBA(zac_kin)) {
        printf("Failed TestCloseLastZoneReportLastLBA\n");
        failures++;
    }
    if (!TestFinishLastZoneReportLastLBA(zac_kin)) {
        printf("Failed TestFinishLastZoneReportLastLBA\n");
        failures++;
    }

    printf("Failed tests: %i\n", failures);
}

bool TestWriteZoneLargeValue(ZacMediator *zac_kin, const char * device_path) {
    ResetDriveState(zac_kin);
    int ret;
    size_t iosize = 1048576;
    size_t ioalign = 4096;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    unsigned int zone_id = ZacMediator::kLAST_CONVENTIONAL_ZONE_ID + 1;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);

    if (ret == -1) {
        printf("Failed posix_memalign\n");
        return false;
    }

    memset(iobuf, 3, 1048576);

    // Write to zone
    // Need to allocate zone
    if (zac_kin->AllocateZone(zone_id, &wp_lba) != 0) {
        printf("Failed to AllocateZone\n");
        return false;
    }

    int bytes_written = 0;

    while (bytes_written < (int)iosize) {
        int status = zac_kin->WriteZone(wp_lba+(bytes_written/512),
                                        iobuf, (iosize - bytes_written));

        if (status == -1) {
            printf("WriteZone failed\n");
            break;
        }
        iobuf += status;
        bytes_written += status;
    }

    zac_kin->GetZoneInfo(&zone, zone_id);

    if (zone.write_pointer != (wp_lba + (iosize/512))) {
        printf("Zone.write_pointer not correct\n");
        return false;
    }

    if (zone.non_seq != 0) {
        printf("non_seq bit set");
        return false;
    }

    int fd = open(device_path, (O_DIRECT | O_RDWR));

    if (fd < 0) {
        printf("invalid file descriptor\n");
        return false;
    }

    uint64_t start_lba = zone.start_lba;
    lseek(fd, start_lba*512, SEEK_SET);

    uint8_t *readbuf = NULL;

    ret = posix_memalign((void **) &readbuf, ioalign, iosize);

    if (ret == -1) {
        printf("Failed posix_memalign\n");
        return false;
    }
    size_t status = read(fd, readbuf, iosize);

    if (status < iosize) {
        printf("Read Failed\n");
        return false;
    }

    // Reset iobuf to start location, so we can compare
    iobuf = iobuf - iosize;
    for (int i = 0; i < 1048576; i++) {
        if (readbuf[i] != iobuf[i]) {
            printf("DO NOT MATCH\n");
            printf("readbuf[i]: %u\n", readbuf[i]);
            printf("iobuf[i]: %u\n", iobuf[i]);
            return false;
        }
    }

    if (iobuf) {
        free(iobuf);
    }

    if (readbuf) {
        free(readbuf);
    }
    return true;
}

int main(int argc, char *argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;
    FLAGS_stderrthreshold = google::INFO;
    char *device_path;

    if (argc != 2) {
        std::cout << "INCORRECT AMOUNT OF ARGUMENTS" << std::endl;
        exit(EXIT_FAILURE);
    }

    device_path = argv[1];

    std::shared_ptr<AtaCmdHandler> zac_ata(new AtaCmdHandler());
    std::unique_ptr<ZacMediator> zac_kin(new ZacMediator(zac_ata.get()));
    zac_kin->OpenDevice(device_path);

    RunTests(zac_kin.get());
    bool status = TestWriteZoneLargeValue(zac_kin.get(), device_path);
    if (!status) {
        printf("TestWriteZoneLargeValue Failed\n");
    }

    zac_kin->CloseDevice();
    return 0;
}
