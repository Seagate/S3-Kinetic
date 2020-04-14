/*
 * BatchSet.cc
 *
 *  Created on: Mar 30, 2016
 *      Author: tri
 */

#include "BatchSet.h"
#include "leveldb/write_batch.h"
#include "kinetic.pb.h"
#include "BatchSetCollection.h"
#include <list>

using namespace com::seagate::kinetic::proto; // NOLINT

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {

ostream& operator<<(ostream& os, const BatchSet& batchSet) {
    os << "=== Batch Id = " << batchSet.id_ << ", conn Id = " << batchSet.connId_ << endl;
    os << "    Batched commands:"  << endl;
    std::list<BatchCmd*>::const_iterator it = batchSet.batchCmds_.begin();
    for (; it != batchSet.batchCmds_.end(); ++it) {
        os << *it << endl;
    }
    os << "=== End of batch set" << endl;
    return os;
}

void BatchSet::createWriteBatch(WriteBatch& writeBatch, Command& commandResponse) {
    commandResponse.mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);

    for (std::list<BatchCmd*>::iterator it = batchCmds_.begin(); it != batchCmds_.end(); ++it) {
       BatchCmd* cmd = *it;
       if (cmd->isPut()) {
           LevelDBData* internalValue;
           if (cmd->toInternalValue(internalValue, commandResponse)) {
               writeBatch.Put(cmd->getKey(), (char*)internalValue);
           } else {
               commandResponse.mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
               break;
           }
       } else if (cmd->isDelete()) {
           writeBatch.Delete(cmd->getKey());
       } else {
           // This case never happens because we have checked before adding
           // batched command to batch set
       }
    }
}

bool BatchSet::isValidVersion(CommandValidator& validator, int cmdIdx, Command& commandResponse) {
    return true;
/* This implementation needs to reimplement because data type of bachCmds is changed from
 * vector to map/unordered map.  Currently this method is not in use.
    BatchCmd* batchCmd = batchCmds_[cmdIdx];
    Command* cmd = batchCmd->getCommand();
    proto::Command_KeyValue const& keyvalue = cmd->body().keyvalue();

    for (int i = cmdIdx - 1; i >= 0; --i) {
        BatchCmd* prevBatchCmd = batchCmds_[i];
        Command* prevCmd = prevBatchCmd->getCommand();
        proto::Command_KeyValue const& prevKeyvalue = prevCmd->body().keyvalue();
        if (prevKeyvalue.key() == keyvalue.key()) {
            if (prevBatchCmd->isPut()) {
                if (prevKeyvalue.dbversion() != keyvalue.dbversion()) {
                    commandResponse.mutable_status()->set_code(Command_Status_StatusCode_VERSION_MISMATCH); // NOLINT
                    commandResponse.mutable_status()->set_statusmessage("Version mismatch");
                }
            }
            break;
        }
    }

    if (commandResponse.status().code() == Command_Status_StatusCode_SUCCESS) {
        validator.isValidVersion(*cmd, commandResponse);
    }
    return (commandResponse.status().code() == Command_Status_StatusCode_SUCCESS)
  */
}
bool BatchSet::isValid(CommandValidator& validator, Command& response, int64_t user_id, RequestContext& request_context) {
    Command_Status_StatusCode status = Command_Status_StatusCode_SUCCESS;
    std::list<BatchCmd*>::iterator it = batchCmds_.begin();
    for (; status == Command_Status_StatusCode_SUCCESS && it != batchCmds_.end(); ++it) {
        BatchCmd* batchCmd = *it;
        Command* cmd = batchCmd->getCommand();
        Command* cmdResponse = batchCmd->getResponse();
        if (cmdResponse->status().code() != Command_Status_StatusCode_SUCCESS) {
            if (cmdResponse->status().code() == Command_Status_StatusCode_NO_SPACE && batchCmd->isDelete()) {
                // OK. Always accept deletions at no space.  Do nothing.
            } else {
                response.mutable_status()->set_code(cmdResponse->status().code());
                response.mutable_status()->set_statusmessage(cmdResponse->status().statusmessage());
                response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
                break;
            }
        }
        string stateName;
        if (!validator.isSupportable(batchCmd->getAuthType(), *cmd, stateName, response)) {
            response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
            break;
        }
        if (!validator.isValidClusterVersion(*cmd, response)) {
            response.mutable_header()->set_clusterversion(
                validator.getClusterVerionsStore().GetClusterVersion());
            response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
            break;
        }
        proto::Command_KeyValue const& keyvalue = cmd->body().keyvalue();

        if (batchCmd->isPut() && !validator.isAuthorized(user_id, Domain::kWrite, keyvalue.key(), request_context, response)) {
            response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
            break;
        } else if (batchCmd->isDelete() && !validator.isAuthorized(user_id, Domain::kDelete, keyvalue.key(), request_context, response)) {
            response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
            break;
        }

        if (batchCmd->isPut() && !validator.isValidKey(*cmd, response)) {
            response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
            break;
        }

        if (!keyvalue.force()) {
            if (!validator.isValidVersion(*cmd, response)) {
                response.mutable_body()->mutable_batch()->set_failedsequence(cmd->header().sequence());
            }
            // Save: Use below line instead of above line for checking into this batch set too
            // isValidVersion(validator, i, commandResponse);
        }
        status = response.status().code();
    }
    return (response.status().code() == Command_Status_StatusCode_SUCCESS);
}
bool BatchSet::addCommand(BatchCmd* cmd, Command& response) {
    bool success = true;
    batchCmds_.push_back(cmd);
    return success;
}

} /* namespace cmd */
} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */
