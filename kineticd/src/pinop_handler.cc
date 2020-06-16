#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "glog/logging.h"
#include "product_flags.h"
#include "pinop_handler.h"
#include "kinetic_state.h"
#include "server.h"
#include "mount_manager.h"
#include "connection_handler.h"
#include "command_line_flags.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_NOT_AUTHORIZED;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;
using proto::Command_Status_StatusCode_INVALID_REQUEST;
using proto::Command_PinOperation_PinOpType_UNLOCK_PINOP;
using proto::Command_PinOperation_PinOpType_LOCK_PINOP;
using proto::Command_PinOperation_PinOpType_ERASE_PINOP;
using proto::Command_PinOperation_PinOpType_SECURE_ERASE_PINOP;
using com::seagate::kinetic::SecurityManager;
using com::seagate::kinetic::MountManager;
using com::seagate::kinetic::PinIndex;

PinOpHandler::PinOpHandler(SkinnyWaistInterface& skinny_waist,
                           const string mountpoint,
                           const string partition,
                           STATIC_DRIVE_INFO static_drive_info)
                           :skinny_waist_(skinny_waist),
                           mount_point_(mountpoint),
                           partition_(partition),
                           static_drive_info_(static_drive_info) {
    server_ = NULL;
    connHandler_ = NULL;
}

void PinOpHandler::ProcessRequest(const proto::Command &command,
        IncomingValueInterface* request_value,
        proto::Command *command_response,
        RequestContext& request_context,
        const proto::Message_PINauth& pin_auth,
        Connection* connection) {
    StoreOperationStatus status = StoreOperationStatus_INTERNAL_ERROR;
    bool success = true;
    PinStatus sed_status = PinStatus::INTERNAL_ERROR;;
    SecurityManager sed_manager;
#ifndef ISE_AND_LOCK_DISABLED
    int state_result;
#endif

    MountManager mount_manager;

    switch (command.body().pinop().pinoptype()) {
    #ifndef ISE_AND_LOCK_DISABLED
    case Command_PinOperation_PinOpType_UNLOCK_PINOP:
        VLOG(1) << "Unlock command received";
        state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::UNLOCK,
                                                        proto::Message_AuthType_PINAUTH,
                                                        command.header().messagetype(),
                                                        command_response);
        if (state_result == 0) {
            sed_status = PinStatus::PIN_SUCCESS;
            break;
        } else if (state_result < 0) {
            return;
        }

        sed_status = sed_manager.Unlock(pin_auth.pin());
        success = (sed_status == PinStatus::PIN_SUCCESS);
        server_->StateChanged(com::seagate::kinetic::StateEvent::UNLOCKED, success);

        if (success) {
            server_->StateChanged(com::seagate::kinetic::StateEvent::RESTORE);

            switch (skinny_waist_.InitUserDataStore()) {
                case UserDataStatus::SUCCESSFUL_LOAD:
                    server_->StateChanged(com::seagate::kinetic::StateEvent::RESTORED);
                    sed_status = PinStatus::PIN_SUCCESS;
                    break;
                case UserDataStatus::STORE_CORRUPT:
                    // even though the database comes back corrupt, the unlock was still successful
                    server_->StateChanged(com::seagate::kinetic::StateEvent::STORE_CORRUPT);
                    sed_status = PinStatus::PIN_SUCCESS;
                    break;
                default:
                    server_->StateChanged(com::seagate::kinetic::StateEvent::STORE_INACCESSIBLE);
                    sed_status = PinStatus::INTERNAL_ERROR;
                    break;
            }
        }
        break;

    case Command_PinOperation_PinOpType_LOCK_PINOP:
        if (EmptyPin(command_response, pin_auth) ||
            (!ValidPin(sed_manager, command_response, pin_auth, PinIndex::LOCKPIN))) {
            return;
        }

        { //open local scope
            bool dbOpened = skinny_waist_.IsDBOpen();
            state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::LOCK,
                                                        proto::Message_AuthType_PINAUTH,
                                                        command.header().messagetype(),
                                                        command_response);
            if (state_result == 0) {
                sed_status = PinStatus::PIN_SUCCESS;
                break;
            } else if (state_result < 0) {
                return;
            }
            skinny_waist_.CloseDB(); // Always True
            bool is_mounted = true; // Remains True on ARM
            if (mount_manager.Unmount(mount_point_)) {
                sed_status = sed_manager.Lock(pin_auth.pin());
                if (sed_status != PinStatus::PIN_SUCCESS) {
                    if (is_mounted && dbOpened) { skinny_waist_.InitUserDataStore();}
                    success = false;
                }
            } else {
                if (is_mounted && dbOpened) { skinny_waist_.InitUserDataStore(); }
                success = false;
            }

            server_->StateChanged(com::seagate::kinetic::StateEvent::LOCKED, success);
        } //close local scope
        break;
        #endif //close ifndef ISE_AND_LOCK_DISABLED for LOCK & UNLOCK cases
    case Command_PinOperation_PinOpType_ERASE_PINOP:
        if (!ValidPin(sed_manager, command_response, pin_auth, PinIndex::ERASEPIN)) {
            //TODO(Gonzalo): Do not think we need to log anything
            LOG(ERROR) << "InvalidPin";
            return;
        }
        state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::ISE,
                                                        proto::Message_AuthType_PINAUTH,
                                                        command.header().messagetype(),
                                                        command_response);
        if (state_result < 0) {
            return;
        }
        status = skinny_waist_.Erase(pin_auth.pin());
        success = (status == StoreOperationStatus_SUCCESS);
        server_->StateChanged(StateEvent::ISED, success);
        server_->StateChanged(StateEvent::RESTORE);
        // Set sed_status appropriately
        if (success) {
            sed_status = PinStatus::PIN_SUCCESS;
            server_->StateChanged(StateEvent::RESTORED, success);
            //Reset Erase pin to empty string
            switch (sed_manager.SetPin(
                "",
                pin_auth.pin(),
                PinIndex::ERASEPIN,
                static_drive_info_.drive_sn,
                static_drive_info_.supports_SED,
                static_drive_info_.sector_size,
                static_drive_info_.non_sed_pin_info_sector_num)) {
                case PinStatus::PIN_SUCCESS:
                    break;
                case PinStatus::AUTH_FAILURE:
                    sed_status = PinStatus::AUTH_FAILURE;
                    break;
                default:
                    sed_status = PinStatus::INTERNAL_ERROR;
                    break;
            }
        } else if (status == StoreOperationStatus_AUTHORIZATION_FAILURE) {
            sed_status = PinStatus::AUTH_FAILURE;
            server_->StateChanged(StateEvent::RESTORED, success);
        } else if (status == StoreOperationStatus_ISE_FAILED_VAILD_DB) {
            sed_status = PinStatus::INTERNAL_ERROR;
            server_->StateChanged(StateEvent::RESTORED, success);
        } else {
            sed_status = PinStatus::INTERNAL_ERROR;
            server_->StateChanged(StateEvent::STORE_INACCESSIBLE, success);
        }
        break;
    #ifndef ISE_AND_LOCK_DISABLED
    case Command_PinOperation_PinOpType_SECURE_ERASE_PINOP:
    {
        if (!ValidPin(sed_manager, command_response, pin_auth, PinIndex::ERASEPIN)) {
            //TODO(Gonzalo): Do not think we need to log anything
            LOG(ERROR) << "InvalidPin";
            return;
        }

        state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::ISE,
                                                        proto::Message_AuthType_PINAUTH,
                                                        command.header().messagetype(),
                                                        command_response);
        if (state_result < 0) {
            return;
        }
        status = skinny_waist_.InstantSecureErase(pin_auth.pin());
        success = (status == StoreOperationStatus_SUCCESS);
        server_->StateChanged(StateEvent::ISED, success);
        server_->StateChanged(StateEvent::RESTORE);
        // Set sed_status appropriately
        if (success) {
            sed_status = PinStatus::PIN_SUCCESS;
            server_->StateChanged(StateEvent::RESTORED, success);
        } else if (status == StoreOperationStatus_AUTHORIZATION_FAILURE) {
            sed_status = PinStatus::AUTH_FAILURE;
            server_->StateChanged(StateEvent::RESTORED, success);
        } else if (status == StoreOperationStatus_ISE_FAILED_VAILD_DB) {
            sed_status = PinStatus::INTERNAL_ERROR;
            server_->StateChanged(StateEvent::RESTORED, success);
        } else {
            sed_status = PinStatus::INTERNAL_ERROR;
            server_->StateChanged(StateEvent::STORE_INACCESSIBLE, success);
        }

        break;
    }
    #endif
    default:
        LOG(ERROR) << "Unknown pinop requested " << command.body().pinop().pinoptype();//NO_SPELL
        break;
    }

    switch (sed_status) {
        case PinStatus::PIN_SUCCESS:
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_SUCCESS);
            break;
        case PinStatus::AUTH_FAILURE:
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            break;
        default:
            LOG(ERROR) << "IE Pin";
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
    if (request_value->GetUserValue()) {
        smr::DynamicMemory::getInstance()->deallocate(request_value->size());
    }
    request_value->FreeUserValue();
}
void PinOpHandler::DestroyAllBatchSets(Connection* connection) {
    LOG(INFO) << "===== Enter DestroyAllBatchSets, ISE conn fd: " << connection->fd();
    LOG(INFO) << "      #of connections: " << Server::connection_map.TotalConnectionCount();
    LOG(INFO) << "      #of batch sets: " << ConnectionHandler::_batchSetCollection.numberOfBatchsets();
    unordered_map<string, BatchSet*>::iterator batchSetIt = ConnectionHandler::_batchSetCollection.begin();
    while (batchSetIt != ConnectionHandler::_batchSetCollection.end()) {
        BatchSet* batchSet = batchSetIt->second;
        std::shared_ptr<Connection> connPtr =  Server::connection_map.GetConnection(batchSet->connFd());
        if (connPtr != nullptr) {
            Connection* conn = connPtr.get();
            if (conn->fd() != connection->fd()) {
                ConnectionRequestResponse requestResponse;
                requestResponse.SetResponseCommand(Command_Status_StatusCode_INVALID_BATCH, "Device will be ISEd");
                int connFd = conn->fd();
                ConnectionHandler::_batchSetCollection.deleteBatchesOnConnection(conn->id());
                connHandler_->SendUnsolicitedStatus(conn, &requestResponse);
                connHandler_->CloseConnection(connFd);
                batchSetIt = ConnectionHandler::_batchSetCollection.begin();
            } else {
                ++batchSetIt;
            }
        } else {
            // This case should never happen
            ++batchSetIt;
        }
    }
    LOG(INFO) << "      #of connections: " << Server::connection_map.TotalConnectionCount();
    LOG(INFO) << "      #of batch sets: " << ConnectionHandler::_batchSetCollection.numberOfBatchsets();
    LOG(INFO) << "===== Exit DestroyAllBatchSets, ISE conn fd: " << connection->fd();
}
bool PinOpHandler::EmptyPin(proto::Command *command_response,
    const proto::Message_PINauth& pin_auth) {
    if (pin_auth.pin().length() > 0) {
        return false;
    }
    // If we got here then we know the pin provided was empty
    command_response->mutable_status()->
            set_code(Command_Status_StatusCode_INVALID_REQUEST);
    command_response->mutable_status()->
            set_statusmessage("Cannot invoke command with an empty pin");
    return true;
}

bool PinOpHandler::ValidPin(SecurityInterface& sed_manager, proto::Command *command_response,
    const proto::Message_PINauth& pin_auth, PinIndex pin_index) {
    #ifdef ISE_AND_LOCK_DISABLED
    switch (sed_manager.SetPin(
        "",
        pin_auth.pin(),
        pin_index,
        static_drive_info_.drive_sn,
        static_drive_info_.supports_SED,
        static_drive_info_.sector_size,
        static_drive_info_.non_sed_pin_info_sector_num)) {
    #else
    switch (sed_manager.SetPin(
        pin_auth.pin(),
        pin_auth.pin(),
        pin_index,
        static_drive_info_.drive_sn,
        static_drive_info_.supports_SED,
        static_drive_info_.sector_size,
        static_drive_info_.non_sed_pin_info_sector_num)) {
    #endif
        case PinStatus::PIN_SUCCESS:
            return true;
        case PinStatus::AUTH_FAILURE:
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            break;
        default:
            LOG(ERROR) << "IE Pin";
            command_response->mutable_status()->
                    set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            break;
    }
    return false;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
