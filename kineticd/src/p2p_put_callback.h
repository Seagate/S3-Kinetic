#ifndef KINETIC_P2P_PUT_CALLBACK_H_
#define KINETIC_P2P_PUT_CALLBACK_H_

#include <sstream>

namespace com {
namespace seagate {
namespace kinetic {

using std::stringstream;

using ::kinetic::KineticStatus;
using proto::Command_Status_StatusCode;
using proto::Command_Status_StatusCode_IsValid;
using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;

/////////////////////////////////////////////////////////
/// Callback for P2P Puts
/// Used in association with P2PPut Handler by CPP client
///
/// @constructor param[in] outstanding_puts -# of puts !complete, decrement on success
/// @constructor param[in] status -response_command's status
/// @constructor param[in] successful_operation_count -# of successful ops,increment on success

class P2PPutCallback : public ::kinetic::PutCallbackInterface {
    public:
    P2PPutCallback(int* outstanding_puts,
            proto::Command_Status* status,
            int* successful_operation_count,
            size_t* heuristic_cache_size,
            size_t heuristic_operation_size) :
            outstanding_puts_(outstanding_puts),
            status_(status),
            successful_operation_count_(successful_operation_count),
            heuristic_cache_size_(heuristic_cache_size),
            heuristic_operation_size_(heuristic_operation_size) {}

    /////////////////////////////////////////////////////////
    /// Successful Callback for P2P Puts
    /// Used in association with P2PPut Handler by CPP client Receiver
    /// Sender and Service Classes.
    ///
    /// Usually Called by Receiver or Sender Class via handler_->handle()
    /// function call (handle() hooks up to this callback's Success())
    virtual void Success() {
        VLOG(2) << "P2P put succeeded";//NO_SPELL
        *outstanding_puts_ = *outstanding_puts_ - 1;
        *successful_operation_count_ = *successful_operation_count_ + 1;
        *heuristic_cache_size_ -= heuristic_operation_size_;
        status_->clear_statusmessage();
        status_->set_code(Command_Status_StatusCode_SUCCESS);
    }

    /////////////////////////////////////////////////////////////////
    /// Successful Callback for P2P Puts
    /// Used in association with P2PPut Handler by CPP client Receiver
    /// Sender and Service Classes.
    ///
    /// Usually Called by Receiver or Sender Class via handler_->Error()
    /// function call (Error() hooks up to this callback's Failure())
    ///
    /// @param[in] error -error code passed from Handler in CPP client
    virtual void Failure(KineticStatus error) {
        VLOG(2) << "P2P Put failed: " << static_cast<int>(error.statusCode());//NO_SPELL
        *outstanding_puts_ = *outstanding_puts_ - 1;
        *heuristic_cache_size_ -= heuristic_operation_size_;

        // The embedded client & kineticd may be using different versions of the protocol
        // so we have to try to convert the status encoded in one version of the protocol
        // into a status encoded in the other version. It will usually succeed but may not
        com::seagate::kinetic::client::proto::Command_Status_StatusCode client_proto_code =
                ::kinetic::ConvertToProtoStatus(error.statusCode());

        int client_status_code_value = static_cast<int>(client_proto_code);

        if (Command_Status_StatusCode_IsValid(client_status_code_value)) {
            status_->set_code(static_cast<Command_Status_StatusCode>(client_status_code_value));
            status_->set_statusmessage("PUT failed with message: " + error.message());
        } else {
            LOG(ERROR) << "IE p2p";//NO_SPELL
            status_->set_code(Command_Status_StatusCode_INTERNAL_ERROR);

            stringstream message;
            message << "Unable to translate client status code due to "
                    << "protobuf mismatch between kineticd and embedded client. "
                    << "Client code: <" << client_status_code_value
                    << "> Client message: \"" << error.message() << "\"";
            status_->set_statusmessage(message.str());
        }
    }

    private:
    int* outstanding_puts_;
    proto::Command_Status* status_;
    int* successful_operation_count_;
    size_t* heuristic_cache_size_;
    size_t heuristic_operation_size_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_P2P_PUT_CALLBACK_H_
