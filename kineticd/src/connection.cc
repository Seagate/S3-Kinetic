#include "connection.h"

#include <sys/epoll.h>
#include <netdb.h>

#include "glog/logging.h"
#include <iostream>
#include <chrono>
#include <errno.h>
#include <chrono>

#include "server.h"
#include "smrdisk/Disk.h"

using namespace std::chrono; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

// Worst case 32 KiB will be used for each ConnectionRequestResponse
// Current OS
// Limit to using less than 77 MiB of memory for pending ConnectionRequestResponse objects
// PENDING_STATUS_MAX = 77 MiB / 32 KiB
// Hard constraint on number of pending statuses
// const int Connection::PENDING_STATUS_MAX = 2464;
// RAM OS
// Limit to using less than 61 MiB of memory for pending ConnectionRequestResponse objects
// PENDING_STATUS_MAX = 30.5 MiB / 32 KiB
// Hard constraint on number of pending statuses
const int Connection::PENDING_STATUS_MAX = 976;
unsigned int Connection::queue_limit = 15;

ConnectionRequestResponse::ConnectionRequestResponse() {
        request_ = new Message();
        command_ = new Command();
        response_ = new Message();
        response_command_ = new Command();
        time_handler_ = new ConnectionTimeHandler();
        request_value_ = NULL;
        response_value_ = new NullableOutgoingValue();
        response_command_->mutable_status()->
            set_code(Command_Status_StatusCode_SUCCESS);
        response_command_->mutable_status()->set_statusmessage("");
        order_ = 0;
}

ConnectionRequestResponse::~ConnectionRequestResponse() {
    if (request_ != NULL) {
        delete request_;
        request_ = NULL;
    }

    if (command_ != NULL) {
        delete command_;
        command_ = NULL;
    }

    if (response_ != NULL) {
        delete response_;
        response_ = NULL;
    }

    if (response_command_ != NULL) {
        delete response_command_;
        response_command_ = NULL;
    }

    if (time_handler_ != NULL) {
        delete time_handler_;
        time_handler_ = NULL;
    }

    if (response_value_ != NULL) {
        delete response_value_;
        response_value_ = NULL;
    }

    if (request_value_ != NULL) {
        request_value_->Consume();
        delete request_value_;
        request_value_ = NULL;
    }
}

bool ConnectionRequestResponse::Interrupted() {
    return time_handler_->GetInterrupt();
}

bool ConnectionRequestResponse::TimedOut() {
    return time_handler_->IsTimeout();
}

void ConnectionRequestResponse::SetTimeDequeued() {
    time_handler_->SetTimeDequeued();
}

void ConnectionRequestResponse::SetMaxTimeInQueue() {
    time_handler_->SetNewMaxQueueTime();
}

void ConnectionRequestResponse::SetTimeResponded() {
    time_handler_->SetTimeResponded();
}

std::chrono::high_resolution_clock::time_point ConnectionRequestResponse::GetTimeResponded() {
    return time_handler_->GetTimeResponded();
}

void ConnectionRequestResponse::SetTimeProcessed() {
    time_handler_->SetTimeProcessed();
}

bool ConnectionRequestResponse::ExceededTimeInQueue() {
    return time_handler_->TimeInQueueGTMax();
}

bool ConnectionRequestResponse::ExceededMaxResponseTime() {
    return time_handler_->TimeElapsedGTMax();
}

void ConnectionRequestResponse::SetMaxResponseTime() {
    return time_handler_->SetNewMaxResponse();
}

void ConnectionRequestResponse::SetNewMaxStartEndBatchResp(std::chrono::milliseconds time) {
    time_handler_->SetNewMaxStartEndBatchResp(time);
}

std::chrono::milliseconds ConnectionRequestResponse::GetMaxStartEndBatchResp() {
    return time_handler_->GetMaxStartEndBatchResp();
}


void ConnectionRequestResponse::SetNewMaxStartEndBatchRcv(std::chrono::milliseconds time) {
    time_handler_->SetNewMaxStartEndBatchRcv(time);
}

std::chrono::milliseconds ConnectionRequestResponse::GetMaxStartEndBatchRcv() {
    return time_handler_->GetMaxStartEndBatchRcv();
}


void ConnectionRequestResponse::ResetMaxResponseTime() {
    return time_handler_->ResetMaxResponseTime();
}

std::chrono::milliseconds ConnectionRequestResponse::MaxTimeInQueue() {
    return time_handler_->GetMaxQueueTime();
}

std::chrono::milliseconds ConnectionRequestResponse::MaxResponseTime() {
    return time_handler_->GetMaxResponseTime();
}

std::chrono::high_resolution_clock::time_point ConnectionRequestResponse::EnqueuedTime() {
    return time_handler_->GetEnqueuedTime();
}

std::chrono::high_resolution_clock::time_point ConnectionRequestResponse::DequeuedTime() {
    return time_handler_->GetDequeuedTime();
}

void ConnectionRequestResponse::ResetInterrupt() {
    time_handler_->ResetInterrupt();
}

uint64_t ConnectionRequestResponse::Timeout() {
    return time_handler_->GetTimeout();
}

void ConnectionRequestResponse::SetTimeout(uint64_t value) {
    time_handler_->SetTimeout(value);
}

int ConnectionRequestResponse::TimeElapsed() {
    return time_handler_->GetTimeElapsed();
}

int ConnectionRequestResponse::TimeProcessElapsed() {
    return time_handler_->GetTimeProcessElapsed();
}

void ConnectionRequestResponse::SetTimeQuanta(uint64_t value) {
    time_handler_->SetTimeQuanta(value);
}

void ConnectionRequestResponse::SetTimeQueued(std::chrono::high_resolution_clock::time_point qTime) {
    time_handler_->SetTimeQueued(qTime);
}

void ConnectionRequestResponse::SetResponseCommand(Command_Status_StatusCode code,
                                                   std::string status_message) {
    response_command_->mutable_status()->
        set_code(code);
    response_command_->mutable_status()->
        set_statusmessage(status_message);
}

void ConnectionRequestResponse::SetResponseCommand(int64_t connection_id) {
    response_command_->mutable_header()->set_connectionid(connection_id);
}

void ConnectionRequestResponse::SetResponseCommand(Message_AuthType message_authtype) {
    response_->set_authtype(message_authtype);
}

Message *ConnectionRequestResponse::request() {
    return request_;
}

Command *ConnectionRequestResponse::command() {
    return command_;
}

Message *ConnectionRequestResponse::response() {
    return response_;
}

Command *ConnectionRequestResponse::response_command() {
    return response_command_;
}


NullableOutgoingValue *ConnectionRequestResponse::response_value() {
    return response_value_;
}

IncomingValueInterface *ConnectionRequestResponse::request_value() {
    return request_value_;
}

ConnectionTimeHandler *ConnectionRequestResponse::time_handler() {
    return time_handler_;
}

void ConnectionRequestResponse::SetRequestValue(IncomingValueInterface *request_value) {
    request_value_ = request_value;
}

std::chrono::milliseconds ConnectionRequestResponse::GetMaxResponseTimeForOperation() {
    return time_handler_->GetResponseTimeForOperation();
}

Connection::Connection(int fd)
    : fd_(fd), cv_(&mu_) {
        ssl_ = NULL;
        max_response_time_ = 0;
        use_ssl_ = false;
        most_recent_access_time_ = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
        should_be_closed_ = false;
        state_ = ConnectionState::IDLE;
        message_stream_ = NULL;
        lastMsgSeq_ = 0;
        lastMsgSeqHasBeenSet_ = false;
        status_for_response_ = false;
        cmd_in_progress_ = false;
        response_in_progress_ = false;
        number_of_commands_remained_ = 0;
}

Connection::~Connection() {
    if (use_ssl_ && ssl_ != NULL) {
        SSL_free(ssl_);
        ssl_= NULL;
        use_ssl_ = false;
    }

    if (message_stream_ != NULL) {
        delete message_stream_;
        message_stream_ = NULL;
    }
    while (!pending_connection_request_responses_.empty()) {
        ConnectionRequestResponse *connection_request_response = pending_connection_request_responses_.front();//NOLINT
        pending_connection_request_responses_.pop();
        delete connection_request_response;
    }
    while (!p_request_responses_.empty()) {
        ConnectionRequestResponse *connection_request_response = p_request_responses_.front();//NOLINT
        p_request_responses_.pop();
        if (connection_request_response->request_value()) {
            connection_request_response->request_value()->Consume();
            delete connection_request_response->request_value();
        }
        delete connection_request_response;
    }
    if (fd_ >= 0) {
        struct epoll_event event;
        event.data.fd = fd_;
        epoll_ctl(Server::epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
    }
    if ((fd_ >= 0) && (close(fd_) != 0)) {
        PLOG(ERROR) << "Failed to close connection file descriptor, fd = " << fd_;
    }
}

MessageStreamInterface::MessageStreamReadStatus Connection::ReadMessage(ConnectionRequestResponse *connection_request_response) {
    MutexLock lock(&mu_);
    IncomingValueInterface* request_value = NULL;
    MessageStreamInterface::MessageStreamReadStatus status;
    status = message_stream_->ReadMessage(connection_request_response->request(),
                &(request_value));
    if (status == MessageStreamInterface::MessageStreamReadStatus_SUCCESS || status == MessageStreamInterface::MessageStreamReadStatus_NO_SPACE) {
        connection_request_response->SetRequestValue(request_value);
    } else {
        connection_request_response->SetRequestValue(NULL);
    }
    return status;
}

int Connection::SendMessage(ConnectionRequestResponse *connection_request_response,
                            NullableOutgoingValue* message_value, int* err) {
    MutexLock lock(&mu_);
    VLOG(4) << " SERIALIZE AND SEND " << connection_request_response->response_command()->DebugString();
    if (message_value == NULL) {
        NullableOutgoingValue value;
        return message_stream_->
            WriteMessage(*(connection_request_response->response()), value, err);
    } else {
        return message_stream_->
            WriteMessage(*(connection_request_response->response()), *message_value, err);
    }
}

void Connection::AddCurrentToResponsePending(ConnectionRequestResponse* connection_request_response) {
    MutexLock lock(&mu_);
    pending_connection_request_responses_.push(connection_request_response);
}

void Connection::AddToResponsePending(ConnectionRequestResponse* connection_request_response) {
    MutexLock lock(&mu_);
    while (p_request_responses_.size() >= queue_limit) {
        cv_.SignalAll();
        cv_.TimedWait();
    }
    p_request_responses_.push(connection_request_response);
}

bool Connection::use_ssl() {
    MutexLock lock(&mu_);
    return use_ssl_;
}

void Connection::SetUseSSL(bool use_ssl) {
    MutexLock lock(&mu_);
    use_ssl_ = use_ssl;
}

void Connection::SetSSl(SSL *ssl) {
    MutexLock lock(&mu_);
    ssl_ = ssl;
}

SSL *Connection::ssl() {
    MutexLock lock(&mu_);
    return ssl_;
}

void Connection::SetMessageStream(MessageStreamInterface *message_stream) {
    MutexLock lock(&mu_);
    message_stream_ = message_stream;
}

int Connection::fd() {
    MutexLock lock(&mu_);
    return fd_;
}

int64_t Connection::id() {
    MutexLock lock(&mu_);
    return id_;
}

void Connection::SetId(int64_t id) {
    MutexLock lock(&mu_);
    id_ = id;
}

ConnectionState Connection::state() {
    MutexLock lock(&mu_);
    return state_;
}

void Connection::SetState(ConnectionState state) {
    MutexLock lock(&mu_);
    state_ = state;
}

uint64_t Connection::lastMsgSeq() {
    MutexLock lock(&mu_);
    return lastMsgSeq_;
}

void Connection::SetLastMsgSeq(uint64_t lastMsgSeq) {
    MutexLock lock(&mu_);
    lastMsgSeqHasBeenSet_ = true;
    lastMsgSeq_ = lastMsgSeq;
}

bool Connection::lastMsgSeqHasBeenSet() {
    MutexLock lock(&mu_);
    return lastMsgSeqHasBeenSet_;
}

void Connection::SetStatusForResponse(bool status_for_response) {
    MutexLock lock(&mu_);
    status_for_response_ = status_for_response;
}

bool Connection::status_for_response() {
    MutexLock lock(&mu_);
    return status_for_response_;
}

uint64_t Connection::most_recent_access_time() {
    MutexLock lock(&mu_);
    return most_recent_access_time_;
}

void Connection::UpdateMostRecentAccessTime() {
    MutexLock lock(&mu_);
    most_recent_access_time_ = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

int Connection::priority() {
    MutexLock lock(&mu_);
    return priority_;
}

void Connection::SetPriority(int priority) {
    MutexLock lock(&mu_);
    priority_ = priority;
}

bool Connection::should_be_closed() {
    MutexLock lock(&mu_);
    return should_be_closed_;
}

void Connection::wait_till_idle() {
    MutexLock lock(&mu_);
    while (cmd_in_progress_ || response_in_progress_) {
        cv_.Wait();
    }
}

void Connection::set_cmd_in_progress(bool in_progress) {
    MutexLock lock(&mu_);
    cmd_in_progress_ = in_progress;
    cv_.Signal();
}
void Connection::set_response_in_progress(bool in_progress) {
    MutexLock lock(&mu_);
    response_in_progress_ = in_progress;
    cv_.Signal();
}

void Connection::SetShouldBeClosed(bool should_be_closed) {
    MutexLock lock(&mu_);
    should_be_closed_ = should_be_closed;
}

bool Connection::HasPendingStatuses() {
    MutexLock lock(&mu_);
    return (pending_connection_request_responses_.size() > 0);
}

bool Connection::HasStatusesPending() {
    MutexLock lock(&mu_);
    bool write_through_pending = !pending_connection_request_responses_.empty();
    bool response_pending = !p_request_responses_.empty();
    return (write_through_pending || response_pending);
}

int Connection::NumPendingStatus() {
    return pending_connection_request_responses_.size();
}

ConnectionRequestResponse *Connection::Dequeue(uint64_t ack_sequence, bool success) {
    MutexLock lock(&mu_);
    if (!pending_connection_request_responses_.empty()) {
        ConnectionRequestResponse *connection_request_response = pending_connection_request_responses_.front();//NOLINT
        if (connection_request_response->command()->header().sequence() <= ack_sequence) {
            pending_connection_request_responses_.pop();
            if (success) {
                connection_request_response->response_command()->
                                            mutable_status()->
                                            set_code(Command_Status_StatusCode_SUCCESS);
                connection_request_response->response_command()->
                                            mutable_status()->
                                            set_statusmessage("Successfully Committed to Disk");
            } else {
                connection_request_response->response_command()->
                                            mutable_status()->
                                            set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                connection_request_response->response_command()->
                                            mutable_status()->
                                            set_statusmessage("Failed to Commit to Disk");
            }
            return connection_request_response;
        }
    }
    return NULL;
}

ConnectionRequestResponse *Connection::Dequeue() {
    MutexLock lock(&mu_);
    ConnectionRequestResponse *connection_request_response = NULL;
    if (!p_request_responses_.empty()) {
        connection_request_response = p_request_responses_.front();
        p_request_responses_.pop();
    }
    cv_.SignalAll();
    return connection_request_response;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
