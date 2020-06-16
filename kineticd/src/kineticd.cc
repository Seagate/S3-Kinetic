#include <signal.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>

#include "flag_vars.h"
#include "glog/logging.h"
#include "leveldb/status.h"
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
#include "version_info.h"
#include "ring_buffer_log_sink.h"
#include "p2p_request_manager.h"
#include "threadsafe_blocking_queue.h"
#include "instant_secure_eraser.h"
#include "network_interfaces.h"
#include "getlog_handler.h"
#include "setup_handler.h"
#include "pinop_handler.h"
#include "power_manager.h"
#include "limits.h"
#include "mount_manager.h"
#include "security_manager.h"
#include "kinetic_state.h"
#include "thread.h"
#include "security_handler.h"
#include "launch_monitor.h"
#include "smrdisk/Disk.h"
#include "kinetic_alarms.h"
#include "pending_cmd_list_proxy.h"
#include "aging_timer.h"
#include "CommandValidator.h"
#include "popen_wrapper.h"
#include "drive_info.h"
#include "getlog_handler.h"
#include "skinny_waist.h"

using namespace com::seagate::kinetic; //NOLINT
using ::kinetic::MessageStreamFactory;
using com::seagate::kinetic::SecurityManager;
using com::seagate::kinetic::STATIC_DRIVE_INFO;

using std::unique_ptr;
using std::move;
using std::string;

int initDone = 0;

//extern KeyValueStore *kvstore__;
extern SkinnyWaist *pskinny_waist__;

extern "C" void* InitMain(int argc, char* argv[]); //struct arg *argu);


void write_stack_trace(const char* data, int size) {
    // This function gets called once per line of the stack trace, so we need to
    // append the line to the stack_trace string and keep rewriting it to the file
    static bool bLogPersisted = false;
    string stackTraceFilePath(FLAGS_util_path + "/stack_trace.txt");
    if (!bLogPersisted) {
        LogRingBuffer::Instance()->makePersistent();
        string cmd("cp ");
        cmd += FLAGS_util_path + "/" + LogRingBuffer::Instance()->getLogFilePath() + " " + stackTraceFilePath;
        system(cmd.c_str());
        bLogPersisted = true;
    }
    string stack_trace;
    stack_trace.append(data, size);
    ofstream trace_file;
    trace_file.open(stackTraceFilePath.c_str(), ios_base::out | ios_base::app);
    trace_file << stack_trace;
    trace_file.close();
}

void updateKineticStartCounter() {
    string filePath(FLAGS_kineticd_start_log);
    int nCount = 0;
    string line;
    ifstream iStream(filePath);
    if (iStream.good() && getline(iStream, line)) {
        nCount = std::atoi(line.c_str());
    }
    iStream.close();
    ++nCount;
    ofstream oStream(filePath);
    if (oStream.good()) {
        oStream << nCount << endl;
    }
    oStream.close();
}


void* InitMain(int argc, char* argv[]) { //struct arg *arg) {
    cout << "1. INIT MAIN" << endl;
    int i = argc;
    for (int j = 0; j < i; j++) {
       cout << " J = " << j << " " << argv[j] << endl;
    }
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    cout << "2. INIT MAIN" << endl;

#ifndef PRODUCT_X86
    google::InstallFailureWriter(&write_stack_trace);
#endif

    // LogRingBuffer log_ring_buffer(1024);
    RingBufferLogSink sink(1024);
    google::AddLogSink(&sink);
    updateKineticStartCounter();
    FLAGS_logtostderr = 1;

    GOOGLE_PROTOBUF_VERIFY_VERSION;
    pid_t tid;
    tid = syscall(SYS_gettid);
    cout << " MAIN THREAD ID " << tid << endl;

    std::string version = std::string("Seagate Technology   PLC\nCopyright  2013\nKinetic ")
        + CURRENT_SEMANTIC_VERSION;

    printf("%s\n", version.c_str());

    google::SetUsageMessage("Kinetic server.");
    google::ParseCommandLineFlags(&argc, &argv, true);

    string dev = FLAGS_store_partition;
    DriveEnv::getInstance()->storePartition(FLAGS_store_partition);
    FLAGS_store_device = "";
    for (char c : FLAGS_store_partition) {
        if (!std::isdigit(c)) FLAGS_store_device += c;
    }
    int dev_pos = dev.find("sd");
    dev = dev.substr(dev_pos);
    string system_command = "echo 1024 > /sys/block/" + dev + "/queue/max_sectors_kb";
    if (!execute_command(system_command)) {
        LOG(WARNING) << "Failed to set max size per read/write request";
    }
    Flag_vars::FLG_socket_timeout = FLAGS_socket_timeout;

    LogRingBuffer::Instance()->parseAndSetStatusCodes(FLAGS_status_codes);
    // Set log file paths
    LogRingBuffer::Instance()->setLogFilePaths(FLAGS_log_file_path, FLAGS_old_log_file_path,
        FLAGS_command_log_file_path, FLAGS_old_command_log_file_path,
        FLAGS_key_value_log_file_path, FLAGS_old_key_value_log_file_path,
        FLAGS_key_size_log_file_path, FLAGS_old_key_size_log_file_path,
        FLAGS_value_size_log_file_path, FLAGS_old_value_size_log_file_path,
        FLAGS_stale_data_log_file_path, FLAGS_old_stale_data_log_file_path);

    LOG(INFO) << "Starting Kinetic Server";

    LOG(INFO) << "Built on " << BUILD_DATE;
    LOG(INFO) << "Git hash " << GIT_HASH;
    LOG(INFO) << "Version " << CURRENT_SEMANTIC_VERSION;

    // Populate Static Drive Attributes
    STATIC_DRIVE_INFO static_drive_info = PopulateStaticDriveAttributes(FLAGS_store_device);
    if (static_drive_info.is_present) {
        LOG(INFO) << "Storage Device is present";
        LOG(INFO) << "SN: " << static_drive_info.drive_sn;
        LOG(INFO) << "FW: " << static_drive_info.drive_fw;
        LOG(INFO) << "WWN: " << static_drive_info.drive_wwn;
        LOG(INFO) << "Model: " << static_drive_info.drive_model;
        LOG(INFO) << "Vendor: " << static_drive_info.drive_vendor;
        LOG(INFO) << "SSD: " << (static_drive_info.is_SSD ? "YES" : "NO");
        LOG(INFO) << "SED: " << (static_drive_info.supports_SED ? "YES" : "NO");
        LOG(INFO) << "ZAC: " << (static_drive_info.supports_ZAC ? "YES" : "NO");
        LOG(INFO) << "Physical Sector size: " << \
        (static_drive_info.logical_sectors_per_physical_sector * static_drive_info.sector_size);
        LOG(INFO) << "Logical Sector Size: " << static_drive_info.sector_size;
        LOG(INFO) << "Capacity in Bytes: " << static_drive_info.drive_capacity_in_bytes;
        LOG(INFO) << "Sectors read at power on: " << static_drive_info.sectors_read_at_poweron;
        LOG(INFO) << "Non-SED erase pin info sector number: " << static_drive_info.non_sed_pin_info_sector_num;
    }

    com::seagate::kinetic::Limits limits(1024, 128, 2048, FLAGS_max_message_size_bytes, FLAGS_max_message_size_bytes,
            100, 30, 20, 800, 1000, 32, FLAGS_max_batch_size, FLAGS_max_deletes_per_batch);

    unique_ptr<CautiousFileHandlerInterface> launch_monitor_file_handler(
                new CautiousFileHandler(FLAGS_util_path, "launch_check.txt"));
    LaunchMonitor launch_monitor(std::move(launch_monitor_file_handler));
    launch_monitor.LoadMonitor();
    smr::Disk::initializeSuperBlockAddr(FLAGS_store_partition);
    VLOG(1) << "Superblock 0 zone# = " << smr::Disk::SUPERBLOCK_0_ADDR/(1024*1024);//NO_SPELL
    VLOG(1) << "Superblock 1 zone# = " << smr::Disk::SUPERBLOCK_1_ADDR/(1024*1024);//NO_SPELL
    KeyValueStore primary_data_store(FLAGS_store_partition,
                FLAGS_table_cache_size,
                FLAGS_block_size,
                FLAGS_sst_size);

    // If we attempt to write to a socket whose peer has disconnected, we get a
    // SIGPIPE signal. We ignore it here, because it is much easier to check
    // the return value than to handle the signal correctly.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::cerr << "Failed to ignore SIGPIPE" << std::endl;
        exit(EXIT_FAILURE);
    }
    KineticAlarms* kinetic_alarms = KineticAlarms::Instance();
    (*kinetic_alarms).set_system_alarm(true);
    com::seagate::kinetic::SetKeyValueStoreInSignal(&primary_data_store);
    com::seagate::kinetic::configure_signal_handling(kinetic_alarms);
    int signal_pipe_fd = com::seagate::kinetic::get_signal_notification_pipe();

    CHECK_GT(FLAGS_table_cache_size, 0);
    primary_data_store.SetStoreMountPoint(FLAGS_store_mountpoint);

    unique_ptr<CautiousFileHandlerInterface> user_file_handler(
            new CautiousFileHandler(FLAGS_metadata_db_path, "users.json"));

    Profiler profiler;
    FileSystemStore file_system_store(FLAGS_file_store_path);
    UserStore user_store(std::move(user_file_handler), limits);
    Authorizer authorizer(user_store, profiler, limits);

    size_t idx = FLAGS_store_device.find_last_of('/');
    string sDevice = FLAGS_store_device.substr(idx + 1);
    DeviceInformation device_information(authorizer, FLAGS_primary_db_path,
        FLAGS_proc_stat_path, sDevice, FLAGS_sysfs_temperature_dir, FLAGS_preused_file_path, FLAGS_kineticd_start_log,
        static_drive_info.drive_capacity_in_bytes);
    NetworkInterfaces network_interfaces;

    InstantSecureEraser instant_secure_eraser(FLAGS_store_mountpoint,
                                                 FLAGS_store_partition,
                                                 FLAGS_store_device,
                                                 FLAGS_metadata_mountpoint,
                                                 FLAGS_metadata_partition,
                                                 FLAGS_store_partition,
                                                 FLAGS_file_store_path);
    device_information.LoadDriveIdentification();
    // Following 3 Vars: for pinop handler to unmount/mount metadata
    // during lock, unlock
    const string pinop_lock_umount_point = FLAGS_metadata_mountpoint;
    const string pinop_lock_umount_part = FLAGS_metadata_partition;

    unique_ptr<CautiousFileHandlerInterface> version_file_handler(
            new CautiousFileHandler(FLAGS_cluster_version_path, "version"));
    ClusterVersionStore cluster_version_store(move(version_file_handler));

    PrimaryStore primary_store(file_system_store,
            primary_data_store,
            cluster_version_store,
            device_information,
            profiler,
            FLAGS_file_store_minimum_size,
            instant_secure_eraser,
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
    ::pskinny_waist__ = &skinny_waist;

    com::seagate::kinetic::HmacProvider hmac_provider;
    HmacAuthenticator authenticator(user_store, hmac_provider);
    StatisticsManager statistics_manager;

    ::kinetic::KineticConnectionFactory kinetic_connection_factory =
            ::kinetic::NewKineticConnectionFactory();

    P2PRequestManager p2p_request_manager(
        authorizer,
        user_store,
        kinetic_connection_factory,
        skinny_waist,
        FLAGS_p2p_heuristic_memory_max_usage,
        FLAGS_p2p_heuristic_memory_continue_threshold,
        FLAGS_p2p_max_outsanding_requests);

    CHECK_GT(FLAGS_max_message_size_bytes, 0);
    LOG(INFO) << "Rejecting protobuf messages or values > " << FLAGS_max_message_size_bytes//NO_SPELL//NOLINT
            << " bytes";

    com::seagate::kinetic::GetLogHandler get_log_handler(
            primary_data_store,
            device_information,
            network_interfaces,
            FLAGS_listen_port,
            FLAGS_listen_ssl_port,
            limits,
            statistics_manager);

    // Parse message types
    get_log_handler.ParseAndSetMessageTypes(FLAGS_message_types);
    SecurityHandler security_handler;
    SetupHandler setup_handler(
            authorizer,
            skinny_waist,
            cluster_version_store,
            FLAGS_firmware_update_tmp_dir,
            security_handler,
            device_information);

    PinOpHandler pinop_handler(skinny_waist,
                               pinop_lock_umount_point,
                               pinop_lock_umount_part,
                               static_drive_info);

    PowerManager power_manager(skinny_waist, FLAGS_store_partition);

    MessageProcessor message_processor(
            authorizer,
            skinny_waist,
            profiler,
            cluster_version_store,
            FLAGS_firmware_update_tmp_dir,
            FLAGS_max_message_size_bytes,
            p2p_request_manager,
            get_log_handler,
            setup_handler,
            pinop_handler,
            power_manager,
            limits,
            static_drive_info,
            user_store);

    SSL_CTX* ssl_context = initialize_openssl(FLAGS_private_key, FLAGS_certificate);
    ValueFactory value_factory;
    MessageStreamFactory message_stream_factory(ssl_context, value_factory);

    ConnectionHandler connection_handler(authenticator, message_processor,
        message_stream_factory, profiler,
        limits, user_store, static_drive_info.sectors_read_at_poweron, statistics_manager);
    primary_store.SetConnectionHandler(&connection_handler);
    primary_data_store.SetLogHandlerInterface(&connection_handler);
    primary_data_store.SetListOwnerReference(&connection_handler);

    CHECK_GT(FLAGS_heartbeat_interval_seconds, 0U);

    Server server(
            connection_handler,
            FLAGS_listen_ipv4_address,
            FLAGS_listen_port,
            FLAGS_listen_ssl_port,
            FLAGS_listen_ipv6_address,
            FLAGS_socket_receive_buffer,
            signal_pipe_fd,
            ssl_context,
            network_interfaces,
            device_information,
            statistics_manager,
            limits,
            FLAGS_heartbeat_interval_seconds,
            p2p_request_manager,
            kinetic_alarms);

    server.Initialize();
    server.SetKeyValueStore(&primary_data_store);
    server.SetSkinnyWaist(&skinny_waist);
    connection_handler.SetServer(&server);
    setup_handler.SetServer(&server);
    pinop_handler.SetServer(&server);
    pinop_handler.SetConnectionHandler(&connection_handler);
    Server::connection_map.SetConnectionHandler(&connection_handler);

    CommandValidator commandValidator(server, primary_store, authorizer,
            cluster_version_store, limits);
    primary_store.SetCommandValidator(&commandValidator);
    power_manager.SetServer(&server);

    Thread serverThread(&server);
    server.StateChanged(StateEvent::START_ANNOUNCER);
    serverThread.start(false);
    if (!server.IsDown() && launch_monitor.OperationAllowed(LaunchStep::LOCK_STATUS_QUERY)) {
        SecurityManager sed_manager;
        sed_manager.SetLogHandlerInterface(&connection_handler);
        // Check if unlocked
        switch (sed_manager.GetLockStatus()) {
            case BitStatus::LOCKED:
                launch_monitor.OperationCompleted();
                server.StateChanged(com::seagate::kinetic::StateEvent::LOCKED);
                break;
            case BitStatus::UNLOCKED: {
                if (!Server::_shuttingDown && !server.IsDown()) {
                    server.StateChanged(com::seagate::kinetic::StateEvent::UNLOCKED);
                    leveldb::Status::createErrorLog();
                    switch (skinny_waist.InitUserDataStore()) {
                        case UserDataStatus::SUCCESSFUL_LOAD:
                            server.StateChanged(com::seagate::kinetic::StateEvent::RESTORED);
                            break;
                        case UserDataStatus::STORE_CORRUPT:
                            server.StateChanged(com::seagate::kinetic::StateEvent::STORE_CORRUPT);
                            break;
                        default:
                            server.StateChanged(com::seagate::kinetic::StateEvent::STORE_INACCESSIBLE);
                            break;
                    }
                }
                break;
            }
            default:
                LOG(ERROR) << "Drive is unable to query state";
                connection_handler.LogLatency(LATENCY_EVENT_LOG_UPDATE);
                LogRingBuffer::Instance()->makePersistent();
                break;
        }
    } else {
        // Lock query resulted in prior hangs
        // Set status to inaccessible - ISE only since we can't recover ACLs
        if (Server::_shuttingDown || !server.IsCommandReady()) {
            launch_monitor.WriteState(LaunchStep::LOCK_STATUS_QUERY, true);
        } else {
            launch_monitor.WriteState(LaunchStep::LOCK_STATUS_QUERY, false);
        }
        server.StateChanged(com::seagate::kinetic::StateEvent::STORE_INACCESSIBLE);
    }

    cout << " SERVER STARTED" << endl;
    // Wait until server is stopped
    initDone = 1;
    pthread_join(serverThread.getThreadId(), NULL);
    cout << " THREAD JOINED" << endl;
    LOG(INFO) << "Server stopped.  Exiting Kinetic...";
    LOG(INFO) << "===== EXIT STATE:" << endl;
    LOG(INFO) << "#Batch sets = " << ConnectionHandler::_batchSetCollection.numberOfBatchsets()
              << ", #connections = " << Server::connection_map.TotalConnectionCount();
    LOG(INFO) << "#Ingested requests = " << connection_handler.currentState();
    LOG(INFO) << "Dynamic memory usage = " << DynamicMemory::getInstance()->usage();
    LOG(INFO) << "# Active connections = " << Server::connection_map.TotalConnectionCount(true);
    LOG(INFO) << "# All connections = " << Server::connection_map.TotalConnectionCount();

    LOG(INFO) << endl;

    delete LogRingBuffer::Instance();
    if (FLAGS_display_profiling) {
        profiler.LogResults();
    }
    free_openssl(ssl_context);
    google::protobuf::ShutdownProtobufLibrary();
    google::ShutdownGoogleLogging();
    delete LogRingBuffer::Instance();
    google::ShutDownCommandLineFlags();
    cout << "END OF KINETICD" << endl;
    return(0);
}

