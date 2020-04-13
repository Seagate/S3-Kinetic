#include <stdlib.h>
#include "gtest/gtest.h"

#include "cluster_version_store.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::unique_ptr;
using std::move;

class ClusterVersionStoreTest : public ::testing::Test {
    protected:
    ClusterVersionStoreTest():
    cluster_version_store_(move(unique_ptr<CautiousFileHandlerInterface>(
            new CautiousFileHandler("test_cluster_version", "version"))))
    {}

    virtual void SetUp() {
        ASSERT_NE(-1, system("rm -rf test_cluster_version"));
        ASSERT_NE(-1, system("mkdir test_cluster_version"));
        ASSERT_TRUE(cluster_version_store_.Init());
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_cluster_version"));
    }

    ClusterVersionStore cluster_version_store_;
};

TEST_F(ClusterVersionStoreTest, SetVersionSetsVersion) {
    ASSERT_TRUE(cluster_version_store_.SetClusterVersion(1234));

    EXPECT_EQ(1234, cluster_version_store_.GetClusterVersion());
}

TEST_F(ClusterVersionStoreTest, PersistsVersion) {
    ASSERT_TRUE(cluster_version_store_.SetClusterVersion(1234));

    // it's just a test so there's not actually any concurrent writes to the file handler
    ClusterVersionStore store2(move(unique_ptr<CautiousFileHandlerInterface>(
            new CautiousFileHandler("test_cluster_version", "version"))));
    ASSERT_TRUE(store2.Init());
    ASSERT_EQ(1234, store2.GetClusterVersion());
}

TEST_F(ClusterVersionStoreTest, FailsOnUnparseableFile) {
    ASSERT_NE(-1, system("echo -n foo > test_cluster_version/version"));

    ClusterVersionStore store2(move(unique_ptr<CautiousFileHandlerInterface>(
            new CautiousFileHandler("test_cluster_version", "version"))));
    ASSERT_FALSE(store2.Init());
}

TEST_F(ClusterVersionStoreTest, FailsOnEmptyFile) {
    ASSERT_NE(-1, system("echo -n '' > test_cluster_version/version"));

    ClusterVersionStore store2(move(unique_ptr<CautiousFileHandlerInterface>(
            new CautiousFileHandler("test_cluster_version", "version"))));
    ASSERT_FALSE(store2.Init());
}

TEST_F(ClusterVersionStoreTest, MissingFileDefaultsToZero) {
    ASSERT_TRUE(cluster_version_store_.Init());

    EXPECT_EQ(0, cluster_version_store_.GetClusterVersion());
}

} // namespace kinetic
} // namespace seagate
} // namespace com
