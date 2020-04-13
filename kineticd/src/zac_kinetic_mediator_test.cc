#include "gtest/gtest.h"
#include "glog/logging.h"
#include "zac_mediator.h"
#include "command_line_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using ::zac_ha_cmd::ZacZone;

DEFINE_uint64(total_zones, 29809, "Total number of zones that should be on the drive");
DEFINE_uint64(total_conventional_zones, 64, "Total number of conventional on the drive");

class ZacKineticMediatorTest : public ::testing::Test {
    protected:
    ZacKineticMediatorTest() : zac_kin(&ata_cmd_hdlr) {}

    virtual void SetUp() {
        zac_kin.OpenDevice(FLAGS_store_test_device);
        zac_kin.ResetAllZones();
    }

    virtual void TearDown() {
        zac_kin.CloseDevice();
    }

    ZacMediator zac_kin;
    AtaCmdHandler ata_cmd_hdlr;
};

uint64_t open_num_zones(ZacMediator *zac_kin, int number_of_zones, uint64_t start_lba) {
    for (int i = 0; i< number_of_zones; i++) {
        zac_kin->OpenZone(start_lba);
        start_lba += 524288;
    }
    return start_lba;
}

TEST_F(ZacKineticMediatorTest, ReportAllZones) {
    uint64_t zones = zac_kin.ReportAllZones(0, 8);
    EXPECT_EQ(FLAGS_total_zones, zones);
}

TEST_F(ZacKineticMediatorTest, ResetAllZones) {
    zac_kin.ResetAllZones();
    uint64_t zones = zac_kin.ReportEmptyZones(0, 8);
    EXPECT_EQ((FLAGS_total_zones - FLAGS_total_conventional_zones), zones);
}

TEST_F(ZacKineticMediatorTest, ReportOpenZones) {
    int number_open_zones;
    int number_zones = 100;
    uint64_t start_lba = (ZacMediator::kLAST_CONVENTIONAL_ZONE_ID + 1);
    start_lba = start_lba << 19;

    open_num_zones(&zac_kin, number_zones, start_lba);

    number_open_zones = zac_kin.ReportOpenZones(0, 0);

    EXPECT_EQ(number_zones, number_open_zones);
}

TEST_F(ZacKineticMediatorTest, ReportEmptyZones) {
    uint64_t number_empty_zones;
    int number_zones = 100;
    int start_lba = (ZacMediator::kLAST_CONVENTIONAL_ZONE_ID + 1);
    start_lba = start_lba << 19;

    start_lba = open_num_zones(&zac_kin, number_zones, start_lba);

    number_empty_zones = zac_kin.ReportEmptyZones(0, 0);

    EXPECT_EQ((FLAGS_total_zones - FLAGS_total_conventional_zones - number_zones),
        number_empty_zones);

    open_num_zones(&zac_kin, number_zones, start_lba);

    number_empty_zones = zac_kin.ReportEmptyZones(0, 0);

    EXPECT_EQ((FLAGS_total_zones - FLAGS_total_conventional_zones - (2*number_zones)),
        number_empty_zones);
}

TEST_F(ZacKineticMediatorTest, ReportClosedZones) {
    int ret;
    int number_close_zones = 0;
    int number_expected_closed_zones = 1;
    uint64_t start_lba;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    unsigned int zone_id = ZacMediator::kLAST_CONVENTIONAL_ZONE_ID + 1;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    ASSERT_NE(-1, ret);

    memset(iobuf, 1, 4096);


    // Need to allocate zone 0 to write to
    ASSERT_EQ(0, zac_kin.AllocateZone(zone_id, &start_lba));
    ASSERT_EQ((int)iosize, zac_kin.WriteZone(start_lba, iobuf, iosize));

    zac_kin.CloseAllZones();

    number_close_zones = zac_kin.ReportClosedZones(0, 8);

    EXPECT_EQ(number_expected_closed_zones, number_close_zones);

    free(iobuf);
}

TEST_F(ZacKineticMediatorTest, WritePointerUpdatedAfterWrite) {
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    unsigned int zone_id = ZacMediator::kLAST_CONVENTIONAL_ZONE_ID + 1;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    ASSERT_NE(-1, ret);

    memset(iobuf, 1, 4096);


    // Write to zone
    // Need to allocate zone
    ASSERT_EQ(0, zac_kin.AllocateZone(zone_id, &wp_lba));
    ASSERT_EQ((int)iosize, zac_kin.WriteZone(wp_lba, iobuf, iosize));

    zac_kin.GetZoneInfo(&zone, zone_id);

    EXPECT_EQ(wp_lba + (iosize/512), zone.write_pointer);

    free(iobuf);
}

TEST_F(ZacKineticMediatorTest, WritePointerUpdatedAfterWriteEightk) {
    int ret;
    size_t iosize = 8192;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    unsigned int zone_id = ZacMediator::kLAST_CONVENTIONAL_ZONE_ID + 1;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    ASSERT_NE(-1, ret);

    memset(iobuf, 1, 8192);


    // Write to zone
    // Need to allocate zone
    ASSERT_EQ(0, zac_kin.AllocateZone(zone_id, &wp_lba));
    ASSERT_EQ((int)iosize, zac_kin.WriteZone(wp_lba, iobuf, iosize));

    zac_kin.GetZoneInfo(&zone, zone_id);

    EXPECT_EQ(wp_lba + (iosize/512), zone.write_pointer);

    free(iobuf);
}

TEST_F(ZacKineticMediatorTest, WritePointerUpdatedAfterWriteForLastZone) {
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;
    ZacZone zone;

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    ASSERT_NE(-1, ret);

    memset(iobuf, 1, 4096);


    // Write to zone
    // Need to allocate zone
    zac_kin.AllocateZone(29808, &wp_lba);
    zac_kin.WriteZone(wp_lba, iobuf, iosize);

    zac_kin.GetZoneInfo(&zone, 29808);

    EXPECT_EQ(wp_lba + (iosize/512), zone.write_pointer);

    free(iobuf);
}

TEST_F(ZacKineticMediatorTest, OpenLastZone) {
    int number_open_zones;
    ZacZone zone;

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.OpenZone(zone.start_lba));
    number_open_zones = zac_kin.ReportOpenZones(0, 0);
    EXPECT_EQ(1, number_open_zones);

    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x03, zone.zone_condition);
}

TEST_F(ZacKineticMediatorTest, CloseLastZone) {
    int number_close_zones;
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;

    ZacZone zone;

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.CloseZone(zone.start_lba));
    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x01, zone.zone_condition);

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    ASSERT_NE(-1, ret);

    memset(iobuf, 1, 4096);

    // Write to zone
    // Need to allocate zone
    ASSERT_EQ(0, zac_kin.AllocateZone(29808, &wp_lba));
    ASSERT_EQ((int)iosize, zac_kin.WriteZone(wp_lba, iobuf, iosize));

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.CloseZone(zone.start_lba));

    number_close_zones = zac_kin.ReportClosedZones(0, 0);
    EXPECT_EQ(1, number_close_zones);

    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x04, zone.zone_condition);

    free(iobuf);
}

TEST_F(ZacKineticMediatorTest, FinishLastZone) {
    int number_finish_zones;
    ZacZone zone;

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.OpenZone(zone.start_lba));
    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x03, zone.zone_condition);

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.FinishZone(zone.start_lba));

    number_finish_zones = zac_kin.ReportFullZones(0, 0);
    EXPECT_EQ(1, number_finish_zones);

    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x0E, zone.zone_condition);
}

TEST_F(ZacKineticMediatorTest, OpenLastZoneReportLastLBA) {
    int number_open_zones;
    ZacZone zone;

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.OpenZone(zone.start_lba));
    number_open_zones = zac_kin.ReportOpenZones(zone.start_lba, 0);
    EXPECT_EQ(1, number_open_zones);

    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x03, zone.zone_condition);
}

TEST_F(ZacKineticMediatorTest, CloseLastZoneReportLastLBA) {
    int number_close_zones;
    int ret;
    size_t iosize = 4096;
    size_t ioalign = 512;
    uint8_t *iobuf = NULL;
    uint64_t wp_lba;

    ZacZone zone;

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.CloseZone(zone.start_lba));
    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x01, zone.zone_condition);

    ret = posix_memalign((void **) &iobuf, ioalign, iosize);
    ASSERT_NE(-1, ret);

    memset(iobuf, 1, 4096);

    // Write to zone
    // Need to allocate zone
    ASSERT_EQ(0, zac_kin.AllocateZone(29808, &wp_lba));
    ASSERT_EQ((int)iosize, zac_kin.WriteZone(wp_lba, iobuf, iosize));

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.CloseZone(zone.start_lba));

    number_close_zones = zac_kin.ReportClosedZones(zone.start_lba, 0);
    EXPECT_EQ(1, number_close_zones);

    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x04, zone.zone_condition);

    free(iobuf);
}

TEST_F(ZacKineticMediatorTest, FinishLastZoneReportLastLBA) {
    int number_finish_zones;
    ZacZone zone;

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.OpenZone(zone.start_lba));
    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x03, zone.zone_condition);

    zac_kin.GetZoneInfo(&zone, 29808);
    ASSERT_EQ(0, zac_kin.FinishZone(zone.start_lba));

    number_finish_zones = zac_kin.ReportFullZones(zone.start_lba, 0);
    EXPECT_EQ(1, number_finish_zones);

    zac_kin.GetZoneInfo(&zone, 29808);
    EXPECT_EQ(0x0E, zone.zone_condition);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
