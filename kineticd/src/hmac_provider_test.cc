#include "gtest/gtest.h"
#include <string.h>

#include "kinetic.pb.h"
#include "hmac_provider.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Message;
using proto::Command;
using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;

TEST(HmacProviderTest, ComputeHmacHandlesSimpleMessage) {
    HmacProvider hmac_provider;
    unsigned char expected_hmac_bytes[] = { 0x40, 0x5F, 0x94, 0x9F, 0xC3, 0x50,
        0xDC, 0x0B, 0x6A, 0x5A, 0x9D, 0x27, 0xA3, 0xCA, 0x44, 0x58, 0x9D, 0xB3,
        0x4A, 0xCD };
    std::string expected_hmac((char *)expected_hmac_bytes, sizeof(expected_hmac_bytes));

    Message response_message;
    Command command_response;
    std::string serialized_command;
    command_response.mutable_status()->
        set_code(Command_Status_StatusCode_SUCCESS);
    command_response.SerializeToString(&serialized_command);
    response_message.set_commandbytes(serialized_command);

    std::string actual_hmac = hmac_provider.ComputeHmac(response_message, "asdfasdf");

    EXPECT_EQ(expected_hmac, actual_hmac);
}

TEST(HmacProviderTest, ComputeHmacOfEmptyMessage) {
    HmacProvider hmac_provider;
    unsigned char expected_hmac_bytes[] = {0xa7, 0x7a, 0x6a, 0xda, 0x5c, 0xe6,
        0x7c, 0xf7, 0xae, 0xe4, 0x8a, 0x79, 0xd4, 0x86, 0x6b, 0xb2, 0x71, 0x24,
        0x18, 0x15 };
    std::string expected_hmac((char *)expected_hmac_bytes, sizeof(expected_hmac_bytes));

    Message response_message;

    std::string actual_hmac =
        hmac_provider.ComputeHmac(response_message, "asdfasdf");

    EXPECT_EQ(expected_hmac, actual_hmac);
}

TEST(HmacProviderTest, ComputeHmacOfFullMessage) {
    HmacProvider hmac_provider;
    unsigned char expected_hmac_bytes[] = { 0xcb, 0x65, 0x6d, 0x75, 0x05, 0xfc,
        0xc8, 0xd7, 0xd3, 0x00, 0xc4, 0xe3, 0x13, 0xd7, 0x54, 0x6b, 0xd2, 0x37,
        0x26, 0x13 };
    std::string expected_hmac((char *)expected_hmac_bytes, sizeof(expected_hmac_bytes));

    Message response_message;
    Command command_response;
    std::string serialized_command;
    response_message.mutable_hmacauth()->set_identity(1234);
    command_response.mutable_body()->mutable_keyvalue()->set_key("the key");
    command_response.mutable_status()->
        set_code(Command_Status_StatusCode_SUCCESS);
    command_response.SerializeToString(&serialized_command);
    response_message.set_commandbytes(serialized_command);

    std::string actual_hmac =
        hmac_provider.ComputeHmac(response_message, "asdfasdf");

    EXPECT_EQ(expected_hmac, actual_hmac);
}

// Test that we cannot change any part of the message without changing the HMAC
TEST(HmacProviderTest, ComputeHmacIncludesAllFields) {
    HmacProvider hmac_provider;

    Message message;
    Command command;
    std::string serialized_command;
    message.mutable_hmacauth()->set_identity(1);
    command.mutable_body()->mutable_keyvalue()->set_key("key");
    command.mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);

    std::string original_hmac = hmac_provider.ComputeHmac(message, "asdfasdf");
    std::string hmac;

    // Identical copy should have the same HMAC
    Message identical_copy(message);
    hmac = hmac_provider.ComputeHmac(identical_copy, "asdfasdf");
    EXPECT_EQ(original_hmac, hmac);

    // Change header
    Message message_with_different_header(message);
    Command modified_command_header(command);
    modified_command_header.mutable_header()->set_acksequence(33);
    modified_command_header.SerializeToString(&serialized_command);
    message_with_different_header.set_commandbytes(serialized_command);
    hmac = hmac_provider.ComputeHmac(message_with_different_header, "asdfadsf");
    EXPECT_NE(original_hmac, hmac);

    // Change body
    Message message_with_different_body(message);
    Command modified_command_body(command);
    modified_command_body.mutable_body()->mutable_keyvalue()->
        set_key("different key");
    modified_command_body.SerializeToString(&serialized_command);
    message_with_different_body.set_commandbytes(serialized_command);
    hmac = hmac_provider.ComputeHmac(message_with_different_body, "asdfasdf");
    EXPECT_NE(original_hmac, hmac);

    // Change status
    Message message_with_different_status(message);
    Command command_with_different_status(command);
    command_with_different_status.mutable_status()->
        set_code(Command_Status_StatusCode_INTERNAL_ERROR);
    command_with_different_status.SerializeToString(&serialized_command);
    message_with_different_status.set_commandbytes(serialized_command);
    hmac = hmac_provider.ComputeHmac(message_with_different_status, "asdfasdf");
    EXPECT_NE(original_hmac, hmac);
}

TEST(HmacProviderTest, ValidateHmacReturnsFalseOnInvalidHmac) {
    HmacProvider hmac_provider;
    unsigned char hmac_bytes[] = { 0xff, 0x5F, 0x94, 0x9F, 0xC3, 0x50,
        0xDC, 0x0B, 0x6A, 0x5A, 0x9D, 0x27, 0xA3, 0xCA, 0x44, 0x58, 0x9D, 0xB3,
        0x4A, 0xCD };
    std::string hmac((char *) hmac_bytes, sizeof(hmac_bytes));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    message.mutable_hmacauth()->set_hmac(hmac);

    EXPECT_FALSE(hmac_provider.ValidateHmac(message, "asdfasdf"));
}

TEST(HmacProviderTest, ValidateHmacReturnsFalseOnWrongLengthHmac) {
    HmacProvider hmac_provider;
    unsigned char hmac_bytes[] = { 0x40, 0x5F, 0x94, 0x9F, 0xC3, 0x50,
        0xDC, 0x0B, 0x6A, 0x5A, 0x9D, 0x27, 0xA3, 0xCA, 0x44, 0x58, 0x9D, 0xB3,
        0x4A };
    std::string hmac((char *) hmac_bytes, sizeof(hmac_bytes));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    message.mutable_hmacauth()->set_hmac(hmac);

    EXPECT_FALSE(hmac_provider.ValidateHmac(message, "asdfasdf"));
}

TEST(HmacProviderTest, ValidateHmacReturnsTrueOnValidHmac) {
    HmacProvider hmac_provider;
    unsigned char hmac_bytes[] = { 0x40, 0x5F, 0x94, 0x9F, 0xC3, 0x50,
        0xDC, 0x0B, 0x6A, 0x5A, 0x9D, 0x27, 0xA3, 0xCA, 0x44, 0x58, 0x9D, 0xB3,
        0x4A, 0xCD };
    std::string hmac((char *) hmac_bytes, sizeof(hmac_bytes));

    Message message;
    Command command;
    std::string serialized_command;
    command.mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
    command.SerializeToString(&serialized_command);
    message.set_commandbytes(serialized_command);
    message.mutable_hmacauth()->set_hmac(hmac);

    EXPECT_TRUE(hmac_provider.ValidateHmac(message, "asdfasdf"));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
