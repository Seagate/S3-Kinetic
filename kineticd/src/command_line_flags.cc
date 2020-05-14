#include "command_line_flags.h"
#include "gflags/gflags.h"

DEFINE_string(kineticd_start_log, "/dev/shm/kineticd_start", "Kineticd start log");
DEFINE_string(primary_db_path, "primary.db", "The primary database path");
DEFINE_string(util_path, "/mnt/util", "Util path");
DEFINE_string(metadata_db_path, "/mnt/metadata/metadata.db", "The metadata database path");
DEFINE_string(file_store_path, "file_store", "The blob storage directory");
DEFINE_string(proc_stat_path, "/proc/stat", "Path to process status");
DEFINE_string(sysfs_temperature_dir, "/sys/class/hwmon/hwmon0/", "Directory to file system temperature");

// do not set file_store_minimum_size above 500KB without changing memory allocation in block builder appropriately
DEFINE_int32(file_store_minimum_size, 8 * 1024, "The minimum size for which values are stored outside the sst");
DEFINE_int32(block_size, 256 * 1024, "SST block size");
DEFINE_int32(sst_size, 1 * 1024 * 1024, "Level 0 SST size");
DEFINE_int32(write_buffer_size, 64 * 1024 * 1024, "Maximum write buffer size");
DEFINE_int32(max_batch_size, 64 * 1024 * 1024, "Maximum batch size");
DEFINE_int32(max_deletes_per_batch, 24000, " Maximum deletes per batch");
DEFINE_string(listen_ipv4_address, "0.0.0.0", "The ipv4 address to listen on");
DEFINE_uint64(listen_port, 8123, "The ipv4 port to listen on");
DEFINE_uint64(listen_ssl_port, 8443, "The ipv4 port to listen on for SSL connections");
DEFINE_string(listen_ipv6_address, "::", "The ipv6 address to listen on");
DEFINE_string(private_key, "private_key.pem", "The SSL private key");
DEFINE_string(certificate, "certificate.pem", "The SSL certificate");
DEFINE_string(vendor_option_path, "/tmp", "The path to the DHCP vendor option files");
DEFINE_int32(socket_receive_buffer, 0,
    "The size of the kernel's socket receive buffer (or 0 for the default), non-SSL only");
DEFINE_int32(socket_timeout, 10000, "Socket timeout in milli second");
DEFINE_bool(display_profiling, false, "Display profiling counters when program exists");
DEFINE_string(cluster_version_path, "cluster_version", "The file containing the cluster version");
DEFINE_string(firmware_update_tmp_dir, "tmp/firmware_update",
    "Directory used to temporarily store firmware update files");
DEFINE_int32(max_message_size_bytes, 1024 * 1024,
    "The maximum size in bytes of a value or protobuf message");
DEFINE_int32(table_cache_size, 10 * 1048576, "SST Index + BF Cache size");
DEFINE_string(metadata_mountpoint, "/mnt/metadata", "Metadata Partition Mount Point");
DEFINE_string(metadata_partition, "/dev/sda5", "Device Partition where Metadata Components reside");
DEFINE_string(store_mountpoint, "/mnt/store", "Location where data partition is mounted");
DEFINE_string(store_partition, "/dev/sda", "Device partition containing data");
DEFINE_string(store_test_partition, "/dev/sde2", "Device partition containing data");
DEFINE_string(store_device, "/dev/sda", "Device containing data partition");
DEFINE_string(store_test_device, "/dev/sde", "Device used for host aware command set");

DEFINE_string(preused_file_path, "fsize",
    "File used to store the amount of preused space");
DEFINE_string(status_codes, "5,11,13,16",
    "List of status codes that will trigger log to write to disk");
DEFINE_string(message_types, "2,4,6,8,10",
    "List of message types whose failure rate will be reported");
DEFINE_string(log_file_path, "log.txt", "File path where log will be written to");
DEFINE_string(old_log_file_path, "previousLog",
    "File path where previous log will be written to");
DEFINE_string(stale_data_log_file_path, "stale_log.txt",
    "File path where the level at which stale data is removed will be written to");
DEFINE_string(old_stale_data_log_file_path, "stale_previouslog",
    "File path where previous stale data log will be written to");
DEFINE_string(command_log_file_path, "command_log.txt",
    "File path where command log will be written to");
DEFINE_string(old_command_log_file_path, "command_previousLog",
    "File path where previous command log will be written to");
DEFINE_string(key_value_log_file_path, "key_value_log.txt",
    "File path where key value frequency log will be written to");
DEFINE_string(old_key_value_log_file_path, "key_value_previousLog",
    "File path where the previous key value frequency log will be written to");
DEFINE_string(key_size_log_file_path, "key_size_log.txt",
    "File path where key size frequency log will be written to");
DEFINE_string(old_key_size_log_file_path, "key_size_previousLog",
    "File path where the previous key size frequency log will be written to");
DEFINE_string(value_size_log_file_path, "value_size_log.txt",
    "File path where value size frequency log will be written to");
DEFINE_string(old_value_size_log_file_path, "value_size_previousLog",
    "File path where the previous value size frequency log will be written to");

DEFINE_int32(latency_threshold, 5000, "The threshold for when latency for a command is logged");

DEFINE_uint64(heartbeat_interval_seconds, 5, "Heartbeat interval in seconds");

DEFINE_uint64(p2p_heuristic_memory_max_usage, 10*1024*1024,
    "Cap on memory used by P2P Operations. Measured by a heuristic, not precise.");

DEFINE_uint64(p2p_heuristic_memory_continue_threshold, 5*1024*1024,
    "Threshold for continuing P2P Operation after working to reduce memory usage");

DEFINE_uint64(p2p_max_outsanding_requests, 10,
    "Maximum number of outstanding p2p requests allowed.");
