#ifndef KINETIC_CLUSTER_VERSION_STORE_H_
#define KINETIC_CLUSTER_VERSION_STORE_H_

#include <inttypes.h>
#include <memory>

#include "gmock/gmock.h"
#include "kinetic/common.h"
#include "cautious_file_handler.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::unique_ptr;

class ClusterVersionStoreInterface {
    public:
    virtual ~ClusterVersionStoreInterface() {}
    virtual bool Init() = 0;
    virtual bool Erase() = 0;
    virtual int64_t GetClusterVersion() = 0;
    virtual bool SetClusterVersion(int64_t new_cluster_version) = 0;
};

/**
* Threadsafe class for storing and retrieving a cluster version on disk
*/
class ClusterVersionStore : public ClusterVersionStoreInterface {
    public:
    static const int kDefaultStoreVersion = 0;
    explicit ClusterVersionStore(
            unique_ptr<CautiousFileHandlerInterface> file_handler);
    virtual ~ClusterVersionStore();
    virtual bool Init();
    virtual bool Erase();
    virtual int64_t GetClusterVersion();
    virtual bool SetClusterVersion(int64_t new_cluster_version);

    private:
    int64_t cluster_version_;
    unique_ptr<CautiousFileHandlerInterface> file_handler_;
    pthread_mutex_t mutex_;
    bool successfully_initialized_;

    DISALLOW_COPY_AND_ASSIGN(ClusterVersionStore);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_CLUSTER_VERSION_STORE_H_
