#include "pending_cmd_list_proxy.h"
#include "glog/logging.h"
namespace com {
namespace seagate {
namespace kinetic {

void PendingCmdListProxy::SendStatus(bool success,
                                     std::unordered_map<int64_t, uint64_t> token_list) {
    send_pending_status_sender_->SendAllPending(success, token_list);
}

void PendingCmdListProxy::HaltTimer() {
    send_pending_status_sender_->HaltTimer();
}

void PendingCmdListProxy::SetListOwnerReference(
        SendPendingStatusInterface* send_pending_status_sender) {
    send_pending_status_sender_ = send_pending_status_sender;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
