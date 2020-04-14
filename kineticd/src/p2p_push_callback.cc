#include <sstream>
#include <vector>
#include "p2p_push_callback.h"
#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::stringstream;
using std::unique_ptr;
using std::vector;

using ::kinetic::KineticStatus;
using proto::Command_Status;
using proto::Command_Status_StatusCode;
using proto::Command_Status_StatusCode_IsValid;
using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;
using proto::Command_Status_StatusCode_NESTED_OPERATION_ERRORS;
using proto::Command_P2POperation_Operation;

/////////////////////////////////////////////////////////
/// Callback for P2P Pushes (piped pushes / copy drive)
/// Used in association with P2PPush Handler by CPP client
///
/// @constructor param[in] outstanding_pushes -# of pushes !complete, decrement on success
/// @constructor param[in] status  -response_command's status
/// @constructor param[in] successful_operation_count -# of successful ops,increment on success
/// @constructor param[in] p2pop -ptr to @result_nested_op located in p2p op executor

P2PPushCallback::P2PPushCallback(int* outstanding_pushes,
        proto::Command_Status* status,
        int* successful_operation_count,
        size_t* heuristic_cache_size,
        size_t heuristic_operation_size,
        Command_P2POperation* p2pop) :
            outstanding_pushes_(outstanding_pushes),
            status_(status),
            successful_operation_count_(successful_operation_count),
            heuristic_cache_size_(heuristic_cache_size),
            heuristic_operation_size_(heuristic_operation_size),
            p2pop_(p2pop) {}

///////////////////////////////////////////////////////////////////
/// Successful Callback for P2P Pushes (piped pushes / copy drive)
/// Used in association with P2PPush Handler by CPP client Receiver
/// Sender and Service Classes.
///
/// Usually Called by Receiver or Sender Class via handler_->handle()
/// function call (handle() hooks up to this callback's Success())
///
/// @param[in] operation_statuses -vector of all sub op status's
/// @param[in] response_command

void P2PPushCallback::Success(unique_ptr<vector<KineticStatus>> operation_statuses,
        const com::seagate::kinetic::client::proto::Command& response_command) {
    VLOG(2) << "P2P Push succeeded";//NO_SPELL
    *outstanding_pushes_ = *outstanding_pushes_ - 1;

    bool client_all_child_op_success =
            response_command.body().p2poperation().allchildoperationssucceeded();
    if (client_all_child_op_success) {
        status_->clear_statusmessage();
        status_->set_code(Command_Status_StatusCode_SUCCESS);
        *successful_operation_count_ = *successful_operation_count_ + 1;
    } else {
        status_->set_code(Command_Status_StatusCode_NESTED_OPERATION_ERRORS);
        status_->set_statusmessage("At least one operation was unsuccessful");
    }

    *heuristic_cache_size_ -= heuristic_operation_size_;

    ConvertClientP2POperationResponse(response_command.body().p2poperation(), p2pop_);
}

////////////////////////////////////////////////////////////////////
/// Failure Callback for P2P Pushes (piped pushes / copy drive)
/// Used in association with P2PPush Handler by CPP client Receiver
/// Sender and Service Classes.
///
/// Usually Called by CPP Client Receiver or Sender Class via
/// handler_->Error() function call
/// ( Error() hooks up to this callback's Failure() )
///
/// @param[in] error -error code passed from Handler in CPP client
/// @param[in] response_command

void P2PPushCallback::Failure(KineticStatus error,
        com::seagate::kinetic::client::proto::Command const * const response_command) {
    VLOG(2) << "P2P Push was not completely successful: " << static_cast<int>(error.statusCode());

    *outstanding_pushes_  = *outstanding_pushes_ - 1;
    *heuristic_cache_size_ -= heuristic_operation_size_;

    // Check for NULL Conditional: Avoid Segfault on cases where response_command
    // is sent as a Null Pointer from the CPP client
    if (response_command != NULL) {
        // The embedded client & kineticd may be using different versions of the protocol
        // so we have to try to convert the status encoded in one version of the protocol
        // into a status encoded in the other version. It will usually succeed but may not
        com::seagate::kinetic::client::proto::Command_Status_StatusCode client_proto_code =
                ::kinetic::ConvertToProtoStatus(error.statusCode());

        int client_status_code_value = static_cast<int>(client_proto_code);

        if (Command_Status_StatusCode_IsValid(client_status_code_value)) {
            status_->set_code(static_cast<Command_Status_StatusCode>(client_status_code_value));
            status_->set_statusmessage("Nested P2P Push failed with message: " + error.message());
        } else {
            LOG(ERROR) << "IE P2P";//NO_SPELL
            status_->set_code(Command_Status_StatusCode_INTERNAL_ERROR);

            stringstream message;
            message << "Unable to translate client status code due to "
                    << "protobuf mismatch between kineticd and embedded client. "
                    << "Client code: <" << client_status_code_value
                    << "> Client message: \"" << error.message() << "\"";
            status_->set_statusmessage(message.str());
        }

        ConvertClientP2POperationResponse(response_command->body().p2poperation(), p2pop_);
    } // end NULL check
}

void P2PPushCallback::ConvertClientP2PStatus(
        com::seagate::kinetic::client::proto::Command_Status client_status,
        Command_Status* result_status) {
    // The embedded client & kineticd may be using different versions of the protocol
    // so we have to try to convert the status encoded in one version of the protocol
    // into a status encoded in the other version. It will usually succeed but may not

    // Check for NULL Conditional: Avoid Segfault on cases where response_command
    // is sent as a Null Pointer from the CPP client
    if (result_status != NULL) {
        int client_status_code_value = static_cast<int>(client_status.code());
        if (Command_Status_StatusCode_IsValid(client_status_code_value)) {
            result_status->set_code(
                    static_cast<Command_Status_StatusCode>(client_status_code_value));
            result_status->set_statusmessage(client_status.statusmessage());
        } else {
            LOG(ERROR) << "IE P2P";//NO_SPELL
            result_status->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            stringstream message;
            message << "Unable to translate client status code due to "
                    << "protobuf mismatch between kineticd and embedded client. "
                    << "Client code: <" << client_status_code_value
                    << "> Client message: \"" << client_status.statusmessage() << "\"";
            status_->set_statusmessage(message.str());
        }
    }
}

void P2PPushCallback::ConvertClientP2POperationResponse(
        com::seagate::kinetic::client::proto::Command_P2POperation client_p2pop,
        Command_P2POperation* result_p2pop) {
    result_p2pop->set_allchildoperationssucceeded(client_p2pop.allchildoperationssucceeded());
    result_p2pop->mutable_peer()->set_hostname(client_p2pop.peer().hostname());
    result_p2pop->mutable_peer()->set_port(client_p2pop.peer().port());
    result_p2pop->mutable_peer()->set_tls(client_p2pop.peer().tls());

    for (int i = 0; i < client_p2pop.operation_size(); ++i) {
        auto client_op = client_p2pop.operation(i);
        auto result_op = result_p2pop->add_operation();

        result_op->set_key(client_op.key());
        result_op->set_version(client_op.version());
        result_op->set_newkey(client_op.newkey());
        result_op->set_force(client_op.force());
        ConvertClientP2PStatus(client_op.status(), result_op->mutable_status());

        if (client_op.has_p2pop()) {
            ConvertClientP2POperationResponse(client_op.p2pop(), result_op->mutable_p2pop());
        }
    }
}
} // namespace kinetic
} // namespace seagate
} // namespace com
