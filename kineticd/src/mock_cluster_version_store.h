#ifndef KINETIC_MOCK_CLUSTER_VERSION_STORE_H_
#define KINETIC_MOCK_CLUSTER_VERSION_STORE_H_

#include "cluster_version_store.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::unique_ptr;

class MockClusterVersionStore : public ClusterVersionStoreInterface {
    public:
    MockClusterVersionStore() {}
    MOCK_METHOD0(Init, bool());
    MOCK_METHOD0(Erase, bool());
    MOCK_METHOD0(GetClusterVersion, int64_t());
    MOCK_METHOD1(SetClusterVersion, bool(int64_t new_cluster_version));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_CLUSTER_VERSION_STORE_H_
