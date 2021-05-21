#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <limits.h>
#include <chrono>
#include <vector>

#include "connection_handler.h"
#include "request_context.h"
#include "server.h"
#include "command_line_flags.h"
#include <signal.h>
#include "glog/logging.h"
#include "kinetic.pb.h"
#include "aging_timer.h"
#include "mem/DynamicMemory.h"
#include "mem/KineticMemory.h" // TODO(Gonzalo) Maybe able to remove this
#include "BatchSet.h"
#include "smrdisk/Disk.h"

#include <vector>
#include "kernel_mem_mgr.h"
#include "util/env_posix.h"
#include <pthread.h>
#include <sys/syscall.h>
#include "stack_trace.h"
#include "leveldb/status.h"

using namespace leveldb; //NOLINT

using namespace com::seagate::kinetic::proto;//NOLINT
using namespace std::chrono;//NOLINT
using ::kinetic::MessageStream;

namespace com {
namespace seagate {
namespace kinetic {
using ::kinetic::MessageStreamInterface;

AtomicCounter ConnectionHandler::numActiveHandlers(1);
AtomicCounter ConnectionHandler::numBatchesRetained(0);

const int ConnectionHandler::LATENCY_COMMAND_MAX = 100;
const int ConnectionHandler::KEY_VALUE_HISTO_MAX = 100;

bool ConnectionHandler::pinOP_in_progress = false;
pthread_mutex_t ConnectionHandler::mtx_pinOP_in_progress = PTHREAD_MUTEX_INITIALIZER;

BatchSetCollection ConnectionHandler::_batchSetCollection;

ConnectionHandler::ConnectionHandler(
        AuthenticatorInterface& authenticator,
        MessageProcessorInterface &message_processor,
        MessageStreamFactoryInterface &message_stream_factory,
        Profiler &profiler,
        Limits& limits,
        UserStoreInterface& user_store,
        uint64_t connIdBase,
        StatisticsManager& statistics_manager)
    : authenticator_(authenticator),
    message_processor_(message_processor),
    message_stream_factory_(message_stream_factory),
    profiler_(profiler),
    limits_(limits),
    user_store_(user_store),
    statistics_manager_(statistics_manager),
    cv_(&mu_),
    cv_ingest_(&mu_ingest_),
    aging_timer_(AgingTimer(this)) {
        num_latency_commands_ = 0;
        num_pending_status_ = 0;
        connIdBase_ = (connIdBase << 32);
        ingestingData_ = false;
        aging_timer_.StartTimerThread();
    }

ConnectionHandler::~ConnectionHandler() {
    aging_timer_.StopTimerThread();
}

void ConnectionHandler::CloseConnection(int connFd, bool bWaitUntilNoResponse) {
    MutexLock l(&mu_);
    // and delete it from the connection map
    std::shared_ptr<Connection> connection =  Server::connection_map.GetConnection(connFd);
    if (connection == nullptr) {
        cv_.SignalAll();
        return;
    }
    connection->SetState(ConnectionState::CLOSED);
    // Remove connection from the map to stop accepting commmands and processing un-processed command
    Server::connection_map.RemoveConnection(connection->fd());
    server_->RemoveFromEpoll(connFd);
    statistics_manager_.DecrementOpenConnections();
    VLOG(1) << "Current number of Connections " << Server::connection_map.TotalConnectionCount();
    if (bWaitUntilNoResponse) {
        // if pendding_status_map_ has connFd, do flush to send all pending responses to response queue
        if (pending_status_map_.find(connection->id()) != pending_status_map_.end()) {
            LOG(INFO) << "Flush to move pending status to response queue";
            mu_.Unlock();
            message_processor_.Flush();
            mu_.Lock();
        }
    }
    VLOG(1) << __func__ << ": Calling wait till idle, fd " << connFd;
    mu_.Unlock();
    connection->wait_till_idle();
    mu_.Lock();
    VLOG(1) << __func__ << ": Return from wait till idle, fd " << connFd;
    // Clear up space taken in latency map by the connection that has been closed
    connection_queue_.RemoveConnectionLatency(connection->id());
    // If connection has pending statuses add to num_pending_status_, so we can clean up later
    num_pending_status_ = num_pending_status_ + connection->NumPendingStatus();
    numBatchesRetained.DecrementBy(connection->NumPendingStatus());
    // Remove from pending_status_map
    RemoveFromPendingMap(connection->id());
    vector<BatchSet*>* batchSets = _batchSetCollection.getAllBatchSetOnConnection(connection->id());
    vector<BatchSet*>::iterator it = batchSets->begin();
    for (; it != batchSets->end(); ++it) {
        TimeoutEvent event = TimeoutEvent(*it);
        EventService::getInstance()->unsubscribe(&event, this);
    }
    delete batchSets;
    _batchSetCollection.deleteBatchesOnConnection(connection->id(), Server::_shuttingDown);
    cv_.SignalAll();
    if (server_->GetStateEnum() == StateEnum::DOWNLOAD) {
        Server::_shuttingDown = true;
        mu_.Unlock();
        KickOffDownload(connection.get());
        mu_.Lock();
    }
    ::close(connFd);
    connection.get()->fd(-1);
}

void ConnectionHandler::TooManyConnections(Connection* connection) {
    ConnectionRequestResponse connection_request_response;
    connection_request_response.SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST, "Too Many Connections");
    LOG(INFO) << "Too many Connections";
    SendUnsolicitedStatus(connection, &connection_request_response);
}

//////////////////////////////////////
//Dequeues Valid Connection, evaluates connection Properties
//and hands off the work to the necessary receiver via ExecuteOperation()
//Control returns to this function near operation completion
void ConnectionHandler::ConnectionThreadWorker(int select_fd) {
    pid_t tid;
    tid = syscall(SYS_gettid);
    cout << " IN THREAD ID " << tid << endl;
    // DO NOT DELETE THIS IS FOR MOBILER
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    for (int i = 0; i < 2; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            cout << "CPU1: IN CPU " << i << endl;
        }
    }
#ifndef PRODUCT_X86
    signal(SIGSEGV, segFaultHandler);
#endif
    // 1. Dequeue connection
    while (true) {
        VLOG(2) << "ConnectionThreadWorker(): " << currentState();
        // Are there commands pending?
        if (!requestQueue_.hasRequest()) {
            // No work to do - are connections pending (which will provide new commands)
            if (connection_for_ingest_.empty()) {
                // Nothing waiting to be added to requestQueue--Are we ingesting a connection at the moment
                if (!ingestingData_) {
                    // Not ingesting a command - Truly Idle from an interface command perspective - Check for pending statuses
                    if (pending_status_map_.size() > 0) {
                        // Idle and Statuses are pending - Fire timer immediately
                        aging_timer_.ArmTimer(0);
                    }
                }
            }
        }
        ConnectionRequestResponse* connection_request_response = requestQueue_.dequeue();
        std::shared_ptr<Connection> connection = connection_request_response->GetConnection();
        //  1.B check for Poison Pills
        if (connection == NULL) {
            delete connection_request_response;
            continue;
        }
        if (connection->fd() == ConnectionHandler::kPoisonPillId) {
            VLOG(1) << "Worker thread received poison pill: " << currentState();
            delete connection_request_response;
            std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection_pair(connection, ULLONG_MAX, false);
            response_connection_queue_.Enqueue(connection_pair);
            return;
        }
        connection->set_cmd_in_progress(true);
        VLOG(4) << " DEQUE SEQ " << connection_request_response->command()->header().sequence();
        bool ssl_required_command = (connection_request_response->
                    request()->authtype() == Message_AuthType_PINAUTH ||
            connection_request_response->
                    command()->header().messagetype() == Command_MessageType_SECURITY);

        if (!ssl_required_command) {
            CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
            ConnectionQueue::thread_workers_idle--;
            CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));
        }

        // Check if request is supportable
        std::string stateName;
        if (!server_->IsSupportable(connection_request_response->request()->authtype(),
                                    connection_request_response->command()->header().messagetype(),
                                    stateName)) {
            IncomingValueInterface* value = connection_request_response->
                                                        request_value();
            if (value) {
                if (value->GetUserValue()) {
                    smr::DynamicMemory::getInstance()->deallocate(value->size());
                }
                value->FreeUserValue();
            }

            LOG(INFO) << "Received Request while in " << stateName;
            connection_request_response->
                        response_command()->mutable_status()->
                        set_statusmessage(string("Drive is in ") + stateName);

            // if HMAC is supported, send a response and keep the connection open
            if (server_->IsHmacSupportable()) {
                if (server_->GetStateEnum() == StateEnum::HIBERNATE) {
                    connection_request_response->
                                response_command()->mutable_status()->
                                set_code(Command_Status_StatusCode_HIBERNATE);
                } else {
                    connection_request_response->
                                response_command()->mutable_status()->
                                set_code(Command_Status_StatusCode_INTERNAL_ERROR);
                }
                connection->set_cmd_in_progress(false);
                SendUnsupportableResponse(connection, select_fd, connection_request_response);
                // Release increment thread_workers_idle and notify thread condition
                if (!ssl_required_command) {
                    CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
                    ConnectionQueue::thread_workers_idle++;
                    CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));
                }
                continue;
            }

            if (server_->GetStateEnum() == StateEnum::LOCKED) {
                connection_request_response->
                            response_command()->mutable_status()->
                set_code(Command_Status_StatusCode_DEVICE_LOCKED);
            } else {
                if (ConnectionHandler::_batchSetCollection.isBatch(connection_request_response->command())) {
                    connection_request_response->
                        response_command()->mutable_status()->
                       set_code(Command_Status_StatusCode_INVALID_BATCH);
                } else {
                    connection_request_response->
                        response_command()->mutable_status()->
                        set_code(Command_Status_StatusCode_INVALID_REQUEST);
                }
            }

            connection->set_cmd_in_progress(false);
            // Release increment thread_workers_idle and notify thread condition
            CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
            ConnectionQueue::thread_workers_idle++;
            CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));
            int connFd = connection->fd();
            SendUnsolicitedStatus(connection.get(), connection_request_response);
            CloseConnection(connFd);
            delete connection_request_response;
            continue;
        }

        //If a Media Scan is attempting to resume and a PinOp has been issued,
        //Stop the Scan, return Status and Close Connection.
        //@param early_response_value - created in case a response is needed immediately
        NullableOutgoingValue early_response_value;
        if (connection_request_response->command()->header().messagetype() == Command_MessageType_MEDIASCAN &&
            pinOP_in_progress) {
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_EXPIRED,
                                           "Scan Interrupted by Pin Operation");
            connection->set_cmd_in_progress(false);
            connection->AddToResponsePending(connection_request_response);
            HandleResponse(connection.get(), &early_response_value, ULLONG_MAX, false, false);
            ConnectionStatusCode earlyStatus = ConnectionStatusCode::CONNECTION_ERROR;
            HandleConnectionStatus(connection, earlyStatus, select_fd);
            // Release increment thread_workers_idle and notify thread condition
            if (!ssl_required_command) {
                CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
                ConnectionQueue::thread_workers_idle++;
               CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));
            }
            return;
        }

        //Set the time connection was dequeued for measuring time until response
        //ONLY if it was NOT interrupted previously
        //(Only measuring Response from first dequeue to final response, not every dequeue)
        if (!connection_request_response->Interrupted()) {
            connection_request_response->SetTimeDequeued();
            if (connection_request_response->
                            ExceededTimeInQueue()) { //measure time spent in queue
                connection_request_response->SetMaxTimeInQueue();
                LOG(INFO) << "MAX TIME IN QUEUE SO FAR " <<
                    connection_request_response->MaxTimeInQueue().count()
                        << " ms" << " for connection " << connection->fd();
                stringstream ss;
                ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << endl;
                ss << endl << "MAX QUEUE  " << connection_request_response->MaxTimeInQueue().count()  << endl;
                leveldb::Status::IOError(ss.str());
                #ifdef KDEBUG
                connection_queue_.LogLatency(LATENCY_EVENT_LOG_UPDATE);
                LogRingBuffer::Instance()->makePersistent();
                #endif
            }
        } else {
            //if previous interrupt, clear bool value
            //in order to accomodate future interrupt's
            connection_request_response->ResetInterrupt();
        }

        // 2. Create Necessary Variables and Start Profiler
        Event processing;
        profiler_.Begin(kMessageProcessing, &processing);
        NullableOutgoingValue response_value;

        statistics_manager_.IncrementOperationCount(
            connection_request_response->command()->header().messagetype());
        if (connection_request_response->request_value()) {
            statistics_manager_.IncrementByteCount(
                connection_request_response->
                            command()->header().messagetype(),
                connection_request_response->request_value()->size());
        }

        // 3. Setup Response Message
        SetResponseTypeAndAckSequence(connection_request_response->command(),
                                      connection_request_response->
                                                  response_command());

        // 4. Check for TimeOut Prior to carrying out any further work
        if (connection_request_response->TimedOut()) {
            profiler_.End(processing);
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_EXPIRED, "Timeout Exceeded");
            connection->set_cmd_in_progress(false);
            IncomingValueInterface* value = connection_request_response->request_value();
            if (value) {
                if (value->GetUserValue()) {
                    smr::DynamicMemory::getInstance()->deallocate(value->size());
                }
                value->FreeUserValue();
            }
            connection->AddToResponsePending(connection_request_response);
            std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection_pair(connection, ULLONG_MAX, false);
            response_connection_queue_.Enqueue(connection_pair);
        } else {
            // 5. Carry out the Operation
            if (!ssl_required_command) {
                CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
                ConnectionQueue::thread_workers_idle++;
                CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));
            }
            ExecuteOperation(connection, connection_request_response);
            profiler_.End(processing);
            connection->set_cmd_in_progress(false);
            continue;
            // 6. On Execute Completion, Determine connection's state
            // Handled in ResponseThread method
        }
        // Release increment thread_workers_idle and notify thread condition
        if (!ssl_required_command) {
            CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
            ConnectionQueue::thread_workers_idle++;
            CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));
        }
    }//END while loop
}

void ConnectionHandler::ResponseThread() {
    pid_t tid;
    tid = syscall(SYS_gettid);
    cout << " OUT THREAD ID " << tid << endl;
    // DO NOT DELETE THIS IS FOR MOBILER
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    for (int i = 0; i < 2; i++) {
        if (CPU_ISSET(i, &cpuset))
            cout << "CPU1: OUT CPU " << i << endl;
    }
#ifndef PRODUCT_X86
    signal(SIGSEGV, segFaultHandler);
#endif
    bool bWaitUntilNoResponse = false;
    while (true) {
        std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection_pair = response_connection_queue_.Dequeue();
        if (std::get<0>(connection_pair)->fd() == ConnectionHandler::kPoisonPillId) {
            VLOG(1) << "Response thread received poison pill: " << currentState();
            return;
        }
        ConnectionStatusCode status = HandleResponse(std::get<0>(connection_pair).get(), NULL,
                                                     std::get<1>(connection_pair), std::get<2>(connection_pair),
                                                     std::get<0>(connection_pair)->status_for_response());

        HandleConnectionStatus(std::get<0>(connection_pair), status, -1, bWaitUntilNoResponse);
    }
}

void ConnectionHandler::ServiceTimer(bool toSST) {
    message_processor_.Flush(toSST);
}

//////////////////////////////////////
///Determine whether to close, keep active or re-enqueue the connection
void ConnectionHandler::HandleConnectionStatus(
    std::shared_ptr<Connection> connection,
    ConnectionStatusCode status,
    int select_fd, bool bWaitUntilNoResponse) {
    //int connFd = -1;
    switch (status) {
        case ConnectionStatusCode::CONNECTION_ERROR:
            LOG(INFO) << "Finished handling connection due to connection error "
                      << connection->fd();
            if (connection->state() != ConnectionState::CLOSED) {
                //connection->SetState(ConnectionState::SHOULD_BE_CLOSED);
                //connFd = connection->fd();
                //CloseConnection(connFd, bWaitUntilNoResponse);
            }
            return;
        case ConnectionStatusCode::CONNECTION_OK:
            //if (connection->state() == ConnectionState::SHOULD_BE_CLOSED) {
            //    TooManyCloseConnection(connection.get(), bWaitUntilNoResponse);
           // }
            return;

        case ConnectionStatusCode::CONNECTION_INTERRUPT:
            // status of CONNECTION_INTERRUPT never hapens.
            // Reason: CONNECTION_INTERRUPT is returned by only ConnectionHandler::ExecuteOperation()
            // but its return status is not captured for use.
            // We need to investigate because CONNECTION_INTERRUPT is used by media scan.
            ReEnqueueConnection(connection);
            return;
    }
}

void ConnectionHandler::SendSignOnMessage(std::shared_ptr<Connection> connection) {
    MessageStreamInterface *message_stream;
    ConnectionRequestResponse connection_request_response;
    if (!message_stream_factory_.NewMessageStream(connection->fd(),
            connection->use_ssl(),
            connection->ssl(),
            limits_.max_message_size(),
            &(message_stream))) {
        CloseConnection(connection->fd());
        return;
    }

    connection->SetMessageStream(message_stream);

    // Send unsolicited status for sign on
    connection_request_response.SetResponseCommand(Command_Status_StatusCode_SUCCESS, "");
    // Generate & set connection ID
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t nanos = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    uint64_t id =  connIdBase_ | (nanos & 0xFFFFFFFF);
    connection->SetId(id);
    connection_request_response.SetResponseCommand(connection->id());
    if (server_->IsClusterSupportable()) {
        message_processor_.SetClusterVersion(connection_request_response.response_command());
    }
    // GetLog for Configuration, Limits
    message_processor_.SetLogInfo(connection_request_response.response_command(),
                                  NULL,
                                  "",
                                  Command_GetLog_Type_LIMITS);
    message_processor_.SetLogInfo(connection_request_response.response_command(),
                                  NULL,
                                  "",
                                  Command_GetLog_Type_CONFIGURATION);

    int connFd = connection->fd();
    if (SendUnsolicitedStatus(connection.get(), &connection_request_response) == ConnectionStatusCode::CONNECTION_ERROR) {
        CloseConnection(connFd);
    }
}

ConnectionStatusCode ConnectionHandler::ValidateMessage(Connection* connection, ConnectionRequestResponse *connection_request_response) {
    // Incoming message lacked authtype
    if (!connection_request_response->request()->has_authtype()) {
        LOG(INFO) << "Received message lacked auth type";//NO_SPELL
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Received message lacked auth type");
        return ConnectionStatusCode::CONNECTION_ERROR;
    }
    if (connection_request_response->request()->authtype() == Message_AuthType_PINAUTH) {
        // Pin Auth Command
        if (!connection->use_ssl()) {
            // Pin Auth Commands Require TLS Port
            LOG(INFO) << "Recieved a PinAuth command on a non-ssl connection";//NO_SPELL
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "PINAUTH commands requre an ssl connection");
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
    } else if (connection_request_response->request()->authtype() == Message_AuthType_HMACAUTH) {
        if (!server_->IsHmacSupportable()) {
            Command_Status_StatusCode status_code;
            std::string stateName = server_->GetStateName();

            if (server_->GetStateEnum() == StateEnum::LOCKED) {
                status_code = Command_Status_StatusCode_DEVICE_LOCKED;
            } else {
                status_code = Command_Status_StatusCode_INVALID_REQUEST;
            }

            LOG(INFO) << "Received Request while in " << stateName;
            connection_request_response->
                        SetResponseCommand(status_code,
                                           std::string("Drive is in ") + stateName);
            return ConnectionStatusCode::CONNECTION_ERROR;
        }

        // Check HMAC missing&it.first->second
        if ((!connection_request_response->request()->has_hmacauth()) ||
                (!connection_request_response->
                              request()->hmacauth().has_hmac()) ||
                (!connection_request_response->
                              request()->hmacauth().has_identity()) ) {
            SetHMACError(connection_request_response->response_command());
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
        // Check Authentication
        Event authentication;
        profiler_.Begin(kAuthentication, &authentication);
        AuthenticationStatus authentication_status =
                authenticator_.Authenticate(*(connection_request_response->request()));
        profiler_.End(authentication);
        if (connection_request_response->request_value() == NULL) {
            LOG(INFO) << "REQUEST VALUE == NULL";//NO_SPELL
            Command_Status_StatusCode status_code = Command_Status_StatusCode_INVALID_REQUEST;
            connection_request_response->
                        SetResponseCommand(status_code,
                                           std::string("Value data may be corrupted"));
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
        if (authentication_status != kSuccess) {
             // Failed authentication
            SetHMACError(connection_request_response->response_command());
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
    } else {
        LOG(INFO) << "Unknown Auth Type";//NO_SPELL
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Received message with unknown auth type");
        return ConnectionStatusCode::CONNECTION_ERROR;
    }
    return ConnectionStatusCode::CONNECTION_OK;
}

ConnectionStatusCode ConnectionHandler::ValidateCommand(Connection* connection, ConnectionRequestResponse *connection_request_response) {
    if (!connection_request_response->command()->header().has_messagetype()) {
        // Command requires message type
        LOG(INFO) << "Command is missing type";
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Received command is missing type");
        return ConnectionStatusCode::CONNECTION_ERROR;
    }

    if ((connection_request_response->
                     command()->header().messagetype() == Command_MessageType_SECURITY) &&
            (!connection->use_ssl())) {
        // Security Commands Require TLS Port
        LOG(INFO) << "Security request on non-TLS port";//NO_SPELL
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Security commands require TLS port");
        return ConnectionStatusCode::CONNECTION_ERROR;
    }

    if (!connection_request_response->
                     command()->header().has_connectionid()) {
        // Missing connection ID
        LOG(INFO) << "Missing Connection ID";
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Missing connection ID");
        return ConnectionStatusCode::CONNECTION_ERROR;
    } else if (connection_request_response->
                           command()->header().connectionid() != connection->id()) {
        // Bad connection ID
        LOG(INFO) << "Invalid Connection ID";
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Invalid connection ID");
        return ConnectionStatusCode::CONNECTION_ERROR;
    }

    // If this is not the first command received on this connection, check that
    // the Seq ID is strictly increasing
    if (connection_request_response->
                    command()->header().sequence() <= connection->lastMsgSeq() &&
            connection->lastMsgSeqHasBeenSet()) {
        LOG(INFO) << "Invalid sequence ID, conn seq = " <<
                connection_request_response->command()->header().sequence() <<
                ", last seq = " <<  connection->lastMsgSeq();
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Invalid sequence ID");
        return ConnectionStatusCode::CONNECTION_ERROR;
    } else {
        connection->SetLastMsgSeq(connection_request_response->
                                              command()->header().sequence());
    }

    // Verify value size limit
    if ((connection_request_response->
                     command()->body().setup().setupoptype() != Command_Setup_SetupOpType_FIRMWARE_SETUPOP) &&
            connection_request_response->request_value() &&
            connection_request_response->
                        request_value()->size() > limits_.max_value_size()) {
        LOG(INFO) << "Received value that was too large";
        connection_request_response->
                    SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                       "Received value that was too large");
        return ConnectionStatusCode::CONNECTION_ERROR;
    }
    if (connection_request_response->command()->body().has_keyvalue()) {
        // If Put() or Delete(), Command requires Valid Synchronization Field
        // Must be one of the following: WRITEBACK, WRITETHROUGH, FLUSH
        bool enforce_synch = ((connection_request_response->command()->header().messagetype() == Command_MessageType_PUT) || //NOLINT
            (connection_request_response->
                         command()->header().messagetype() == Command_MessageType_DELETE));
        bool cmd_has_synch = connection_request_response->
                                         command()->body().keyvalue().has_synchronization();
        // if cmd requires synch && does not provide field: reject
        // if required field is present but is invalid: reject
        if ((enforce_synch && !cmd_has_synch) ||
            ((enforce_synch && cmd_has_synch) &&
             connection_request_response->
                         command()->body().keyvalue().synchronization() ==
                proto::Command_Synchronization_INVALID_SYNCHRONIZATION)) {
                    connection_request_response->
                                SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                                   "Synchronization Field Invalid / Not Provided");
                    return ConnectionStatusCode::CONNECTION_ERROR;
        }
        #ifdef QOS_ENABLED
        // if cmd requires synch check qos, if not provided or default map to constant
        if (enforce_synch && ((connection_request_response->
                                command()->body().keyvalue().prioritizedqos_size() == 0) ||
                             (connection_request_response->
                                command()->body().keyvalue().prioritizedqos(0) ==
                                Command_QoS_DEVICE_DEFAULT))) {
            // Set qos to DEFAULT_QOS, which is defined in product_flags
            Command_QoS qos = Command_QoS(DEFAULT_QOS);
            if (connection_request_response->
                                command()->body().keyvalue().prioritizedqos_size() == 0) {
                connection_request_response->
                            command()->mutable_body()->mutable_keyvalue()->add_prioritizedqos(qos);
            } else {
                connection_request_response->
                            command()->mutable_body()->
                            mutable_keyvalue()->set_prioritizedqos(0, qos);
            }
        }
        #endif
        // Verify version length, if provided
        if (connection_request_response->
                        command()->body().keyvalue().has_dbversion() &&
            (connection_request_response->
                         command()->body().keyvalue().dbversion().size()
                        > limits_.max_version_size()) ) {
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "Version too long");
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
        // Verify new version length, if provided
        if (connection_request_response->
                        command()->body().keyvalue().has_newversion() &&
            (connection_request_response->
                         command()->body().keyvalue().newversion().size()
                        > limits_.max_version_size()) ) {
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "New version too long");
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
        // Verify key size, if provided
        if (connection_request_response->
                        command()->body().keyvalue().has_key() &&
                (connection_request_response->
                             command()->body().keyvalue().key().size()
                        > limits_.max_key_size()) ) {
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "Key too long");
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
        // Verify Tag Size
        if (connection_request_response->
                        command()->body().keyvalue().has_tag() &&
                connection_request_response->
                            command()->body().keyvalue().tag().size() > limits_.max_tag_size()) {
            connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "Tag too long");
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
    }

    if (connection_request_response->request()->authtype() == Message_AuthType_HMACAUTH) {
        int cmdPriority = connection_request_response->command()->header().priority();
        int64_t userId = connection_request_response->request()->hmacauth().identity();
        User user;
        if (user_store_.Get(userId, &user)) {
            if (cmdPriority > user.maxPriority()) {
                connection_request_response->SetResponseCommand(Command_Status_StatusCode_NOT_AUTHORIZED,
                                               "Command priority is higher than permitted");
                return ConnectionStatusCode::CONNECTION_ERROR;
            }
        } else {
            connection_request_response->SetResponseCommand(Command_Status_StatusCode_NOT_AUTHORIZED,
                                          "Unauthorized user");
            return ConnectionStatusCode::CONNECTION_ERROR;
        }
    }

    return ConnectionStatusCode::CONNECTION_OK;
}

void ConnectionHandler::LogLatency(unsigned char flag) {
    connection_queue_.LogLatency(flag);
}

void ConnectionHandler::LogStaleEntry(int level) {
    connection_queue_.LogStaleEntry(level);
}

void ConnectionHandler::Enqueue(std::shared_ptr<Connection> connection) {
    MutexLock l(&mu_ingest_);
    connection->setEnqueuedTime(std::chrono::high_resolution_clock::now());
    connection_for_ingest_.push_back(connection);
    cv_ingest_.SignalAll();
}

std::shared_ptr<Connection> ConnectionHandler::Dequeue() {
    MutexLock l(&mu_ingest_);
    while (connection_for_ingest_.size() == 0) {
        cv_ingest_.Wait();
    }
    std::shared_ptr<Connection> connection = connection_for_ingest_.front();
    connection_for_ingest_.pop_front();
    return connection;
}

bool ConnectionHandler::IngestBatchCmds(std::shared_ptr<Connection> connection, Command* command,
                                       ConnectionRequestResponse *connection_request_response,
                                       char* valBuf, uint32_t valSize) {
    if (_batchSetCollection.isStartBatchCommand(command)) {
        BatchSet* batchSet = NULL;
        batchSet = _batchSetCollection.getBatchSet(command->header().batchid(), connection->id());
        if (batchSet) {
           LOG(ERROR) << "Batch ID was in use";
           connection_request_response->SetResponseCommand(Command_Status_StatusCode_INVALID_BATCH,
                                                           "Batch ID was in use");
           return false;
        } else {
           BatchSet* batchSet = NULL;
           batchSet = _batchSetCollection.createBatchSet(command, connection,
                                                        connection_request_response->EnqueuedTime(),
                                                        *(connection_request_response->response_command()));

           if (!batchSet) {
               // create batch set already set bad status and message.
               return false;
           } else {
               connection_request_response->command()->mutable_header()->set_priority(
                                (com::seagate::kinetic::proto::Command_Priority)batchSet->getCmdPriority());
               connection_request_response->command()->mutable_header()->set_timeout(batchSet->getTimeOut());
               connection_request_response->command()->mutable_header()->set_clusterversion(batchSet->getClusterVersion());
               connection_request_response->command()->mutable_header()->set_earlyexit(batchSet->getEarlyExit());
               connection_request_response->command()->mutable_header()->set_timequanta(batchSet->getTimeQuanta());
           }
        }
    } else {
        //not a START BATCH make sure priority and others are the same as START BATCH
        //has to do this to preserve priority in the request Q
        BatchSet* batchSet = NULL;
        batchSet = _batchSetCollection.getBatchSet(command->header().batchid(), connection->id());
        if (batchSet) {
            connection_request_response->command()->mutable_header()->set_priority(
                                           (com::seagate::kinetic::proto::Command_Priority)batchSet->getCmdPriority());
            connection_request_response->command()->mutable_header()->set_timeout(batchSet->getTimeOut());
            connection_request_response->command()->mutable_header()->set_clusterversion(batchSet->getClusterVersion());
            connection_request_response->command()->mutable_header()->set_earlyexit(batchSet->getEarlyExit());
            connection_request_response->command()->mutable_header()->set_timequanta(batchSet->getTimeQuanta());
        } else {
            connection_request_response->SetResponseCommand(Command_Status_StatusCode_INVALID_BATCH,
                                                            "Batch command without a batch");
            return false;
        }
    }
    MessageStreamInterface::MessageStreamReadStatus status;
    if (!_batchSetCollection.isPutBatchCommand(command) && valSize == 0) {
        status = connection->ReadValue(valSize, connection_request_response);
    } else {
        valBuf = DynamicMemory::getInstance()->allocate(valSize);
        if (valBuf) {
            status = connection->ReadValue(valBuf, valSize, connection_request_response);
        } else {
            LOG(ERROR) << "Failed to allocate memory";
            connection_request_response->SetResponseCommand(Command_Status_StatusCode_INTERNAL_ERROR,
                                                           "Cannot allocate memory for batched command");
            return false;
        }
    }
    if (status != MessageStreamInterface::MessageStreamReadStatus_SUCCESS) {
        free(valBuf);
        smr::DynamicMemory::getInstance()->deallocate(valSize);
        connection_request_response->SetResponseCommand(Command_Status_StatusCode_INTERNAL_ERROR,
                                                        "Unable to read value");
        return false;
    }
    return true;
}

void ConnectionHandler::CommandIngestThread() {
    //  ------Deserialize Message--------
#ifdef RESPONSE
    struct timeval tv;
    uint64_t start, end;
    uint64_t max_cmd_read = 0;
#endif

    while (true) {
        ingestingData_ = false;
        std::shared_ptr<Connection> connection = Dequeue();
        ingestingData_ = true;
        if (connection->fd() == ConnectionHandler::kPoisonPillId) {
            VLOG(1) << "Command ingest thread received poison pill: " << currentState();
            ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
            connection_request_response->setPriority(com::seagate::kinetic::proto::Command::LOWEST);
            connection_request_response->SetTimeout(ConnectionRequestResponse::DEFAULT_CMD_TIMEOUT);
            connection_request_response->SetConnection(connection);
            connection_queue_.clearLatency(connection.get()->id());
            VLOG(1) << ": Enqueueing poision pill to request queue";
            requestQueue_.enqueue(connection_request_response);
            VLOG(1) << ": Enqueued poision pill to request queue: " << currentState();
            return;
        }
    //assume non batch cmd
        bool bBatchCmd = false;
#ifdef RESPONSE
            gettimeofday(&tv, NULL);
            start = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif
        do {
            char* valBuf = NULL;
            connection->UpdateMostRecentAccessTime();
            ConnectionRequestResponse *connection_request_response = new ConnectionRequestResponse();
            connection_request_response->SetTimeQueued(connection->getEnqueuedTime());
            Event deserialization;
            profiler_.Begin(kMessageDeserialization, &deserialization);
    // If we get here then the signal pipe wasn't written to. Since there's only
    // one other socket in the fd set that means there must be data available to read.
            uint32_t msgSize = 0;
            uint32_t valSize = 0;
            int err = 0;
            MessageStreamInterface::MessageStreamReadStatus status =
            connection->ReadHeader(msgSize, valSize, &err);

            if (status != MessageStreamInterface::MessageStreamReadStatus_SUCCESS) {
                LOG(INFO) << "READ HEADER ERROR " << err << " " << errno << " " << strerror(errno);
                if (err == 0xFF) {
                    connection_request_response->
                            SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                               "Error on reading data from socket");
                    int connFd = connection->fd();
                    CloseConnection(connFd);
                }
                delete connection_request_response;
                break;
            }
            Command* command = connection_request_response->command();
            if (smr::Disk::isNoSpace()) {
                connection_request_response->SetResponseCommand(Command_Status_StatusCode_NO_SPACE, "Drive is full");
                status = MessageStreamInterface::MessageStreamReadStatus_SUCCESS;
            }

            if (status == MessageStreamInterface::MessageStreamReadStatus_SUCCESS) {
                status = connection->ReadMessageAPI(msgSize, connection_request_response);

                if (status == MessageStreamInterface::MessageStreamReadStatus_SUCCESS) {
                    if (connection_request_response->request()->authtype() == Message_AuthType_HMACAUTH) {
                        User user;
                        if (user_store_.Get(connection_request_response->request()->
                                                                           hmacauth().identity(), &user)) {
                            connection_request_response->user(user);
                        }
                    }

                    //    ----- Deserialize Command---------
                    // Deserialize Command
                    if (!connection_request_response->command()->
                                ParseFromString(connection_request_response->
                                       request()->commandbytes())) {
                        LOG(ERROR) << "Failed to deserialize command";
                        connection_request_response->
                                SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                               "Unable to process proto message");
                        int connFd = connection->fd();
                        SendUnsolicitedStatus(connection.get(), connection_request_response);
                        CloseConnection(connFd);
                        delete connection_request_response;
                        break;
                    }
                    //VLOG(4) <<"\nIngesting command:\n"<< connection_request_response->command()->DebugString();
                    if (_batchSetCollection.isBatchCommand(command)) {
                        if (!IngestBatchCmds(connection, command, connection_request_response, valBuf, valSize)) {
                            int connFd = connection->fd();
                            SendUnsolicitedStatus(connection.get(), connection_request_response);
                            CloseConnection(connFd);
                            delete connection_request_response;
                            break;
                        }
                        bBatchCmd = true;
                    } else {
                        status = connection->ReadValue(valSize, connection_request_response);
                        if (status == MessageStreamInterface::MessageStreamReadStatus_NO_SPACE) {
                            connection_request_response->response_command()->mutable_status()->
                                    set_code(Command_Status_StatusCode_NO_SPACE);
                        }
                        bBatchCmd = false;
                    }
                } else {
                    connection_request_response->
                            SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "Unable to process proto message");
                    int connFd = connection->fd();
                    SendUnsolicitedStatus(connection.get(), connection_request_response);
                    CloseConnection(connFd);
                    delete connection_request_response;
                    break;
                }
            } else {
                connection_request_response->
                        SetResponseCommand(Command_Status_StatusCode_INVALID_REQUEST,
                                           "Unable to process proto message");
                int connFd = connection->fd();
                SendUnsolicitedStatus(connection.get(), connection_request_response);
                CloseConnection(connFd);
                delete connection_request_response;
                return;
            }
            IncomingValueInterface* value = connection_request_response->
                                                request_value();
            if (ValidateMessage(connection.get(), connection_request_response) == ConnectionStatusCode::CONNECTION_ERROR) {
                if (ConnectionHandler::_batchSetCollection.isBatchCommand(command)) {
                    Command* cmd = connection_request_response->response_command();
                    if (cmd->status().code() == Command_Status_StatusCode_INVALID_REQUEST) {
                        cmd->mutable_status()->set_code(Command_Status_StatusCode_INVALID_BATCH);
                    }
                    if (valBuf) {
                        smr::DynamicMemory::getInstance()->deallocate(valSize);
                    }
                    value->FreeUserValue();
                } else {
                    if (value) {
                        if (value->GetUserValue()) {
                            smr::DynamicMemory::getInstance()->deallocate(value->size());
                        }
                        value->FreeUserValue();
                    }
                }
                int connFd = connection->fd();
                SendUnsolicitedStatus(connection.get(), connection_request_response);
                CloseConnection(connFd);
                delete connection_request_response;
                break;
            }
            VLOG(4) <<"\n After Ingesting command:\n"<< connection_request_response->command()->DebugString();

            profiler_.End(deserialization);
            if (ValidateCommand(connection.get(), connection_request_response) == ConnectionStatusCode::CONNECTION_ERROR) {
                if (ConnectionHandler::_batchSetCollection.isBatchCommand(command)) {
                    if (valBuf) {
                        smr::DynamicMemory::getInstance()->deallocate(valSize);
                    }
                    value->FreeUserValue();
                } else {
                    if (value) {
                        if (value->GetUserValue()) {
                            smr::DynamicMemory::getInstance()->deallocate(value->size());
                        }
                        value->FreeUserValue();
                    }
                }
            }
            if (_batchSetCollection.isEndBatchCommand(command) || _batchSetCollection.isAbortBatchCommand(command)) {
                bBatchCmd = false;
            }
            //Initialize the Priority, and Time fields
            //for the Time Handler and Connection Struct
            //Used for interrupts and "Expired" checks
            Server::connection_map.ValidateConnection(connection->fd());
            InitConnectionPriorityAndTime(connection.get(), connection_request_response);
            connection_request_response->SetConnection(connection);
            connection_queue_.clearLatency(connection.get()->id());
            requestQueue_.enqueue(connection_request_response);
            // May need this later for closing connnections cleanly
            //if (!_batchSetCollection.isBatch(command)) {
            //    connection->increment_cmds_count();
            //} else if (_batchSetCollection.isStartBatchCommand(command) || _batchSetCollection.isEndBatchCommand(command)) {
            //    connection->increment_cmds_count();
            //}
            //cout << " ENQ INCRE CMDS " << connection->get_cmds_count() << " " << connection->fd() << endl;
            // Only allow more commands to come in if state of connection is valid
            // May not need this, one can argue with current flow will are protected here
#ifdef RESPONSE
            if (!bBatchCmd) {
                gettimeofday(&tv, NULL);
                end = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
                uint64_t lapsed = end -start;
                if (lapsed > max_cmd_read) {
                    cout << "MAX READ CMD " << lapsed << endl;
                    max_cmd_read = lapsed;
                }
            }
#endif
        } while (bBatchCmd);
        if (connection->state() != ConnectionState::CLOSED) {
            Server::SetActiveFD(connection->fd());
        }
    } //End while true
}

void ConnectionHandler::SendPoisonPills(size_t number_worker_threads, size_t number_response_threads, size_t number_cmd_ingest_threads) {
    VLOG(1) << ": SendPoisonPills(): Enter: " << currentState();
    for (size_t i = 0; i < number_cmd_ingest_threads; ++i) {
        std::shared_ptr<Connection> connection(new Connection(ConnectionHandler::kPoisonPillId));
        VLOG(1) << ": Enqueueing poision pill to ingest queue: " << currentState();
        Enqueue(connection);
    }
    MutexLock l(&mu_ingest_);
    while (!connection_for_ingest_.empty()) {
        cv_ingest_.TimedWait();
    }
    VLOG(1) << ": SendPoisonPills(): Exit: " << currentState();
}

void ConnectionHandler::SetResponseTypeAndAckSequence(const Command* command,
        Command *command_response) {
    command_response->mutable_header()->
            set_acksequence(command->header().sequence());

    switch (command->header().messagetype()) {
        case Command_MessageType_GET:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_GET_RESPONSE);
            break;
        case Command_MessageType_PUT:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_PUT_RESPONSE);
            break;
        case Command_MessageType_DELETE:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_DELETE_RESPONSE);
            break;
        case Command_MessageType_GETNEXT:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_GETNEXT_RESPONSE);
            break;
        case Command_MessageType_GETPREVIOUS:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_GETPREVIOUS_RESPONSE);
            break;
        case Command_MessageType_GETKEYRANGE:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_GETKEYRANGE_RESPONSE);
            break;
        case Command_MessageType_GETVERSION:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_GETVERSION_RESPONSE);
            break;
        case Command_MessageType_SETUP:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_SETUP_RESPONSE);
            break;
        case Command_MessageType_GETLOG:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_GETLOG_RESPONSE);
            break;
        case Command_MessageType_SECURITY:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_SECURITY_RESPONSE);
            break;
        case Command_MessageType_NOOP:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_NOOP_RESPONSE);
            break;
       case Command_MessageType_PEER2PEERPUSH:
           command_response->mutable_header()->
               set_messagetype(Command_MessageType_PEER2PEERPUSH_RESPONSE);
           break;
        case Command_MessageType_FLUSHALLDATA:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_FLUSHALLDATA_RESPONSE);
            break;
        case Command_MessageType_PINOP:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_PINOP_RESPONSE);
            break;
        case Command_MessageType_MEDIASCAN:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_MEDIASCAN_RESPONSE);
            break;
        case Command_MessageType_MEDIAOPTIMIZE:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_MEDIAOPTIMIZE_RESPONSE);
            break;
        case Command_MessageType_START_BATCH:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_START_BATCH_RESPONSE);
            break;
        case Command_MessageType_END_BATCH:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_END_BATCH_RESPONSE);
            break;
        case Command_MessageType_ABORT_BATCH:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_ABORT_BATCH_RESPONSE);
            break;
        case Command_MessageType_SET_POWER_LEVEL:
            command_response->mutable_header()->
                set_messagetype(Command_MessageType_SET_POWER_LEVEL_RESPONSE);
            break;
        default:
            LOG(INFO) << "Message has unrecognized or inapplicable type";
            break;
    }
}

// HMAC Errors have a unique function so that we always send the same unsolicited status message
// Otherwise, we might inadvertently leak id or HMAC info.  All other unsolicited errors should
// use SendUnsolicitedStatus directly
void ConnectionHandler::SetHMACError(Command* response_command) {
    LOG(INFO) << "Received command with unknown id or incorrect or missing HMAC";//NO_SPELL
    response_command->mutable_status()->set_statusmessage("Incorrect or missing HMAC");
    response_command->mutable_status()->set_code(Command_Status_StatusCode_HMAC_FAILURE);
}

///////////////////////////////////////////////////
/// SerializeAndSendMessage now takes in a connection_request_response object, the current,
/// repsonse and request that will be serialized and sent over the wire.
///@param[in] - connection
///@param[in] - message_value
///@param[in] - connection_request_response current request to be sent
///@param[out] - ConnectionStatusCode status of connection
ConnectionStatusCode ConnectionHandler::SerializeAndSendMessage(
            Connection *connection,
            NullableOutgoingValue* message_value,
            ConnectionRequestResponse *connection_request_response) {
    //MutexLock l(&mu_);
    Event serialization;
    profiler_.Begin(kMessageSerialization, &serialization);
    std::string serialized_command_string;
    connection_request_response->response_command()->SerializeToString(&serialized_command_string);
    connection_request_response->response()->set_commandbytes(serialized_command_string);
    bool response_proto_too_large = (size_t)(connection_request_response->response()->ByteSize())
            > limits_.max_message_size();
    if (message_value != NULL) {
        bool response_value_too_large = message_value->size() > limits_.max_value_size();

        if ((response_proto_too_large || response_value_too_large) &&
            ((connection_request_response->response()->authtype() != Message_AuthType_HMACAUTH) ||
                (connection_request_response->command()->header().messagetype() !=
                    Command_MessageType_GETLOG_RESPONSE))) {
            connection_request_response->response_command()->Clear();
            SetResponseTypeAndAckSequence(connection_request_response->command(),
                                          connection_request_response->response_command());
            LOG(ERROR) << "IE Cmd Status";//NO_SPELL
            connection_request_response->SetResponseCommand(Command_Status_StatusCode_INTERNAL_ERROR,
                                                            "Response too large");
            connection_request_response->response_command()->
                                     SerializeToString(&serialized_command_string);
            connection_request_response->response()->set_commandbytes(serialized_command_string);
        }
    }
    if (connection_request_response->request()->authtype() == Message_AuthType_HMACAUTH) {
        Event mac;
        profiler_.Begin(kMacAssignment, &mac);
        authenticator_.AssignMac(connection_request_response->user(), connection_request_response->response());
        profiler_.End(mac);
    }

    // WriteMessage() make separate write() calls for the header and values which
    // creates several small packets. It's nicer to send one big packet so we set cork here.

    // Need to initialize to zero is we do not use garbage data
    int err = 0;
    int write_status = connection->SendMessage(connection_request_response, message_value, &err);
    switch (write_status) {
        case 0:
#ifdef KDEBUG
            DLOG(INFO) << "Successfully sent response";
#endif
            break;
        case 4:
            // Only log if err was set
            if (err != 0) {
                LOG(ERROR) << "Err is set to: " << err;
            }

            // Encountered an error, write log to disk
            connection_queue_.LogLatency(LATENCY_EVENT_LOG_UPDATE);
            LogRingBuffer::Instance()->makePersistent();
            if (err == EIO) {
                std::string key;
                Command_MessageType type = connection_request_response->response_command()->
                                                                        header().messagetype();
                if (type == Command_MessageType_GETNEXT) {
                    key = GetKey(connection_request_response->response_command()->
                                                              body().keyvalue().key(), true);
                } else if (type == Command_MessageType_GETPREVIOUS) {
                    key = GetKey(connection_request_response->response_command()->
                                                              body().keyvalue().key(), false);
                } else {
                    key = connection_request_response->response_command()->body().keyvalue().key();
                }
                SetRecordStatus(key);
            }
            LOG(ERROR) << "Logging failure after SendMessage attempt";//NO_SPELL
            statistics_manager_.IncrementFailureCount(
                connection_request_response->command()->header().messagetype());
            return ConnectionStatusCode::CONNECTION_ERROR;
        default:
            // If sending fails we break before removing the cork but that's OK because
            // the connection will be closed anyway
            LOG(ERROR) << "Return ConnectionStatusCode::CONNECTION_ERROR";
            //connection_request_response->response_command()->Clear();
            SetResponseTypeAndAckSequence(connection_request_response->command(),
                                                      connection_request_response->response_command());
            connection_request_response->SetResponseCommand(Command_Status_StatusCode_INTERNAL_ERROR,
                                                                        "Failed to send response");
            //connection_request_response->response_command()->SerializeToString(&serialized_command_string);
            //            connection_request_response->response()->set_commandbytes(serialized_command_string);
            //LOG(INFO) << "=== # connections: " << Server::connection_map.TotalConnectionCount();
            return ConnectionStatusCode::CONNECTION_ERROR;
    }

    connection->UpdateMostRecentAccessTime();

    // Pull the cork to make sure any partially-filled packet gets sent immediately
    FlushTcpCork(connection->fd());
    profiler_.End(serialization);
    return ConnectionStatusCode::CONNECTION_OK;
}

// Assumes that response_command has already been populated with the outgoing message
ConnectionStatusCode ConnectionHandler::SendUnsolicitedStatus(
        Connection *connection, ConnectionRequestResponse *connection_request_response, bool fromResponseThread) {
    if (!fromResponseThread) {
        // if pending_status_map_ has connFd, do flush to send all pending responses to response queue
        if (pending_status_map_.find(connection->id()) != pending_status_map_.end()) {
            LOG(INFO) << "Flush to move pending status to response queue";
            message_processor_.Flush();
        }
    }
    connection_request_response->response_command()->mutable_header()->set_connectionid(connection->id());
    NullableOutgoingValue response_val;
    connection_request_response->SetResponseCommand(Message_AuthType_UNSOLICITEDSTATUS);
    return SerializeAndSendMessage(connection, &response_val, connection_request_response);
}

///////////////////////////////////////////////////
///Hand off appropriate connection Command elements to @MessageProcessor
///Has access to mutex locks associated with Pin Operations
///
///Upon Return, if the Operation was interrupted (currently only possible in mediascan)
///do not consume the Request value for connection, as it will potentially
///be needed later
///------------------------
/// -If(NOT interrupted)
///    -then: check request value for consumption
///    -then: call HandleResponse()
/// -If(interrupted)
///    -then: return interrupt status in-order to be re-enqueued
/// -----------------------
///@param[in] - connection struct
///@param[in] - response_value
///@param[out] - ConnectionStatusCode (via Handle Response or an Explicit Interrupt)
///--------------------
///TODO(jdevore): flow of this function needs improvement.
//                (pin op checking flow vs hmac flow if statements)
ConnectionStatusCode ConnectionHandler::ExecuteOperation(std::shared_ptr<Connection> connection,
                                                         ConnectionRequestResponse *connection_request_response) {
    RequestContext request_context = {connection->use_ssl()};
    Command* cmd = connection_request_response->command();
    #ifdef QOS_ENABLED
    //if ((PUT or DELETE) and not FLUSH or SHORT_LATENCY) flag to retain
    bool FullRetain = ((!_batchSetCollection.isBatchCommand(cmd) &&
                        ((connection_request_response->
                                     command()->header().messagetype() ==
                       Command_MessageType_PUT) ||
                       connection_request_response->
                                   command()->header().messagetype() ==
                       Command_MessageType_DELETE) &&
                      ((connection_request_response->
                                   command()->body().keyvalue().synchronization() !=
                       proto::Command_Synchronization_FLUSH) &&
                       !connection_request_response->
                                    command()->body().keyvalue().flush()) &&
                      (connection_request_response->
                                   command()->body().keyvalue().prioritizedqos(0) !=
                       Command_QoS_SHORT_LATENCY)) ||
                       (connection_request_response->
                                     command()->header().messagetype() ==
                       Command_MessageType_END_BATCH));
    #else
    //if ((PUT or DELETE) and WRITETHROUGH) flag to retain
    bool FullRetain = (connection_request_response->command()->header().messagetype() == Command_MessageType_END_BATCH ||
                      (!_batchSetCollection.isBatchCommand(cmd) &&
                              ((connection_request_response->command()->header().messagetype() == Command_MessageType_PUT) ||
                                connection_request_response->command()->header().messagetype() == Command_MessageType_DELETE) &&
                       connection_request_response->command()->body().keyvalue().synchronization() == proto::Command_Synchronization_WRITETHROUGH));
    #endif

    if (connection_request_response->
                    request()->authtype() == Message_AuthType_PINAUTH ||
        connection_request_response->
                    command()->header().messagetype() == Command_MessageType_SECURITY) {
        CHECK(!pthread_mutex_lock(&mtx_pinOP_in_progress));
        pinOP_in_progress = true;
    }
    if (connection_request_response->
                    request()->authtype() == Message_AuthType_HMACAUTH) {
        connection_request_response->
                    response()->set_authtype(Message_AuthType_HMACAUTH);
        bool corrupt = (server_->GetStateEnum() == StateEnum::STORE_CORRUPT);
        // Save request value pointer because it can be set to NULL by ProcessMessager()
        IncomingValueInterface* request_value = connection_request_response->request_value();
        message_processor_.ProcessMessage(*(connection_request_response),
                                          connection_request_response->response_value(),
                                          request_context, connection->id(), connection->fd(),
                                          false, corrupt);
        if (connection_request_response->
                        command()->header().messagetype() == Command_MessageType_PUT) {
            const Command command = *(connection_request_response->
                                                  command());
            proto::Command_KeyValue const& keyvalue = command.body().keyvalue();
            int key_size = keyvalue.key().size();
            int value_size = request_value->size();
            //Log Key and Value size for histogram data
            LogRingBuffer::Instance()->logKeyValueHisto(key_size, value_size);
            LogRingBuffer::Instance()->logTransferLength(key_size, value_size);
        } else if (connection_request_response->
                               command()->header().messagetype() == Command_MessageType_GET) {
            const Command command = *(connection_request_response->command());
            proto::Command_KeyValue const& keyvalue = command.body().keyvalue();
            int key_size = keyvalue.key().size();
            int value_size = connection_request_response->response_value()->size();
            //Log Key and Value size for histogram data
            LogRingBuffer::Instance()->logKeyValueHisto(key_size, value_size);
            LogRingBuffer::Instance()->logTransferLength(key_size, value_size);
      }
    } else {
        // This will be Pin Auth.  All other Auth types are rejected in ValidateMessage
        connection_request_response->
            response()->set_authtype(Message_AuthType_PINAUTH);
        message_processor_.ProcessPinMessage(*(connection_request_response->
                                                         command()),
                                             connection_request_response->
                                                         request_value(),
                                             connection_request_response->
                                                         response_command(),
                                             NULL,
                                             request_context,
                                             connection_request_response->
                                                         request()->pinauth(),
                                             connection.get());
    }
    // pthread_mutex_lock(&mtx_pinOP_in_progress);
    if (connection_request_response->
                  request()->authtype() == Message_AuthType_PINAUTH ||
        connection_request_response->
                  command()->header().messagetype() == Command_MessageType_SECURITY) {
        pinOP_in_progress = false;
        CHECK(!pthread_mutex_unlock(&mtx_pinOP_in_progress));
    }
    // if (Flagged for Retain) and (SUCCESS)):
    // Add to pending collection, Arm Timer, Return
    if (FullRetain &&
        (connection_request_response->
            response_command()->status().code() == Command_Status_StatusCode_SUCCESS)) {
        uint64_t timeout, elapsedTime;

        if (connection_request_response->command()->header().messagetype() == Command_MessageType_END_BATCH) {
            BatchSet* batchSet = NULL;
            batchSet = _batchSetCollection.getBatchSet(connection_request_response->command()->header().batchid(), connection->id());
            if (batchSet) {
                timeout = batchSet->getTimeOut();
                elapsedTime = duration_cast<milliseconds>(std::chrono::high_resolution_clock::now() - batchSet->getEnqueuedTime()).count();
            } else {
                connection_request_response->SetResponseCommand(Command_Status_StatusCode_INTERNAL_ERROR,
                                                           "Batch was not found");
            }
        } else {
            timeout = connection_request_response->Timeout();
            elapsedTime = duration_cast<milliseconds>(std::chrono::high_resolution_clock::now() - connection->getEnqueuedTime()).count();
        }

        // Truncate maximum timeout to cover OVS
        if (timeout > 7000) {
            timeout = 7000;
        }
        if (elapsedTime > timeout) {
            bool shortenTimeout = true;
            // Check for pending connections
            if (!connection_for_ingest_.empty()) {
                std::shared_ptr<Connection> nextCon = connection_for_ingest_.front();
                elapsedTime = duration_cast<milliseconds>(std::chrono::high_resolution_clock::now() - nextCon->getEnqueuedTime()).count();
                if (elapsedTime > timeout) {
                    shortenTimeout = false;
                }
            }
            // Check for pending request
            ConnectionRequestResponse* nextRequest = requestQueue_.peekTop();
            if (nextRequest != NULL) {
                elapsedTime = duration_cast<milliseconds>(std::chrono::high_resolution_clock::now() - nextRequest->EnqueuedTime()).count();
                if (elapsedTime > timeout) {
                    shortenTimeout = false;
                }
            }
            if (shortenTimeout) {
                timeout = 0;
            }
        } else {
            timeout = timeout - elapsedTime;
        }
        connection_request_response->SetTimeProcessed();
        mu_.Lock();
        connection->AddCurrentToResponsePending(connection_request_response);
        // Only add to pending map if connection hasn't be already added
        AddToPendingMap(connection);
        mu_.Unlock();
        // If have reached limit for number of pending statuses schedule a flush to occur
        if (connection->NumPendingStatus() >= Connection::PENDING_STATUS_MAX) {
            timeout = 0;
        } else if (connection_request_response->command()->header().messagetype() == Command_MessageType_END_BATCH) {
            numBatchesRetained.Increment();
        }
        if (connection->HasPendingStatuses()) {
            aging_timer_.ArmTimer(timeout);
        }
        return ConnectionStatusCode::CONNECTION_OK;
    } else if (!connection_request_response->Interrupted()) {
        connection_request_response->SetTimeProcessed();
        // Do not enqueue response for individual error batched commands.  They are responded after END_BATCH received.
        bool bQueue = true;
        Command* cmd = connection_request_response->command();
        if (_batchSetCollection.isBatchCommand(cmd) && _batchSetCollection.isCommand(cmd)) {
            Command_Status_StatusCode respStatus = connection_request_response->response_command()->status().code();
            if (respStatus == Command_Status_StatusCode_SUCCESS || respStatus == Command_Status_StatusCode_NO_SPACE ||
                    respStatus != Command_Status_StatusCode_INVALID_BATCH) {
                bQueue = false;
            }
        }
        if (bQueue) {
            std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection_pair(connection, ULLONG_MAX, false);
            connection->AddToResponsePending(connection_request_response);
            response_connection_queue_.Enqueue(connection_pair, true);
        } else {
            delete connection_request_response;
        }
        return ConnectionStatusCode::CONNECTION_OK;
    } else {
        if (connection_request_response->Interrupted()) {
            connection_request_response->ResetInterrupt();
            requestQueue_.enqueue(connection_request_response);
        } else {
            delete connection_request_response;
        }
        return ConnectionStatusCode::CONNECTION_INTERRUPT;
    }
}

//////////////////////////////////////
//Handle Response Message After A command has completed
//Serializes and Sends Message, Checks for INVALID REQUEST
//Returns either Connection OK or ERROR back to Connection Thread Worker
//Handle Response now operates over a list of ConnectionRequestResponse
// in the normal case, meaning statuses that are not hold of will only operate
// over a list that contains one of these objects, the current ConnectionRequestResponse object
// for the connection. Otherwise iterate over the pending ConnectionRequestResponse objects.
//Handle Response also takes care of cleaning up the request_value and the ConnectionRequestResponse object.//NOLINT
///@param[in] - connection
///@param[in] - response_value
///@param[in] - ack_sequence
///@param[in] - success
///@param[out] - ConnectionStatusCode status of connection
ConnectionStatusCode ConnectionHandler::HandleResponse(Connection *connection,
        NullableOutgoingValue *response_value, uint64_t ack_sequence, bool validAckSeq, bool success) {
    NullableOutgoingValue *res_value = response_value;

    connection->set_response_in_progress(true);

    std::vector<ConnectionRequestResponse*> connection_request_response_vector;
    ConnectionStatusCode res = ConnectionStatusCode::CONNECTION_OK;

    // if ack_sequence is less than 0 then operate on current_connection_request of the connection
    if (!validAckSeq) {
        while (true) {
            ConnectionRequestResponse *connection_request_response = connection->Dequeue();//NOLINT
            if (connection_request_response == NULL) {
                break;
            }
            if (connection_request_response->response_command()->status().code() != Command_Status_StatusCode_SUCCESS &&
                _batchSetCollection.isBatch(connection_request_response->command())) {
            }
            connection_request_response_vector.push_back(connection_request_response);
        }
    } else {
        while (true) {
            ConnectionRequestResponse *connection_request_response = connection->Dequeue(ack_sequence, success);//NOLINT
            if (connection_request_response == NULL) {
                break;
            }
            connection_request_response_vector.push_back(connection_request_response);
        }
    }
    for (std::vector<ConnectionRequestResponse*>::iterator it
         = connection_request_response_vector.begin();
         it != connection_request_response_vector.end(); ++it) {
        //If an invalid request response status was sent to client, close the connection
        Command* cmd = (*it)->command();
        if ((*it)->response_command()->status().code() ==
            Command_Status_StatusCode_INVALID_REQUEST ||
              (*it)->response_command()->status().code() ==
            Command_Status_StatusCode_INVALID_BATCH) {
            if (!_batchSetCollection.isBatchCommand(cmd) ||
                 _batchSetCollection.isBatchableCommand(cmd) ||
                  _batchSetCollection.isStartBatchCommand(cmd)) {
                LOG(INFO) << "Invalid Request/Batch - Sending unsolicited status and closing";
                if (_batchSetCollection.isBatchableCommand(cmd) || _batchSetCollection.isStartBatchCommand(cmd)) {
                    _batchSetCollection.deleteBatchesOnConnection(connection->id());
                }
                SendUnsolicitedStatus(connection, *it, true);
                connection->SetState(ConnectionState::SHOULD_BE_CLOSED);
                Server::connection_map.MoveToWillBeClosed(connection->fd());
                res = ConnectionStatusCode::CONNECTION_ERROR;
            }
        }
        // Batched command has no response.
        if (_batchSetCollection.isEndBatchCommand(cmd)) {
            if (numBatchesRetained.Read() > 0) {
                numBatchesRetained.Decrement();
            }
        }
        if (res == ConnectionStatusCode::CONNECTION_OK &&
            !_batchSetCollection.isBatchableCommand(cmd)) {
            (*it)->response_command()->mutable_header()->set_connectionid((*it)->command()->header().connectionid());
            if (_batchSetCollection.isStartBatchCommand(cmd) || (_batchSetCollection.isEndBatchCommand(cmd))) {
                (*it)->response_command()->mutable_header()->set_batchid((*it)->command()->header().batchid());
            }

            if (res_value == NULL) {
                res_value = (*it)->response_value();
            }
            res = SerializeAndSendMessage(
                connection,
                res_value,
                (*it));
            if (res != ConnectionStatusCode::CONNECTION_OK) {
                ConnectionRequestResponse requestResponse;
                SendUnsolicitedStatus(connection, &requestResponse);
                connection->SetState(ConnectionState::SHOULD_BE_CLOSED);
                Server::connection_map.MoveToWillBeClosed(connection->fd());
            }
            // May need this later for closing connections cleanly
            //connection->decrement_cmds_count();
        }
        if (_batchSetCollection.isBatch(cmd) && !_batchSetCollection.isBatchableCommand(cmd)) {
            if (_batchSetCollection.isStartBatchCommand(cmd)) {
                if ((*it)->response_command()->status().code() == Command_Status_StatusCode_SUCCESS ||
                    ((*it)->response_command()->status().code() == Command_Status_StatusCode_NO_SPACE)) {
                    BatchSet* batchSet = _batchSetCollection.getBatchSet(cmd->header().batchid(), connection->id());
                    TimeoutEvent* event = new TimeoutEvent(batchSet);
                    EventService::getInstance()->subscribe(event, this);
                } else {
                    LOG(INFO) << "Invalid start batch request - Sending unsolicited status and closing";
                    _batchSetCollection.deleteBatchesOnConnection(connection->id());
                    connection->SetState(ConnectionState::SHOULD_BE_CLOSED);
                    SendUnsolicitedStatus(connection, *it);
                    Server::connection_map.MoveToWillBeClosed(connection->fd());
                    res = ConnectionStatusCode::CONNECTION_ERROR;
                }
            } else {
                BatchSet* batchSet = _batchSetCollection.getBatchSet(cmd->header().batchid(), connection->id());
                if (batchSet) {
                    TimeoutEvent event(batchSet);
                    EventService::getInstance()->unsubscribe(&event, this);
                }
            }
        }
        if (!_batchSetCollection.isBatchableCommand(cmd)) {
            (*it)->SetTimeResponded();
            // Need to iterate over pending status queue or operate solely on current
            if ((*it)->ExceededMaxResponseTime()) {
                (*it)->SetMaxResponseTime();
                LOG(INFO) << "MAX RES TIME SO FAR " << (*it)->MaxResponseTime().count()
                        << " ms" << " for connection " << connection->fd();
                //show in error log file only if response time > 5s
                if ((*it)->MaxResponseTime().count() > std::chrono::duration<int64_t, std::ratio<1ll, 1000ll>>(FLAGS_latency_threshold).count()) {
                    stringstream ss;
                    ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << endl;
                    ss << endl << "MAX RES " << (*it)->MaxResponseTime().count() << endl;
                    leveldb::Status::IOError(ss.str());
                }
                #ifndef NDEBUG
                connection_queue_.LogLatency(LATENCY_EVENT_LOG_UPDATE);
                LogRingBuffer::Instance()->makePersistent();
                #endif
            }
            if (_batchSetCollection.isEndBatchCommand(cmd)) {
                BatchSet* batchSet = _batchSetCollection.getBatchSet(cmd->header().batchid(), connection->id());
                if (batchSet) {
//#ifndef NDEBUG
                // Time from START BATCH recieved to END BATCH received
                    std::chrono::milliseconds time_span =
                          duration_cast<milliseconds>((*it)->EnqueuedTime() - batchSet->getEnqueuedTime());
                    if (time_span.count() > (*it)->GetMaxStartEndBatchRcv().count()) {
                        (*it)->SetNewMaxStartEndBatchRcv(time_span);
                        LOG(INFO) << " Max Time from START BATCH received to END BATCH received " << time_span.count() << " ms";
                        stringstream ss;
                        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << endl;
                        ss << endl << "MAX START-END-BATCH " << time_span.count() << endl;
                        leveldb::Status::IOError(ss.str());
                    }

                // Time from START BATCH RECEIVED to END BATCH responsed
                    time_span = duration_cast<milliseconds>((*it)->GetTimeResponded() - batchSet->getEnqueuedTime());
                    if (time_span.count() > (*it)->GetMaxStartEndBatchResp().count()) {
                        (*it)->SetNewMaxStartEndBatchResp(time_span);
                        LOG(INFO) << " Batch response from END TO START BATCH " << time_span.count() << " ms";
                        stringstream ss;
                        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << endl;
                        ss << endl << "MAX RESPONSE START-END-BATCH " << time_span.count() << endl;
                        leveldb::Status::IOError(ss.str());
                    }
//#endif
                    string batchSetId = batchSet->getId();
                    _batchSetCollection.deleteBatchSet(batchSetId);
                }
            }
            if (cmd->body().pinop().pinoptype() == Command_PinOperation_PinOpType_ERASE_PINOP ||
                (cmd->body().pinop().pinoptype() == Command_PinOperation_PinOpType_SECURE_ERASE_PINOP)) {
                if ((*it)->response_command()->status().code() == Command_Status_StatusCode_SUCCESS) {
                    (*it)->ResetMaxResponseTime();
                }
            }
            int message_type = (*it)->response_command()->header().messagetype();
            std::chrono::milliseconds recorded_operation_maxlatency = statistics_manager_.GetLatencyForMessageType(message_type);
            std::chrono::milliseconds current_op_response_time = (*it)->GetMaxResponseTimeForOperation();
            if (current_op_response_time.count() > recorded_operation_maxlatency.count()) {
                statistics_manager_.SetMaxLatencyForMessageType(message_type, current_op_response_time);
            }
        }
        if (!_batchSetCollection.isBatchableCommand(cmd)) {
            statistics_manager_.IncrementOperationCount((*it)->
                    response_command()->header().messagetype());
            if (res_value != NULL) {
                statistics_manager_.IncrementByteCount((*it)->response_command()->header().messagetype(),
                    res_value->size());
            } else {
                statistics_manager_.IncrementByteCount((*it)->response_command()->header().messagetype(), 0);
            }
            DLOG(INFO) << "Done handling message";

            unsigned char *bit = connection_queue_.GetLatencyCauseBitMask(connection->id());

            // Make log persistent if message status code matches condition
            if (LogRingBuffer::Instance()->checkTrigger((*it)->response_command()->status().code())) {
                // Log error
                LOG(ERROR) << "RecordOperation for failed operation"; //NO_SPELL
                statistics_manager_.IncrementFailureCount((*it)->command()->header().messagetype());
            }

            // Log command to command history buffer
            std::time_t time_queued = std::chrono::high_resolution_clock::to_time_t(
                (*it)->EnqueuedTime());
            std::time_t time_dequeued = std::chrono::high_resolution_clock::to_time_t(
                (*it)->DequeuedTime());
            // Command Successful
            std::string success_message = "FALSE";
            if ((*it)->response_command()->status().code() ==
                Command_Status_StatusCode_SUCCESS) {
                success_message = "TRUE";
            }
            int time_elapsed = (*it)->TimeProcessElapsed();
            LogRingBuffer::Instance()->logCommand(time_queued, time_dequeued,
                time_elapsed, (*it)->command()->header().messagetype(), success_message);
            if (time_elapsed > FLAGS_latency_threshold && *bit != 0) {
                int bitint = *bit;
                VLOG(1) << "Time queued: " << time_queued << " Time dequeued: " << time_dequeued
                        << " Time to process: " << time_elapsed;
                VLOG(1) << "bitmask: " << bitint;//NO_SPELL
                num_latency_commands_++;

                if (num_latency_commands_ == LATENCY_COMMAND_MAX) {
                    connection_queue_.LogLatency(LATENCY_EVENT_LOG_UPDATE);
                    LogRingBuffer::Instance()->makePersistent();
                    num_latency_commands_ = 0;
                }
            }
            connection_queue_.LogLatency(LATENCY_EVENT_LOG_OUTSTANDINGCOMMAND);
            res_value = NULL;
        }
       delete (*it);
    }
    connection->set_response_in_progress(false);
    if (server_->GetStateEnum() == StateEnum::DOWNLOAD) {
        Server::_shuttingDown = true;
        KickOffDownload(connection);
    }
    return res;
}

void ConnectionHandler::FlushTcpCork(int fd) {
    // OSX doesn't support cork or even define TCP_CORK so make this a noop on OSX.
#ifdef TCP_CORK
    int val =  0;
    if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &val, sizeof(val))) {
        PLOG(WARNING) << "Unable to pull TCP_CORK";//NO_SPELL
    }
    val = 1;
    // Enable/Disable Nagle's Algorithm - Approximate Cork on non-Linux
    if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &val, sizeof(val))) {
            PLOG(WARNING) << "Unable to set TCP_CORK";//NO_SPELL
    }
#endif
}


bool ConnectionHandler::SetRecordStatus(const std::string& key) {
    return message_processor_.SetRecordStatus(key);
}

const std::string ConnectionHandler::GetKey(const std::string& key, bool next) {
    return message_processor_.GetKey(key, next);
}

//////////////////////////////////////
//Re-enqueue an Interrupted Operation
//Without reinitializing the state for later (potential) resumption
void ConnectionHandler::ReEnqueueConnection(std::shared_ptr<Connection> connection) {
}

//////////////////////////////////////
//Get and Initialize the Priority Value in the connection struct
//specified from the proto message in order for the Priority Queue to appropriately
//place the Connection
void ConnectionHandler::InitCommandPriority(Connection *connection, ConnectionRequestResponse *connection_request_response) {
    connection->SetPriority(connection_request_response->command()->header().priority());
}

//////////////////////////////////////
//Initialize the timers for timeout, quanta and the time enqueued in order to begin
//timing the associated operation and its life cycle
void ConnectionHandler::InitCommandTimers(ConnectionRequestResponse *connection_request_response) {
    if (connection_request_response->command()->header().timeout() == 0) {
        connection_request_response->SetTimeout(ConnectionRequestResponse::DEFAULT_CMD_TIMEOUT);
    } else {
        connection_request_response->SetTimeout(connection_request_response->command()->header().timeout());
    }
    connection_request_response->SetTimeQuanta(connection_request_response->command()->header().timequanta());
}

//////////////////////////////////////
//Calls both the InitCommandPriority and InitCommandTimers functions above
void ConnectionHandler::InitConnectionPriorityAndTime(Connection *connection, ConnectionRequestResponse *connection_request_response) {
    InitCommandPriority(connection, connection_request_response);
    InitCommandTimers(connection_request_response);
}



void ConnectionHandler::AddToPendingMap(std::shared_ptr<Connection> connection) {
#ifdef KDEBUG
    DLOG(INFO) << "=== AddToPendingMap:: conn id:" << connection->id() //NO_SPELL
            << " fd:" << connection->fd() << " seq:" << connection->lastMsgSeq(); //NO_SPELL
#endif
    auto token_iter = pending_status_map_.find(connection->id());
    if (token_iter == pending_status_map_.end()) {
        pending_status_map_.insert({connection->id(), connection});
    }
#ifdef KDEBUG
    DLOG(INFO) << "=== AddToPendingMap::DONE. Map Size: " << pending_status_map_.size();//NO_SPELL
#endif
}

void ConnectionHandler::RemoveAllFromPendingMap() {
    MutexLock l(&mu_);
    pending_status_map_.clear();
}

bool ConnectionHandler::RemoveFromPendingMap(int64_t key) {
  mu_.AssertHeld();

#ifdef KDEBUG
    DLOG(INFO) << "=== RemoveFromPendingMap:: key: " << key;//NO_SPELL
#endif
    bool status = false;
    if (!pending_status_map_.empty()) {
        auto connection_iter = pending_status_map_.find(key);

        if (connection_iter != pending_status_map_.end()) {
#ifdef KDEBUG
            DLOG(INFO) << "\t-RemoveFromPendingMap:: key found ";//NO_SPELL
            DLOG(INFO) << "\t-RemoveFromPendingMap:: Erasing ";//NO_SPELL
#endif
            pending_status_map_.erase(connection_iter);
        }
    }
    return status;
}

void ConnectionHandler::SendAllPending(bool success,
                                       std::unordered_map<int64_t, uint64_t> token_list) {
#ifdef KDEBUG
    DLOG(INFO) << "======= SendAllPending() -For: "//NO_SPELL
               << pending_status_map_.size() << " connections";//NO_SPELL
#endif
    MutexLock l(&mu_);
    if (!pending_status_map_.empty()) {
#ifndef NLOG
        VLOG(2) << "Send All pending" << endl;
#endif
        for (auto it = token_list.begin(); it != token_list.end(); ++it) {
            auto connection_iter = pending_status_map_.find(it->first);
            if (connection_iter != pending_status_map_.end()) {
                connection_iter->second->SetStatusForResponse(success);
                if (connection_iter->second->HasPendingStatuses()) {
                    std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection_pair(connection_iter->second, it->second, true);
                    response_connection_queue_.Enqueue(connection_pair);
                } else {
                    pending_status_map_.erase(connection_iter);
                }
            }
        }
    }
    auto it = pending_status_map_.begin();
    while (it != pending_status_map_.end()) {
        if (!it->second->HasPendingStatuses()) {
            it = pending_status_map_.erase(it);
        } else {
            ++it;
        }
    }
#ifdef KDEBUG
    DLOG(INFO) << " === PendingMap FINISHED Sending  mapsize: "//NO_SPELL
               << pending_status_map_.size();//NO_SPELL
#endif
}

void ConnectionHandler::HaltTimer() {
#ifdef KDEBUG
    DLOG(INFO) << "=== Halt Aging Timer ===";
#endif
    if (numBatchesRetained.Read() == 0) {
        aging_timer_.DisarmTimer();
    }
}

void ConnectionHandler::SendUnsupportableResponse(std::shared_ptr<Connection> connection,
                                                  int select_fd, ConnectionRequestResponse *connection_request_response) {
    connection_request_response->
                response()->set_authtype(connection_request_response->
                                                     request()->authtype());
    SetResponseTypeAndAckSequence(connection_request_response->command(),
                                  connection_request_response->
                                              response_command());
    connection->AddToResponsePending(connection_request_response);
    NullableOutgoingValue response_value;
    ConnectionStatusCode status = HandleResponse(connection.get(), &response_value, 0, true, false);
    HandleConnectionStatus(connection, status, select_fd);
}

void ConnectionHandler::KickOffDownload(Connection *connection) {
    static bool _bExecutingFirmware = false;

    MutexLock l(&mu_);
    int workers_idle = 0;
    CHECK(!pthread_mutex_lock(&ConnectionQueue::mtx_thread_workers_idle));
    workers_idle = ConnectionQueue::thread_workers_idle;
    CHECK(!pthread_mutex_unlock(&ConnectionQueue::mtx_thread_workers_idle));

    auto it = pending_status_map_.begin();
    while (it != pending_status_map_.end()) {
        if (!it->second->HasStatusesPending()) {
            it = pending_status_map_.erase(it);
        } else {
            ++it;
        }
    }

    if (!_bExecutingFirmware && response_connection_queue_.Empty() && pending_status_map_.empty() &&
        (!connection->HasStatusesPending()) &&
        (workers_idle == Server::kInitialThreadPoolSize)) {
        switch (server_->GetStateEnum()) {
            case StateEnum::DOWNLOAD:
                _bExecutingFirmware = true;
                mu_.Unlock();
                server_->StateChanged(StateEvent::DOWNLOADED);
                mu_.Lock();
                _bExecutingFirmware = false;
                Server::_shuttingDown = true;
                break;
            case StateEnum::SHUTDOWN:
                mu_.Unlock();
                server_->StateChanged(StateEvent::READY_TO_SHUTDOWN);
                mu_.Lock();
                break;
            default:
                break;
        }
    }
}

void ConnectionHandler::inform(com::seagate::common::event::Event* event) {
    TimeoutEvent* timeoutEvent = (TimeoutEvent*)event;
    BatchSet* batchSet = (BatchSet*)timeoutEvent->publisher();
    if (batchSet->isComplete()) {  // Check here to avoid flushing time done below
        return;
    }
    int connFd = batchSet->getConnectionFd();
    message_processor_.Flush(true);
    mu_.Lock();
    std::shared_ptr<Connection> conn = Server::connection_map.GetConnection(connFd);
    TimeoutEvent timeEvent(batchSet);
    EventService::getInstance()->unsubscribe(&timeEvent, this);
    if (conn != nullptr) {
        ConnectionRequestResponse connection_request_response;
        connection_request_response.
                            SetResponseCommand(Command_Status_StatusCode_EXPIRED,
                                               "Timed out");
        Connection *connection = conn.get();
        mu_.Unlock();
        string batchSetId = batchSet->getId();
        if (!batchSet->isComplete()) {  // Check again before sending unsolicted message
            _batchSetCollection.deleteBatchSet(batchSetId);
            //_batchSetCollection.deleteBatchesOnConnection(connection->id());
            SendUnsolicitedStatus(connection, &connection_request_response);
            CloseConnection(connFd);
        }
    } else {
        LOG(INFO) << "Batch timeouts: not found, batchID = " << batchSet->getId() << ", #BatchSets = " << _batchSetCollection.numberOfBatchsets();
        string batchSetId = batchSet->getId();
        _batchSetCollection.deleteBatchSet(batchSetId);
        mu_.Unlock();
    }
}

string ConnectionHandler::currentState() {
    stringstream ss;
    ss << "#ingested connections = " << connection_for_ingest_.size() << ", #requests = " << requestQueue_.numberOfRequests() << endl;
    return ss.str();
}
} // namespace kinetic
} // namespace seagate
} // namespace com
