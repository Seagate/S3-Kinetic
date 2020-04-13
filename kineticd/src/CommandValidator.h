/*
 * CommandValidator.h
 *
 *  Created on: Apr 19, 2016
 *      Author: tri
 */

#ifndef COMMANDVALIDATOR_H_ //NOLINT
#define COMMANDVALIDATOR_H_

#include "store_operation_status.h"
#include "kinetic_state.h"
#include "kinetic.pb.h"
#include "status_interface.h"
#include "primary_store_interface.h"
#include "authorizer_interface.h"
#include "cluster_version_store.h"
#include "limits.h"

using namespace com::seagate::kinetic::proto; //NOLINT
using namespace std; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class CommandValidator {
 public:
    CommandValidator(StatusInterface& server, PrimaryStoreInterface& primaryStore,
        AuthorizerInterface& authorizer, ClusterVersionStoreInterface& clusterVersionStore,
        Limits& limits) : server_(server), primaryStore_(primaryStore),
        authorizer_(authorizer), clusterVersionStore_(clusterVersionStore), limits_(limits) {
    }
    virtual ~CommandValidator() {
    }

    bool isValidVersion(const Command& command, Command& command_response);
    bool isValidKey(const Command& command, Command& command_response);
    bool doesKeyExist(const Command& command, Command& command_response);
    Command_Status_StatusCode toCommandStatus(StoreOperationStatus opStatus);

    ClusterVersionStoreInterface& getClusterVerionsStore() {
        return clusterVersionStore_;
    }
    bool isSupportable(Message_AuthType authType, Command& command,
        string& stateName, Command& command_response) {
        if (!server_.IsSupportable(authType, command, stateName)) {
            command_response.mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
            command_response.mutable_status()->set_statusmessage("Unsupportable");
            return false;
        }
        return true;
    }
    bool isSupportable(const Message& msg, const Command &command,
        string& stateName, Command& command_response) {
        if (!server_.IsSupportable(msg, command, stateName)) {
            command_response.mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
            command_response.mutable_status()->set_statusmessage("Unsupportable");
            return false;
        }
        return true;
    }
    bool isAuthorized(uint64_t userId, role_t permission, const string& key,
            RequestContext& reqContext, Command& command_response) {
        if (!authorizer_.AuthorizeKey(userId, permission, key, reqContext)) {
            command_response.mutable_status()->set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response.mutable_status()->set_statusmessage("permission denied");
            return false;
        }
        return true;
    }
    bool isValidClusterVersion(const Command& command, Command& command_response) {
        int64_t current_cluster_version = clusterVersionStore_.GetClusterVersion();
        if (current_cluster_version != command.header().clusterversion()) {
            command_response.mutable_status()->set_code(Command_Status_StatusCode_VERSION_FAILURE);
            command_response.mutable_status()->set_statusmessage("Cluster version failure");
            command_response.mutable_header()->set_clusterversion(current_cluster_version);
            return false;
        }
        return true;
    }

 private:
    StatusInterface& server_;
    PrimaryStoreInterface& primaryStore_;
    AuthorizerInterface& authorizer_;
    ClusterVersionStoreInterface& clusterVersionStore_;
    Limits limits_;
};

} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */

#endif /* COMMANDVALIDATOR_H_ */ //NOLINT
