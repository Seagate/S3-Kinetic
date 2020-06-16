#include "gtest/gtest.h"
#include "glog/logging.h"

#include "kinetic.pb.h"
#include "key_value_store_interface.h"
#include "key_value_store.h"
#include "command_line_flags.h"

using com::seagate::kinetic::proto::Message;
using com::seagate::kinetic::KeyValueStore;
using com::seagate::kinetic::StoreOperationStatus;

TEST(SerializationTest, SerializeAndDeserialize) {
    Message message;
    message.mutable_hmacauth()->set_hmac("abc123");

    std::string serialized;
    ASSERT_TRUE(message.SerializeToString(&serialized));

    Message deserialized;
    ASSERT_TRUE(deserialized.ParseFromString(serialized));
    ASSERT_TRUE(deserialized.hmacauth().has_hmac());
    EXPECT_EQ("abc123", deserialized.hmacauth().hmac());
}

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    testing::InitGoogleTest(&argc, argv);
    google::ParseCommandLineFlags(&argc, &argv, true);
    FLAGS_store_test_device = "";
    for (char c : FLAGS_store_test_partition) {
        if (!std::isdigit(c)) FLAGS_store_test_device += c;
    }
    int status = RUN_ALL_TESTS();
    google::ShutdownGoogleLogging();
    google::ShutDownCommandLineFlags();
    google::protobuf::ShutdownProtobufLibrary();
    return status;
}
