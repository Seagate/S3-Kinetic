#include <stdlib.h>
#include "glog/logging.h"

#include "cluster_version_store.h"
#include "pthreads_mutex_guard.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::move;
using std::unique_ptr;
using std::string;

ClusterVersionStore::ClusterVersionStore(
        unique_ptr<CautiousFileHandlerInterface> file_handler)
        : cluster_version_(kDefaultStoreVersion),
        file_handler_(move(file_handler)),
        successfully_initialized_(false) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&mutex_, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
}

ClusterVersionStore::~ClusterVersionStore() {
    pthread_mutex_destroy(&mutex_);
}


bool ClusterVersionStore::Init() {
    PthreadsMutexGuard guard(&mutex_);

    string data;
    FileReadResult res = file_handler_->Read(data);

    if (res == FileReadResult::CANT_OPEN_FILE) {
        LOG(INFO) << "Defaulting to cluster version " << cluster_version_;

        successfully_initialized_ = true;
    } else if (res == FileReadResult::OK) {
        char* end;
        int64_t parse_res = strtol(data.c_str(), &end, 10);
        // strtol sets end to point to the start of the input on empty input or parse failure
        if (end == data.c_str()) {
            PLOG(WARNING) << "Unable to parse cluster version; result was " << parse_res;
        } else {
            cluster_version_ = parse_res;
            successfully_initialized_ = true;
            VLOG(1) << "Loaded " << cluster_version_;
        }
    } else {
        LOG(WARNING) << "Could not load cluster version";
    }

    return successfully_initialized_;
}

int64_t ClusterVersionStore::GetClusterVersion() {
    CHECK(successfully_initialized_);
    PthreadsMutexGuard guard(&mutex_);

    return cluster_version_;
}

bool ClusterVersionStore::SetClusterVersion(int64_t new_cluster_version) {
    CHECK(successfully_initialized_);
    VLOG(1) << "Setting cluster version to " << new_cluster_version;
    PthreadsMutexGuard guard(&mutex_);

    string data = std::to_string(new_cluster_version);

    if (file_handler_->Write(data)) {
        VLOG(1) << "Saved cluster version as " << data;
        cluster_version_ = new_cluster_version;
        return true;
    } else {
        LOG(ERROR) << "Could not save cluster version " << data;
        return false;
    }
}

bool ClusterVersionStore::Erase() {
    // Initialize state of cluster version store
    successfully_initialized_ = true;
    // Erasing ClusterVersionStore means setting cluster version back to default
    return SetClusterVersion(kDefaultStoreVersion);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
