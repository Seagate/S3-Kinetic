/*
 * CommandValidator.cc
 *
 *  Created on: Apr 19, 2016
 *      Author: tri
 */

#include "CommandValidator.h"

namespace com {
namespace seagate {
namespace kinetic {

bool CommandValidator::isValidVersion(const Command& command, Command& command_response) {
    Command_Status_StatusCode cmdStatus = Command_Status_StatusCode_SUCCESS;
    proto::Command_KeyValue const& keyvalue = command.body().keyvalue();
    PrimaryStoreValue existing_primary_store_value;
    StoreOperationStatus opStatus = primaryStore_.Get(keyvalue.key(),
            &existing_primary_store_value, NULL);

    switch (opStatus) {
    case StoreOperationStatus_SUCCESS:
        if (existing_primary_store_value.version != keyvalue.dbversion()) {
            cmdStatus = Command_Status_StatusCode_VERSION_MISMATCH;
            command_response.mutable_status()->set_statusmessage("Version mismatch");
        }
        break;
    case StoreOperationStatus_NOT_FOUND:
        if (command.header().messagetype() == Command_MessageType_PUT) {
            if (keyvalue.dbversion().length()) {
                cmdStatus = Command_Status_StatusCode_VERSION_MISMATCH;
                command_response.mutable_status()->set_statusmessage("Version mismatch");
            }
        } else {
            cmdStatus = Command_Status_StatusCode_NOT_FOUND;
            command_response.mutable_status()->set_statusmessage("Key not found");
        }
        break;
    case StoreOperationStatus_STORE_CORRUPT:
        cmdStatus = Command_Status_StatusCode_INTERNAL_ERROR;
        command_response.mutable_status()->set_statusmessage("Corrupted database");
        break;
    default:
        cmdStatus = Command_Status_StatusCode_INTERNAL_ERROR;
        command_response.mutable_status()->set_statusmessage("Internal error");
        break;
    }
    command_response.mutable_status()->set_code(cmdStatus);
    return (cmdStatus == Command_Status_StatusCode_SUCCESS);
}
bool CommandValidator::doesKeyExist(const Command& command, Command& command_response) {
    Command_Status_StatusCode cmdStatus = Command_Status_StatusCode_SUCCESS;
    proto::Command_KeyValue const& keyvalue = command.body().keyvalue();
    StoreOperationStatus opStatus = primaryStore_.DoesKeyExist(keyvalue.key());

    switch (opStatus) {
        case StoreOperationStatus_SUCCESS:
            break;
        case StoreOperationStatus_NOT_FOUND:
            cmdStatus = Command_Status_StatusCode_NOT_FOUND;
            command_response.mutable_status()->set_statusmessage("Key not found");
            break;
        default:
            cmdStatus = Command_Status_StatusCode_INTERNAL_ERROR;
            command_response.mutable_status()->set_statusmessage("Internal error");
            break;
    }
    command_response.mutable_status()->set_code(cmdStatus);
    return (cmdStatus == Command_Status_StatusCode_SUCCESS);
}

bool CommandValidator::isValidKey(const Command& command, Command& command_response) {
    Command_Status_StatusCode status = Command_Status_StatusCode_INVALID_REQUEST;
    string msg;
    proto::Command_KeyValue const& keyvalue = command.body().keyvalue();

    if (!keyvalue.has_tag()) {
        msg = "Tag required";
    } else if (keyvalue.newversion().size() > limits_.max_value_size()) {
        msg = "Version too long";
    } else if (keyvalue.key().size() > limits_.max_key_size()) {
        msg = "Key too long";
    } else if (keyvalue.tag().size() > limits_.max_tag_size()) {
        msg = "Tag too long";
    } else {
        status = Command_Status_StatusCode_SUCCESS;
    }
    command_response.mutable_status()->set_code(status);
    command_response.mutable_status()->set_statusmessage(msg);
    return (status == Command_Status_StatusCode_SUCCESS);
}
Command_Status_StatusCode CommandValidator::toCommandStatus(StoreOperationStatus opStatus) {
    Command_Status_StatusCode cmdStatus = Command_Status_StatusCode_SUCCESS;
    switch (opStatus) {
    case StoreOperationStatus_SUCCESS:
        cmdStatus = Command_Status_StatusCode_SUCCESS;
        break;
    case StoreOperationStatus::StoreOperationStatus_NOT_FOUND:
        cmdStatus = Command_Status_StatusCode_NOT_FOUND;
        break;
    case StoreOperationStatus::StoreOperationStatus_NO_SPACE:
        cmdStatus = Command_Status_StatusCode_NO_SPACE;
        break;
    case StoreOperationStatus::StoreOperationStatus_VERSION_MISMATCH:
        cmdStatus = Command_Status_StatusCode_VERSION_MISMATCH;
        break;
    case StoreOperationStatus::StoreOperationStatus_INVALID_REQUEST:
        cmdStatus = Command_Status_StatusCode_INVALID_REQUEST;
        break;
    case StoreOperationStatus::StoreOperationStatus_VERSION_FAILURE:
        cmdStatus = Command_Status_StatusCode_VERSION_FAILURE;
        break;
    case StoreOperationStatus_STORE_CORRUPT:
    case StoreOperationStatus_DATA_CORRUPT:
        cmdStatus = Command_Status_StatusCode_DATA_ERROR;
        break;
    case StoreOperationStatus_INTERNAL_ERROR:
    default: // All other store operation statuses have no corresponding command status
        cmdStatus = Command_Status_StatusCode_INTERNAL_ERROR;
        break;
    }
    return cmdStatus;
}

} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */
