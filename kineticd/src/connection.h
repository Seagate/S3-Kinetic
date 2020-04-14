#ifndef KINETIC_CONNECTION_H_
#define KINETIC_CONNECTION_H_

#include <queue>
#include <vector>
#include <utility>
#include <stdint.h>
#include <tuple>
#include <arpa/inet.h>
#include <iostream>
#include <memory>
#include <time.h>

#include "kinetic/message_stream.h"
#include "pthread.h"
#include "kinetic.pb.h"
#include "connection_time_handler.h"
#include "log_ring_buffer.h"
#include "outgoing_value.h"
#include "kinetic/incoming_value.h"
#include "util/mutexlock.h"
#include "user.h"

using namespace std; //NOLINT

#include "util/mutexlock.h"
#include "port/port_posix.h"

using namespace leveldb; // NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using namespace proto;//NOLINT
using namespace com::seagate::kinetic;//NOLINT
using ::kinetic::MessageStreamInterface;
using ::kinetic::IncomingValueInterface;
using ::kinetic::IncomingBuffValue;

enum class ConnectionState {
    IDLE,
    BUSY,
    SHOULD_BE_CLOSED,
    CLOSED
};

class Connection;

class ConnectionRequestResponse {
    public:
    static const uint DEFAULT_CMD_TIMEOUT = 3600000;  // milliseconds or 3600 seconds

    ConnectionRequestResponse();
    ~ConnectionRequestResponse();
    // ConnectionTimeHandler specific methods
    bool Interrupted();
    bool TimedOut();
    void SetTimeDequeued();
    bool ExceededTimeInQueue();
    void SetMaxTimeInQueue();
    bool ExceededMaxResponseTime();
    void SetMaxResponseTime();
    void ResetMaxResponseTime();
    void SetNewMaxStartEndBatchResp(std::chrono::milliseconds time);
    std::chrono::milliseconds GetMaxStartEndBatchResp();
    void SetNewMaxStartEndBatchRcv(std::chrono::milliseconds time);
    std::chrono::milliseconds GetMaxStartEndBatchRcv();
    std::chrono::milliseconds MaxTimeInQueue();
    std::chrono::milliseconds MaxResponseTime();
    std::chrono::high_resolution_clock::time_point EnqueuedTime();
    std::chrono::high_resolution_clock::time_point DequeuedTime();
    uint64_t Timeout();
    void SetTimeout(uint64_t value);
    int TimeElapsed();
    int TimeProcessElapsed();
    void ResetInterrupt();
    void SetTimeResponded();
    std::chrono::high_resolution_clock::time_point GetTimeResponded();
    void SetTimeProcessed();
    void SetTimeQuanta(uint64_t value);
    void SetTimeQueued(std::chrono::high_resolution_clock::time_point qTime);
    // Response Specific methods
    void SetResponseCommand(Command_Status_StatusCode code, std::string status_message);
    void SetResponseCommand(int64_t connection_id);
    void SetResponseCommand(Message_AuthType message_authtype);
    // Setters / Mutators for instance variables
    void SetRequestValue(IncomingValueInterface *request_value);
    std::chrono::milliseconds GetMaxResponseTimeForOperation();
    Message *request();
    void SetRequest(Message* req) {
        request_ = req;
    }
    Command *command();
    void SetCommand(Command* cmd) {
        delete command_;
        command_ = cmd;
    }
    Message *response();
    Command *response_command();
    void SetResponseCommand(Command* respCmd) {
        delete response_command_;
        response_command_ = respCmd;
    }
    IncomingValueInterface *request_value();
    NullableOutgoingValue  *response_value();

    ConnectionTimeHandler* time_handler();
    void SetTimeHandler(ConnectionTimeHandler* handler) {
        time_handler_ = handler;
    }
    void SetConnection(std::shared_ptr<Connection> conn) {
        connection_ = conn;
    }
    shared_ptr<Connection> GetConnection() {
        return connection_;
    }
    int getPriority() const {
        return  command_->header().priority();
    }
    void setPriority(com::seagate::kinetic::proto::Command_Priority n) {
        command_->mutable_header()->set_priority(n);
    }

    uint64_t getOrder() const {
        return order_;
    }
    void setOrder(uint64_t n) {
        order_ = n;
    }
    void setExpiredTime() {
        struct timespec timeNow;
        clock_gettime(CLOCK_MONOTONIC, &timeNow);
        //  times are converted to milliseconds
        uint64_t nano = (uint64_t)timeNow.tv_sec*1000*1000*1000 + (uint64_t)timeNow.tv_nsec;
        expiredTime_ = nano  + (uint64_t)Timeout()*1000000;
        VLOG(4) << " SET EXPIRED " << Timeout()*1000000 << " " << nano << " " << expiredTime_;
    }
    uint64_t getExpiredTime() const {
        return expiredTime_;
    }

    bool operator <(const ConnectionRequestResponse& rhs) const {
        return (getExpiredTime() > rhs.getExpiredTime());
    }

    void user(const User& aUser) {
        // We only want to save user id and key
        user_.id(aUser.id());
        user_.key(aUser.key());
    }
    const User& user() const {
        return user_;
    }

    public:
      struct Comparator {
          bool operator() (ConnectionRequestResponse* arg1, ConnectionRequestResponse* arg2) {
              return  (*arg1) < (*arg2);
          }
      };

    private:
    uint64_t order_;
    Message* request_;
    Command* command_;
    Message* response_;
    Command* response_command_;
    IncomingValueInterface* request_value_;
    NullableOutgoingValue* response_value_;
    // Times are in milliseconds
    uint64_t expiredTime_;
    ConnectionTimeHandler* time_handler_;

    shared_ptr<Connection> connection_;
    User user_;
};

class Connection {
    public:
    static unsigned int queue_limit;
    static const int PENDING_STATUS_MAX;
    explicit Connection(int fd);
    ~Connection();
    MessageStreamInterface::MessageStreamReadStatus ReadMessage(ConnectionRequestResponse *connection_request_response);
    MessageStreamInterface::MessageStreamReadStatus ReadHeader(uint32_t& msgSize,
        uint32_t& valSize, int* err) {
        return message_stream_->ReadHeader(&msgSize, &valSize, err);
    }

    MessageStreamInterface::MessageStreamReadStatus ReadMessageAPI(uint32_t msgSize, ConnectionRequestResponse *connection_request_response) {
        return message_stream_->ReadMessageAPI(connection_request_response->request(),
                msgSize);
    }
    MessageStreamInterface::MessageStreamReadStatus ReadValue(char* valBuffer, uint32_t valSize,
                                                              ConnectionRequestResponse *connection_request_response) {
        MessageStreamInterface::MessageStreamReadStatus status =
                message_stream_->ReadValueToBuffer(valBuffer, valSize);
        if (status == MessageStreamInterface::MessageStreamReadStatus_SUCCESS) {
            IncomingBuffValue* val = new IncomingBuffValue(valBuffer, valSize);
            connection_request_response->SetRequestValue(val);
        } else {
            connection_request_response->SetRequestValue(NULL);
        }
        return status;
    }
    MessageStreamInterface::MessageStreamReadStatus ReadValue(uint32_t valSize, ConnectionRequestResponse *connection_request_response) {
        IncomingValueInterface* value = NULL;
        MessageStreamInterface::MessageStreamReadStatus status =
                message_stream_->ReadValue(&value, valSize);
        connection_request_response->SetRequestValue(value);
        return status;
    }
    int SendMessage(ConnectionRequestResponse *connection_request_response,
                    NullableOutgoingValue* message_value, int* err);
    void AddCurrentToResponsePending(ConnectionRequestResponse* connection_request_response);
    void AddToResponsePending(ConnectionRequestResponse* connection_request_response);

    bool operator <(const Connection& rhs) const {
        if (priority_ < rhs.priority_) {
            return true;
        }
        return false;
    }

    ConnectionRequestResponse* Dequeue(uint64_t ack_sequence, bool success);
    ConnectionRequestResponse* Dequeue();
    // Setters / Mutators for instance variables
    void SetUseSSL(bool use_ssl);
    void SetSSl(SSL *ssl);
    void SetMessageStream(MessageStreamInterface *message_stream);
    void SetId(int64_t id);
    void SetState(ConnectionState state);
    void SetLastMsgSeq(uint64_t lastMsgSeq);
    void SetStatusForResponse(bool status_for_response);
    void UpdateMostRecentAccessTime();
    void SetMaxPriority(int priority) {
        MutexLock lock(&mu_);
        maxPriority_ = priority;
    }

    void SetPriority(int priority);
    void SetShouldBeClosed(bool should_be_closed);
    bool HasPendingStatuses();
    bool HasStatusesPending();

    int NumPendingStatus();
    bool use_ssl();
    SSL *ssl();
    int fd();
    void fd(int aFd) {
        fd_ = aFd;
    }
    int64_t id();
    ConnectionState state();
    uint64_t lastMsgSeq();
    bool lastMsgSeqHasBeenSet();
    bool status_for_response();
    uint64_t most_recent_access_time();
    int maxPriority() {
        return maxPriority_;
    }
    int priority();
    bool should_be_closed();
    void set_idle(bool idle);
    void wait_till_idle();
    void set_cmd_in_progress(bool in_progress);
    void set_response_in_progress(bool in_progress);
    void increment_cmds_count() { MutexLock lock(&mu_);
                                  number_of_commands_remained_++;}
    void decrement_cmds_count() { MutexLock lock(&mu_);
                                  number_of_commands_remained_--;}
    uint  get_cmds_count() { MutexLock lock(&mu_);
                             uint number = number_of_commands_remained_;
                             return number;}
    size_t numberOfPendingResponses() {
        return p_request_responses_.size();
    }
    size_t numberOfResponses() {
        return pending_connection_request_responses_.size();
    }
    void setEnqueuedTime(std::chrono::high_resolution_clock::time_point enqueued_time) {
        enqueued_time_ = enqueued_time;
    }

    std::chrono::high_resolution_clock::time_point getEnqueuedTime() {
        return enqueued_time_;
    }

    private:
    ConnectionState state_;
    int fd_;
    bool use_ssl_;
    bool should_be_closed_;
    SSL *ssl_;
    uint64_t most_recent_access_time_;
    int64_t id_;
    uint64_t max_response_time_;
    uint64_t lastMsgSeq_;
    bool lastMsgSeqHasBeenSet_;
    bool status_for_response_;
    int maxPriority_;
    int priority_;
    uint number_of_commands_remained_;
    bool cmd_in_progress_;
    bool response_in_progress_;
    MessageStreamInterface* message_stream_;
    std::queue<ConnectionRequestResponse*> pending_connection_request_responses_;
    std::queue<ConnectionRequestResponse*> p_request_responses_;

    port::Mutex mu_;
    port::CondVar cv_;
    std::chrono::high_resolution_clock::time_point enqueued_time_;

    DISALLOW_COPY_AND_ASSIGN(Connection);
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_CONNECTION_H_
