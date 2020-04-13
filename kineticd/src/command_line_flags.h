#ifndef KINETIC_COMMAND_LINE_FLAGS_H_
#define KINETIC_COMMAND_LINE_FLAGS_H_

#include "gflags/gflags.h"

DECLARE_string(kineticd_start_log);
DECLARE_string(primary_db_path);
DECLARE_string(util_path);
DECLARE_string(metadata_db_path);
DECLARE_string(file_store_path);
DECLARE_int32(file_store_minimum_size);
DECLARE_int32(block_size);
DECLARE_int32(sst_size);
DECLARE_int32(write_buffer_size);
DECLARE_string(listen_ipv4_address);
DECLARE_uint64(listen_port);
DECLARE_uint64(listen_ssl_port);
DECLARE_string(listen_ipv6_address);
DECLARE_string(private_key);
DECLARE_string(certificate);
DECLARE_string(vendor_option_path);
DECLARE_int32(socket_receive_buffer);
DECLARE_int32(socket_timeout);
DECLARE_bool(display_profiling);
DECLARE_string(cluster_version_path);
DECLARE_string(firmware_update_tmp_dir);
DECLARE_int32(max_message_size_bytes);
DECLARE_int32(max_batch_size);
DECLARE_int32(max_deletes_per_batch);

DECLARE_int32(table_cache_size);

DECLARE_string(metadata_mountpoint);
DECLARE_string(metadata_partition);
DECLARE_string(store_mountpoint);
DECLARE_string(store_partition);
DECLARE_string(store_test_partition);
DECLARE_string(store_device);
DECLARE_string(store_test_device);

DECLARE_string(preused_file_path);
DECLARE_string(status_codes);
DECLARE_string(message_types);
DECLARE_string(log_file_path);
DECLARE_string(old_log_file_path);
DECLARE_string(stale_data_log_file_path);
DECLARE_string(old_stale_data_log_file_path);
DECLARE_string(command_log_file_path);
DECLARE_string(old_command_log_file_path);
DECLARE_string(key_value_log_file_path);
DECLARE_string(old_key_value_log_file_path);
DECLARE_string(key_size_log_file_path);
DECLARE_string(old_key_size_log_file_path);
DECLARE_string(value_size_log_file_path);
DECLARE_string(old_value_size_log_file_path);

DECLARE_int32(latency_threshold);

DECLARE_uint64(heartbeat_interval_seconds);

DECLARE_uint64(p2p_heuristic_memory_max_usage);

DECLARE_uint64(p2p_heuristic_memory_continue_threshold);

DECLARE_uint64(p2p_max_outsanding_requests);

#endif  // KINETIC_COMMAND_LINE_FLAGS_H_
