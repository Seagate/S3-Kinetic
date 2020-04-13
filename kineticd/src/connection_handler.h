#ifndef KINETIC_CONNECTION_HANDLER_H_
#define KINETIC_CONNECTION_HANDLER_H_
#include "kinetic/common.h"
#include "kinetic/message_stream.h"

#include "authenticator_interface.h"
#include "connection_queue.h"
#include "message_processor_interface.h"
#include "profiler.h"
#include "statistics_manager.h"
#include "limits.h"
#include "user_store_interface.h"
#include "status_interface.h"
#include "timer_handler_interface.h"
#include "log_ring_buffer.h"
#include "log_handler_interface.h"
#include "aging_timer.h"
#include "send_pending_status_interface.h"

#include "util/mutexlock.h"
#include <unordered_map>
#include <deque>
#include <sstream>
#include <tuple>
#include <memory>

#include "BatchSetCollection.h"
#include "common/event/Subscriber.h"
#include "common/event/Event.h"

#include "PriorityRequestQueue.h"

using namespace com::seagate::common::event; //NOLINT
using com::seagate::common::event::Event;

enum class ConnectionStatusCode {
    CONNECTION_ERROR,
    CONNECTION_OK,
    CONNECTION_INTERRUPT
};

namespace com {
namespace seagate {
namespace kinetic {

using namespace com::seagate::kinetic::cmd; //NOLINT
using namespace leveldb; //NOLINT
using ::kinetic::MessageStreamInterface;
class Server;
class AgingTimer;

using ::kinetic::MessageStreamFactoryInterface;
using proto::Command;
using proto::Message;
using proto::Command_Status_StatusCode;

class AtomicCounter {
    private:
    port::Mutex mu_;
    int count_;

    public:
    explicit AtomicCounter(int count) : count_(count) { }
    void Increment() {
      IncrementBy(1);
    }
    void IncrementBy(int count) {
      MutexLock l(&mu_);
      count_ += count;
    }
    void Decrement() {
      DecrementBy(1);
    }
    void DecrementBy(int count) {
      MutexLock l(&mu_);
      count_ -= count;
    }

    int Read() {
      MutexLock l(&mu_);
      return count_;
    }
    void Reset() {
      MutexLock l(&mu_);
      count_ = 0;
    }
};

class ConnectionHandler : public LogHandlerInterface, public SendPendingStatusInterface,
    public Subscriber, public TimerHandlerInterface {
    public:
    static BatchSetCollection _batchSetCollection;
    static bool pinOP_in_progress;
    static pthread_mutex_t mtx_pinOP_in_progress;
    static const int kPoisonPillId = -2;
    static AtomicCounter numActiveHandlers;
    static AtomicCounter numBatchesRetained;
    static const int LATENCY_COMMAND_MAX;
    static const int KEY_VALUE_HISTO_MAX;

    ConnectionHandler(
        AuthenticatorInterface &authenticator,
        MessageProcessorInterface &message_processor,
        MessageStreamFactoryInterface &message_stream_factory,
        Profiler &profiler,
        Limits& limits,
        UserStoreInterface& user_store,
        uint64_t connIdBase,
        StatisticsManager& statistics_manager);
    ~ConnectionHandler();
    void ConnectionThreadWorker(int signal_fd);
    void ResponseThread();
    bool IngestBatchCmds(std::shared_ptr<Connection> connection, Command* command,
                         ConnectionRequestResponse *connection_request_response,
                         char* valBuf, uint32_t valSize);
    void CommandIngestThread();
    void Enqueue(std::shared_ptr<Connection> connection);
    std::shared_ptr<Connection> Dequeue();
    void SendSignOnMessage(std::shared_ptr<Connection> connection);
    void SendPoisonPills(size_t number_worker_threads, size_t number_response_threads, size_t number_cmd_ingest_threads);
    void CloseConnection(int connFd, bool bWaitUntilNoResponse = true);
    void TooManyConnections(Connection* connection);
    void SetServer(StatusInterface* server) {
        server_ = (Server*)server;
    }
    ConnectionStatusCode ValidateCommand(Connection* connection, ConnectionRequestResponse *connection_request_response);
    ConnectionStatusCode ValidateMessage(Connection* connection, ConnectionRequestResponse *connection_request_response);
    void LogStaleEntry(int level);
    void testprint();
    void LogLatency(unsigned char flag);
    void SendAllPending(bool success, std::unordered_map<int64_t, uint64_t> token_list);

    void RemoveAllFromPendingMap();
    void HaltTimer();
    void ServiceTimer(bool toSST);
    void SetSelectFD(int select_fd) {
        LOG(INFO) << "========= SetSelectFD " << select_fd;
        select_fd_ = select_fd;
    }
    void inform(com::seagate::common::event::Event* event);
    ConnectionStatusCode SendUnsolicitedStatus(Connection *connection, ConnectionRequestResponse* requestResponse,
                                          bool fromResponseThread = false);
    string currentState();

    private:
    // Threadsafe; AuthenticatorInterface implementations must be threadsafe
    AuthenticatorInterface& authenticator_;
    ConnectionStatusCode HandleResponse(Connection *connection,
        NullableOutgoingValue *response_value, uint64_t ack_sequence, bool validAckSeq, bool success);
    void HandleConnectionStatus(std::shared_ptr<Connection> connection,
        ConnectionStatusCode status,
        int select_fd, bool bWaitUntilNoResponse = true);
    void FlushTcpCork(int fd);
    bool SetRecordStatus(const std::string& key);
    const std::string GetKey(const std::string& key, bool next);
    void SetResponseTypeAndAckSequence(const Command* command, Command *command_response);
    void SetHMACError(Command* response_command);

    ConnectionStatusCode SerializeAndSendMessage(
            Connection *connection,
            NullableOutgoingValue* message_value,
            ConnectionRequestResponse *connection_request_response);
    //TODO(Gonzalo): Better way of doing this, it makes more sense keeping this as a private method
    // ConnectionStatusCode SendUnsolicitedStatus(
    //         Connection *connection, ConnectionRequestResponse *connection_request_response);
    void InitCommandPriority(Connection *connection, ConnectionRequestResponse *connection_request_response);
    void InitCommandTimers(ConnectionRequestResponse *connection_request_response);
    void InitConnectionPriorityAndTime(Connection *connection, ConnectionRequestResponse *connection_request_response);
    void ReEnqueueConnection(std::shared_ptr<Connection> connection);
    ConnectionStatusCode ExecuteOperation(std::shared_ptr<Connection> connection, ConnectionRequestResponse *connection_request_response);
    void AddToPendingMap(std::shared_ptr<Connection> connection);
    bool RemoveFromPendingMap(int64_t key);
    void SendUnsupportableResponse(std::shared_ptr<Connection> connection, int select_fd, ConnectionRequestResponse *connection_request_response);
    void KickOffDownload(Connection *connection);
    MessageProcessorInterface &message_processor_;
    MessageStreamFactoryInterface &message_stream_factory_;
    Profiler &profiler_;
    ConnectionQueue connection_queue_;
    ConnectionResponseQueue response_connection_queue_;
    Limits& limits_;
    UserStoreInterface& user_store_;
    Server* server_;
    int num_latency_commands_;
    int num_pending_status_;
    std::string key_;
    std::string prev_key_;
    std::unordered_map<int64_t, std::shared_ptr<Connection>> pending_status_map_;
    StatisticsManager& statistics_manager_;
    uint32_t number_of_retained_;
    int select_fd_;
    PriorityRequestQueue requestQueue_;

    port::Mutex mu_;
    port::CondVar cv_;
    uint64_t connIdBase_;
    port::Mutex mu_ingest_;
    port::CondVar cv_ingest_;
    std::deque<std::shared_ptr<Connection>> connection_for_ingest_;
    AgingTimer aging_timer_;
    bool ingestingData_;

    DISALLOW_COPY_AND_ASSIGN(ConnectionHandler);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_CONNECTION_HANDLER_H_
