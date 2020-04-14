#ifndef KINETIC_PENDING_CMD_LIST_PROXY_H_
#define KINETIC_PENDING_CMD_LIST_PROXY_H_
#include "leveldb/env.h"
#include "send_pending_status_interface.h"

namespace com {
namespace seagate {
namespace kinetic {
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/// PendingCmdListProxy
///     -Extends: @OutstandingStatusSender (leveldb)
/// -------------------------------------------
/// @Summary:
/// - Creates a Proxy between the DB & Status Collection owner
///   in order to allow the DB to pass along Success Status of
///   MemTable compaction to commands being retained in the outstanding collection
/// -------------------------------------------
/// @Member Variables
/// -send_pending_status_sender_: interface to oustanding status collection owner(ConnectionHandler)
///------------------------------------------------------
class PendingCmdListProxy : public leveldb::OutstandingStatusSender {
    public:
    /////////////////////////////////////////////////////////
    /// SendStatus()
    /// -------------------------------------------
    /// -Send boolean indicating MemTable compaction results &
    /// a token list of each commands sequence id & connection id (tokens match collection keys)
    virtual void SendStatus(bool success, std::unordered_map<int64_t, uint64_t> token_list);

    /////////////////////////////////////////////////////////
    /// HaltTimer()
    /// -------------------------------------------
    /// -Halt current aging timer
    virtual void HaltTimer();

    /////////////////////////////////////////////////////////
    /// SetListOwnerReference()
    /// -------------------------------------------
    /// -Establish reference to owner of outstanding status collection
    /// -Owner will be called upon when time to send pending statuses
    ///------------------------------------------------------
    void SetListOwnerReference(SendPendingStatusInterface* send_pending_status_sender);

    private:
    SendPendingStatusInterface *send_pending_status_sender_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_PENDING_CMD_LIST_PROXY_H_
