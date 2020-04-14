#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <memory>

#include "glog/logging.h"
#include "openssl/rand.h"

#include "authorizer.h"
#include "command_line_flags.h"
#include "connection_handler.h"
#include "device_information.h"
#include "file_system_store.h"
#include "hmac_authenticator.h"
#include "key_value_store.h"
#include "message_processor.h"
#include "openssl_initialization.h"
#include "primary_store.h"
#include "server.h"
#include "signal_handling.h"
#include "user_store.h"
#include "value_factory.h"
#include "ring_buffer_log_sink.h"
#include "spliceable_value.h"
#include "instant_secure_eraser.h"
#include "request_context.h"
#include "limits.h"
#include "launch_monitor.h"
#include "stack_trace.h"
#include "smrdisk/Disk.h"

using com::seagate::kinetic::Authorizer;
using com::seagate::kinetic::DeviceInformation;
using com::seagate::kinetic::FileSystemStore;
using com::seagate::kinetic::KeyValueStore;
using com::seagate::kinetic::MessageProcessor;
using com::seagate::kinetic::PrimaryStore;
using com::seagate::kinetic::Profiler;
using com::seagate::kinetic::proto::Message;
using com::seagate::kinetic::SkinnyWaist;
using com::seagate::kinetic::UserStore;
using com::seagate::kinetic::DeviceInformationInterface;
using com::seagate::kinetic::InstantSecureEraserX86;
using com::seagate::kinetic::InstantSecureEraserARM;
using com::seagate::kinetic::PrimaryStoreValue;
using com::seagate::kinetic::CautiousFileHandlerInterface;
using com::seagate::kinetic::CautiousFileHandler;
using com::seagate::kinetic::ClusterVersionStore;
using com::seagate::kinetic::LaunchMonitorPassthrough;

using ::kinetic::MessageStreamFactory;
using ::kinetic::IncomingStringValue;
using com::seagate::kinetic::Limits;
using std::string;
using std::unique_ptr;

DEFINE_uint64(keys_to_write, 10000, "The number of keys to write");
DEFINE_uint64(key_size, 4000, "How big the keys should be ");

bool kineticd_idle = true;

int main(int argc, char *argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    google::SetUsageMessage("drive_filler_upper");
    google::ParseCommandLineFlags(&argc, &argv, true);

    com::seagate::kinetic::Limits limits(4000, 128, 2048, FLAGS_max_message_size_bytes,
            FLAGS_max_message_size_bytes, 100, 30, 20, 200, 100, 5, FLAGS_max_batch_size, FLAGS_max_deletes_per_batch);

    LaunchMonitorPassthrough launch_monitor;
    launch_monitor.LoadMonitor();

    LOG(INFO) << "Opening LevelDB DB at " << FLAGS_primary_db_path;//NO_SPELL

    CHECK_GT(FLAGS_table_cache_size, 0);
    KeyValueStore primary_data_store(FLAGS_primary_db_path, FLAGS_table_cache_size);
    if (!primary_data_store.Init(true)) {
        LOG(ERROR) << "Unable to initialize primary store";
    } else {
        LOG(INFO) << "Initialized primary store";
    }

    unique_ptr<CautiousFileHandlerInterface> file_handler(
            new CautiousFileHandler(FLAGS_metadata_db_path, "users.json"));

    Profiler profiler;
    FileSystemStore file_system_store(FLAGS_file_store_path);
    if (!file_system_store.Init(true)) {
        LOG(ERROR) << "Failed to initialize file system store";
        exit(EXIT_FAILURE);
    }
    UserStore user_store(std::move(file_handler), limits);
    Authorizer authorizer(user_store, profiler, limits);
    DeviceInformation device_information(authorizer, FLAGS_primary_db_path,
        "/proc/stat", "sda", "/sys/devices/platform/axp-temp.0/", FLAGS_preused_file_path,
        FLAGS_kineticd_start_log);

    #if BUILD_FOR_ARM == 1
    InstantSecureEraserARM instant_secure_eraser(FLAGS_store_mountpoint,
                                                 FLAGS_store_partition,
                                                 FLAGS_store_device,
                                                 FLAGS_metadata_mountpoint,
                                                 FLAGS_metadata_partition);
    #else
    InstantSecureEraserX86 instant_secure_eraser(FLAGS_primary_db_path, FLAGS_file_store_path);
    #endif

    unique_ptr<CautiousFileHandlerInterface> version_file_handler(
            new CautiousFileHandler(FLAGS_cluster_version_path, "version"));
    ClusterVersionStore cluster_version_store(move(version_file_handler));
    if (!cluster_version_store.Init()) {
        LOG(ERROR) << "Failed to initialize the cluster version store";
        exit(EXIT_FAILURE);
    }

    PrimaryStore primary_store(file_system_store, primary_data_store, cluster_version_store,
        device_information, profiler, FLAGS_file_store_minimum_size, instant_secure_eraser,
        FLAGS_preused_file_path);

    SkinnyWaist skinny_waist(FLAGS_primary_db_path,
            FLAGS_store_partition,
            FLAGS_store_mountpoint,
            FLAGS_metadata_partition,
            FLAGS_metadata_mountpoint,
            authorizer,
            user_store,
            primary_store,
            profiler,
            cluster_version_store,
            launch_monitor);

    // If this is a new database there won't be a Demo user so there won't
    // be any way to talk to this server. To avoid this nightmare scenario
    // we make sure there's a demo user
    if (!user_store.DemoUserExists()) {
        CHECK(user_store.CreateDemoUser());
    }

    char* key_buf = new char[FLAGS_key_size];

    for (uint64_t i = 0; i < FLAGS_keys_to_write; i++) {
        if (!(i % 1000)) {
            LOG(INFO) << "Finished " << i;
        }

        RAND_pseudo_bytes((unsigned char*)key_buf, FLAGS_key_size);
        string key(key_buf, FLAGS_key_size);

        PrimaryStoreValue psv;
        psv.version = key;
        psv.tag = key;


        IncomingStringValue incoming_value("");

        com::seagate::kinetic::RequestContext request_context;
        std::tuple<int64_t, int64_t> token {0, 0};//NOLINT
        skinny_waist.Put(1, key, "", psv, &incoming_value, true, false, request_context, token);
    }

    delete[] key_buf;

    return 0;
}
