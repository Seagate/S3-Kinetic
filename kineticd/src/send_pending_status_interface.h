#ifndef KINETIC_SEND_PENDING_STATUS_INTERFACE_H_
#define KINETIC_SEND_PENDING_STATUS_INTERFACE_H_

#include "gmock/gmock.h"
#include <tuple>
#include <unordered_map>

namespace com {
namespace seagate {
namespace kinetic {
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/// SendPendingStatusInterface
///     -Extended By: @ConnectionHandler
/// -------------------------------------------
/// ***This class exists simply to satisfy the kineticd test suite***
/// -------------------------------------------
/// - On Mem table compaction, the DB instructs it's cmd list proxy to send all statuses
/// - The Proxy originally linked up w/ ConnectionHandler (owner of pending status collection),
///    and since no Mock ConnectionHandler exists, tests triggering memtable compaction would fail
/// - Though not ideal, this interface results in the least amount of coupling and prevents us
///    from having to pass the ConnectionHandler around, or make an entire mock class.
///------------------------------------------------------
class SendPendingStatusInterface {
    public:
    virtual ~SendPendingStatusInterface() {}
    virtual void SendAllPending(bool success, std::unordered_map<int64_t, uint64_t> token_list) = 0;
    virtual void HaltTimer() = 0;
};

class MockSendPendingStatusInterface: public SendPendingStatusInterface {
    public:
    MockSendPendingStatusInterface() {}
    MOCK_METHOD2(SendAllPending,
                 void(bool success, std::unordered_map<int64_t, uint64_t> token_list));
    MOCK_METHOD0(HaltTimer, void());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SEND_PENDING_STATUS_INTERFACE_H_
