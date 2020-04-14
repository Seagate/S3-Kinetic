#include <iostream>
#include <sstream>
#include <cstdio>
#include <chrono>

#include "gflags/gflags.h"

#include "glog/logging.h"

#include "leveldb/db.h"
#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"

#include "db_logger.h"
#include "stack_trace.h"

bool kineticd_idle = true;

DEFINE_string(storage_path, "manifest-stress-storage", "leveldb storage location");
DEFINE_uint64(num_keys, 100, "number of keys");
DEFINE_uint64(key_size, 4096, "Key size in bytes");
DEFINE_uint64(value_size, 10, "Value size in bytes");
DEFINE_uint64(num_iterations, 1, "Iterations over keyspace");
DEFINE_string(mode, "write", "mode: write = open and write to db, open = only open the db");

using std::string;
using std::stringstream;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

using com::seagate::kinetic::DbLogger;

int writeDb();
int openDb();

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    google::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_mode == "write") {
        return writeDb();
    } else if (FLAGS_mode == "open") {
        return openDb();
    } else {
        LOG(ERROR) << "Invalid mode: " << FLAGS_mode;
    }

    return 1;
}


int writeDb() {
    leveldb::Options options;
    options.create_if_missing = true;
    DbLogger logger;
    options.info_log = &logger;
    options.compression = leveldb::kNoCompression;

    leveldb::DB* db;

    leveldb::Status status = leveldb::DB::Open(options, FLAGS_storage_path, &db);

    if (!status.ok()) {
        LOG(ERROR) << "Failed to open LevelDB database: " << status.ToString();//NO_SPELL
        return 1;
    }

    char key[FLAGS_key_size + 1];
    string value('x', FLAGS_value_size);

    stringstream format_stream;
    format_stream << "%0";
    format_stream << FLAGS_key_size;
    format_stream << "u";
    string key_format = format_stream.str();

    leveldb::WriteOptions write_options;

    for (uint64_t iter = 0; iter < FLAGS_num_iterations; iter++) {
        for (uint64_t i = 0; i < FLAGS_num_keys; ++i) {
            int ret = sprintf(key, key_format.c_str(), i);

            if (ret != (int) FLAGS_key_size) {
                LOG(ERROR) << "sprintf returned " << ret;//NO_SPELL
                delete db;
                return 1;
            }

            status = db->Put(write_options, key, value);

            if (!status.ok()) {
                LOG(ERROR) << "Failed to put: " << status.ToString();
                delete db;
                return 1;
            }
        }
    }

    delete db;
    return 0;
}

int openDb() {
    leveldb::Options options;
    options.create_if_missing = true;
    DbLogger logger;
    options.info_log = &logger;
    options.compression = leveldb::kNoCompression;

    leveldb::DB* db;

    auto begin = high_resolution_clock::now();
    leveldb::Status status = leveldb::DB::Open(options, FLAGS_storage_path, &db);

    if (!status.ok()) {
        LOG(ERROR) << "Failed to open LevelDB database: " << status.ToString();//NO_SPELL
        delete db;
        return 1;
    }

    auto end = high_resolution_clock::now();
    auto dur = end - begin;
    auto ms = duration_cast<milliseconds>(dur).count();

    LOG(INFO) << "Opening took " << ms << "ms";

    delete db;
    return 0;
}
