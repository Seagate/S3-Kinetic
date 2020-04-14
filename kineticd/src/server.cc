#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/syscall.h>
#include <fstream>
#include <vector>
#include <signal.h>
#include <memory>
#include <sstream>

#include "glog/logging.h"
#include "openssl/err.h"
#include "command_line_flags.h"

#include "port_listener.h"
#include "getlog_multicastor.h"
#include "signal_handling.h"
#include "announcer_controller.h"
#include "server.h"
#include "down_state.h"
#include "thread.h"
#include "schedule_compaction.h"
#include "multicast_announcer.h"
#include "lldp_announcer.h"
#include "connection_handler.h"
#include "stack_trace.h"
#include <sys/un.h>

using com::seagate::kinetic::announcer::AnnouncerController;
using std:: string;

bool kineticd_idle = true;

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::vector;

static const int alarm_time = 20;  // 20 seconds

static const char kVendorOptionsFile[] = "/kinetic_";
static const char kIPv6VendorOptionsFile[] = "/kinetic_ipv6_";
static const char kVendorFileExtension[] = ".txt";

int Server::epollfd;
int Server::select_fd[];
ConnectionMap Server::connection_map;

extern "C" void resetMsgCount() {
    // LOG(INFO) << "ALARM .... IN resetMsgCount";//NO_SPELL
}

bool Server::_shuttingDown = false;

static void* connection_thread_worker(void *arg) {
    ConnectionHandler *connection_handler = (ConnectionHandler *) arg;
    connection_handler->SetSelectFD(Server::select_fd[1]);
    connection_handler->ConnectionThreadWorker(Server::select_fd[1]);
    // Free thread-local OpenSSL resources
    ERR_remove_thread_state(NULL);
    return NULL;
}

static void* connection_response_worker(void *arg) {
    ConnectionHandler *connection_handler = (ConnectionHandler *) arg;
    connection_handler->ResponseThread();
    // Free thread-local OpenSSL resources
    ERR_remove_thread_state(NULL);
    return NULL;
}

static void* command_ingest_worker(void *arg) {
    ConnectionHandler *connection_handler = (ConnectionHandler *) arg;
    connection_handler->CommandIngestThread();
    // Free thread-local OpenSSL resources
    ERR_remove_thread_state(NULL);
    return NULL;
}

bool  Server::SetActiveFD(int fd) {
    struct epoll_event event;
    int status;
    memset(&event, 0, sizeof(struct epoll_event));
    std::shared_ptr<Connection> connection = Server::connection_map.GetConnection(fd);
    if (connection != nullptr) {
        if (connection->state() ==  ConnectionState::SHOULD_BE_CLOSED) {
            return false;
        } else {
            connection->SetState(ConnectionState::IDLE);
            event.data.fd = fd;
            event.events = EPOLLIN | EPOLLET;
            status = epoll_ctl(Server::epollfd, EPOLL_CTL_ADD, fd, &event);
            if (status == -1) {
                PLOG(ERROR) << "Epoll_ctl failed to ADD ";//NO_SPELL
                return false;
            }
            return true;
        }
    }
    return false;
}

void Server::ShowCertificate(SSL* ssl ) {
    X509 *cert;
    char *line;

    // Get certificates (if available)
    cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL) {
        VLOG(1) << "Server certificates";
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        VLOG(1) << "Subject: " <<  line;
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        VLOG(1) << "Issuer: " << line;
        free(line);
        X509_free(cert);
    } else {
        VLOG(1) << "No certificates.";
    }
}


SSL * Server::SetSSL(int fd) {
    SSL *ssl = SSL_new(ssl_context_);
    // We want to automatically retry reads and writes when a renegotiation
    // takes place. This way the only errors we have to handle are real,
    // permanent ones.
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    if (ssl == NULL) {
        LOG(ERROR) << "Failed to create new SSL object";//NO_SPELL
        return NULL;
    }
    if (SSL_set_fd(ssl, fd) != 1) {
        LOG(ERROR) << "Failed to associate SSL object with file descriptor";//NO_SPELL
        SSL_free(ssl);
        return NULL;
    }

    if (SSL_accept(ssl) != 1) {
        int ret = 0;
        int status = -1;
        status = SSL_get_error(ssl, ret);
        switch (status) {
            case SSL_ERROR_NONE:
                if (ret <= 0) break;
                LOG(INFO) << "SSL_ERROR_NONE and ret > 0";//NO_SPELL
                break;
            case SSL_ERROR_WANT_READ:
                LOG(INFO) << "SSL_ERROR_WANT_READ";//NO_SPELL
                break;
            case SSL_ERROR_WANT_WRITE:
                LOG(INFO) << "SSL_ERROR_WANT_WRITE";//NO_SPELL
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                LOG(INFO) << "SSL_ERROR_WANT_X509_LOOKUP";//NO_SPELL
                break;
            case SSL_ERROR_WANT_ACCEPT:
                LOG(INFO) << "SSL_ERROR_WANT_ACCEPT";//NO_SPELL
                break;
            case SSL_ERROR_WANT_CONNECT:
                break;
            case SSL_ERROR_SYSCALL:
                LOG(INFO) << "SSL_ERROR_SYSCALL";//NO_SPELL
                break;
            case SSL_ERROR_SSL:
                LOG(INFO) << "SSL_ERROR_SSL";//NO_SPELL
                break;
            case SSL_ERROR_ZERO_RETURN:
                LOG(INFO) << "SSL_ERROR_ZERO_RETURN";//NO_SPELL
                break;
            default:
                break;
            }
        PLOG(ERROR) << "Failed to perform SSL handshake status  ";//NO_SPELL
        LOG(ERROR) << "The client may have attempted to use an SSL/TLS version below TLSv1.1";//NO_SPELL//NOLINT
        SSL_free(ssl);
        return NULL;
    }
    ShowCertificate(ssl);
    VLOG(1) << "Successfully performed SSL handshake";//NO_SPELL
    return ssl;
}
bool Server::RemoveFromEpoll(int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    epoll_ctl(Server::epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
    return true;
}

bool Server::RegisterToEpoll(int fd) {
    struct epoll_event event;
    int status;
    memset(&event, 0, sizeof(struct epoll_event));
    event.data.fd = fd;
    event.events = EPOLLIN;
    event.events |= EPOLLET;
    VLOG(2) << "Register to epoll fd: " << fd;//NO_SPELL
    status = epoll_ctl(Server::epollfd, EPOLL_CTL_ADD, fd, &event);
    if (status == -1) {
        PLOG(ERROR) << "epoll_ctl failed to ADD fd " << fd;//NO_SPELL
        return false;
    }
    return true;
}

Server::Server(
    ConnectionHandler &connection_handler,
    const std::string &listen_ipv4_address,
    uint port, uint ssl_port,
    const std::string &listen_ipv6_address,
    int receive_buffer_size,
    int signal_pipe_fd,
    SSL_CTX *ssl_context,
    NetworkInterfaces& network_interfaces,
    DeviceInformationInterface& device_information,
    StatisticsManager& statistics_manager, Limits& limits,
    int heartbeat_frequency_seconds,
    P2PRequestManagerInterface& p2p_request_manager,
    KineticAlarms* kinetic_alarms)
        : connection_handler_(connection_handler),
        listen_ipv4_address_(listen_ipv4_address),
        port_(port), ssl_port_(ssl_port),
        listen_ipv6_address_(listen_ipv6_address),
        receive_buffer_size_(receive_buffer_size),
        signal_pipe_fd_(signal_pipe_fd),
        ssl_context_(ssl_context),
        network_interfaces_(network_interfaces),
        device_information_(device_information),
        statistics_manager_(statistics_manager),
        limits_(limits),
        heartbeat_frequency_seconds_(heartbeat_frequency_seconds),
        p2p_request_manager_(p2p_request_manager),
        kinetic_alarms_(kinetic_alarms) {
}

void Server::Initialize() {
    // Initilize State Management
    currentState_ = new DownState(this);
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&currentStateMutex_, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    LOG(INFO) << "========== In " << currentState_->GetName();
}

void Server::run() {
    pid_t tid;
    tid = syscall(SYS_gettid);
    cout << " SERVER THREAD ID " << tid << endl;
    // DO NOT DELETE THIS IS FOR MOBILER
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    cout << " SERVER THREAD ID " << tid << " CPU 0" << endl;
    StateChanged(StateEvent::START_ANNOUNCER);
    std::vector<std::unique_ptr<AnnouncerInterface>> announcers;
    CreateAnnouncers(announcers);
    AnnouncerController announcerController(
            heartbeat_frequency_seconds_,
            &announcers);

    max_connections_so_far_ = 0;
    alarm(1);

    int max_events = limits_.max_connections();

    if (!announcerController.Init()) {
        PLOG(ERROR) << "Failed to start network for announcer controller";
        StateChanged(StateEvent::DOWN);
        return;
    }
    announcerController.Start();
    CHECK_NE(pipe(Server::select_fd), -1);

    // Make read end nonblocking
    int flags, signal_pipe_select_fd;

    CHECK_NE(flags = fcntl(Server::select_fd[0], F_GETFL), -1);
    flags |= O_NONBLOCK;
    CHECK_NE(fcntl(Server::select_fd[0], F_SETFL, flags), -1);

    // Make write end nonblocking
    CHECK_NE(flags = fcntl(Server::select_fd[1], F_GETFL), -1);
    flags |= O_NONBLOCK;
    CHECK_NE(fcntl(Server::select_fd[1], F_SETFL, flags), -1);
    signal_pipe_select_fd = Server::select_fd[0];

    PortListener port_listener(receive_buffer_size_);

    int socket_fd;

    if (!port_listener.Listen(listen_ipv4_address_, port_, &socket_fd)) {
        StateChanged(StateEvent::DOWN);
        announcerController.Stop();
        return;
    }
    port_listener.MakeSocketNonBlocking(socket_fd);

    int ssl_fd;

    if (!port_listener.Listen(listen_ipv4_address_, ssl_port_, &ssl_fd)) {
        StateChanged(StateEvent::DOWN);
        announcerController.Stop();
        return;
    }
    port_listener.MakeSocketNonBlocking(ssl_fd);

    int ipv6_socket_fd;

    if (!port_listener.Listen(listen_ipv6_address_, port_, &ipv6_socket_fd)) {
        StateChanged(StateEvent::DOWN);
        announcerController.Stop();
        return;
    }
    port_listener.MakeSocketNonBlocking(ipv6_socket_fd);

    int ipv6_ssl_socket_fd;
    if (!port_listener.Listen(listen_ipv6_address_, ssl_port_, &ipv6_ssl_socket_fd)) {
        StateChanged(StateEvent::DOWN);
        announcerController.Stop();
        return;
    }
    port_listener.MakeSocketNonBlocking(ipv6_ssl_socket_fd);

    // Create the pool of threads that will handle incoming connections
    for (int i = 0; i < kInitialThreadPoolSize; i++) {
        SpawnWorkerThread();
        SpawnResponseThread();
        SpawnCmdIngestThread();
    }

    // Start up the worker thread for p2p operations
    p2p_request_manager_.Start();


#ifdef LLDP_ENABLED
    // Create LLDP announcer and start thread
    std::vector<std::string> ifaces = {LldpAnnouncer::kETH0_NAME, LldpAnnouncer::kETH1_NAME};
    LldpAnnouncer lldp_announcer(ifaces, network_interfaces_, device_information_);
    Thread lldp_announcer_thread(&lldp_announcer);
    lldp_announcer_thread.start(false);
#endif

    int connection_fd, ready;
    Server::epollfd = epoll_create(max_events);
    if (Server::epollfd == -1) {
        PLOG(ERROR) << "epoll_create failed to CREATE";//NO_SPELL
        StateChanged(StateEvent::DOWN);
        SendPoisonPills();
#ifdef LLDP_ENABLED
        LOG(INFO) << "Closing LLDP announcer...";
        pthread_join(lldp_announcer_thread.getThreadId(), NULL);
#endif
        p2p_request_manager_.Stop();
        announcerController.Stop();
        return;
    }
    //Add all listening sockets to be monitored
    int fds[] = {signal_pipe_fd_, socket_fd, ssl_fd,
                 ipv6_socket_fd, ipv6_ssl_socket_fd};
    int nfds = sizeof(fds)/sizeof(fds[0]);
    for (int i = 0; i < nfds; ++i) {
        if (!RegisterToEpoll(fds[i])) {
            PLOG(ERROR) << "1. Failed to Register to Epoll";//NO_SPELL
            StateChanged(StateEvent::DOWN);
            SendPoisonPills();
#ifdef LLDP_ENABLED
            LOG(INFO) << "Joining LLDP announcer...";
            pthread_join(lldp_announcer_thread.getThreadId(), NULL);
#endif
            p2p_request_manager_.Stop();
            announcerController.Stop();
            return;
        }
        VLOG(1) << "Socket fd = " << fds[i];//NO_SPELL
    }
    if (!RegisterToEpoll(signal_pipe_select_fd)) {
        PLOG(ERROR) << "2. Failed to Register to Epoll";//NO_SPELL
        StateChanged(StateEvent::DOWN);
        SendPoisonPills();
#ifdef LLDP_ENABLED
        pthread_join(lldp_announcer_thread.getThreadId(), NULL);
#endif
        p2p_request_manager_.Stop();
        announcerController.Stop();
        return;
    }
    VLOG(1) << "Socket fd = " << signal_pipe_select_fd;//NO_SPELL
    ScheduleCompaction scheduleCompaction(keyValueStore_);
    Thread scheduleCompactionThread(&scheduleCompaction);
#ifndef PRODUCT_X86
    signal(SIGSEGV, segFaultHandler);
#endif
    bool status;
    status = true;
    int i = 0;
    unsigned int number_connections;
    int blockTime = 2*60*1000; // 2 min
    struct epoll_event *events = new struct epoll_event[max_events];
    for (int i = 0; i< max_events; i++) {
        memset(&events[i], 0, sizeof(struct epoll_event));
    }
    StateChanged(StateEvent::STARTED);
    // Start accepting connections
    while (status) {
        ready = epoll_wait(Server::epollfd, events, max_events, blockTime);

        if (ready == -1 && errno == EINTR) {
            PLOG(ERROR) << "epoll wait was interrupted ";//NO_SPELL
        }
        if (ready == 0) {
            // Make epoll_wait blocking 10 seconds
            if (!kineticd_idle) {
                kineticd_idle = true;
                scheduleCompactionThread.start(true);
            }
            blockTime = 10000;
        } else {
            blockTime = 10000; // 1s = 1000ms
        }
        Server::connection_map.CloseConnections();
        while (ready >0) {
            ready--;
            i = ready;
            if (events[i].data.fd == signal_pipe_fd_) {
                VLOG(1) << "Received signal; shutting down server";
                if (close(signal_pipe_fd_) != 0) {
                    LOG(ERROR) << "Failed to close read end of signal pipe";
                }
                SendPoisonPills();
#ifdef LLDP_ENABLED
                LOG(INFO) << "Joining LLDP announcer...";
                pthread_join(lldp_announcer_thread.getThreadId(), NULL);
                LOG(INFO) << "LLDP announcer joined";
#endif
                p2p_request_manager_.Stop();
                announcerController.Stop();
                status = false;
                goto EXIT;
            }
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                //If Peer just do the close(socket) {
                //    An error will detect by epoll on this fd,
                //    We can Close the connection here without
                //    sending unsolicited message
                //}
                //If  Peer just do the close(socket);
                //If Peer does shutdown(socket, SHUT_WR) or
                //                 shutdown(socket, SHUT_RDWR {
                //    This will detect by epoll as data ready with EOF when
                //    we read the socket.
                //     Case WR:
                //         Peer shutdown the Transmit, Receive still available.
                //         We can send Status normally if there is one.
                //         then send unsolicited message.
                //         then close socket.
                //     Case RDWR:
                //         Peer shutdown both Transmit & Receive.
                //         If there is Status to be sent, our transmit will
                //         return 0 byte.
                //         We just close the socket without the need to send
                //         unsolicited message.
                //     Case RD:
                //         Peer shutdown Receive.
                //         Same as case RDWR.
                //}

                LOG(INFO) <<  strerror(errno) << ": Epoll error or socket is not ready";//NO_SPELL
                connection_handler_.CloseConnection(events[i].data.fd);
                continue;
            } else if (events[i].events & EPOLLRDHUP) {
             // Stream socket peer closed connection,
             // or shut down writing half of connection.
                LOG(INFO) << "Closed connection EPOLLRDHUP " <<  events[i].data.fd;//NO_SPELL
                connection_handler_.CloseConnection(events[i].data.fd);
                continue;
            }
            if (events[i].data.fd == signal_pipe_select_fd) {
                //LOG(INFO) << "Received select signal";
                char buf;
                ssize_t status;
                status = read(signal_pipe_select_fd, &buf, 1);
                CHECK_NE(status, -1);
                continue;
            }
            if ((events[i].data.fd == socket_fd) ||
                (events[i].data.fd == ipv6_socket_fd) ||
                (events[i].data.fd == ssl_fd) ||
                (events[i].data.fd == ipv6_ssl_socket_fd)) {
              while (true) {
                status = AcceptConnection(events[i].data.fd, &connection_fd);
                int saveErrno = errno;
                if (status == false) {
                    if (saveErrno == EAGAIN || saveErrno == EWOULDBLOCK) {
                        status = true;
                        errno = 0;
                        LOG(INFO) << "End of connection requests";
                    } else {
                        LOG(ERROR) << strerror(saveErrno) << ", fd = " << events[i].data.fd << ", saveErrno = " << saveErrno;
                        LOG(ERROR) << "***** Connection error";
                    }
                    break;
                }
                if (status == false) {
                    break;
                }
                std::shared_ptr<Connection> connection(new Connection(connection_fd));

                if (events[i].data.fd == ssl_fd ||
                    events[i].data.fd == ipv6_ssl_socket_fd) {
                    connection->SetUseSSL(true);
                    connection->SetSSl(SetSSL(connection_fd));
                    if (connection->ssl() == NULL) {
                        break;
                        //continue;
                    }
                }
                port_listener.MakeSocketNonBlocking(connection_fd);
                //Look for an idle connection and close it.
                //if (open_connection_counter_.GetNumberOpenConnections() >=
                if (Server::connection_map.TotalConnectionCount(true) >=
                    limits_.max_connections()) {
                    std::shared_ptr<Connection> close_connection = Server::connection_map.FindConnectionToClose();
                    if (close_connection != nullptr) {
                        close_connection->SetState(ConnectionState::SHOULD_BE_CLOSED);
                        close_connection->SetShouldBeClosed(true);
                        LOG(INFO) << "Found LEAST RECENTLY ACCESSED Connection " << close_connection->fd();
                        connection_handler_.TooManyConnections(close_connection.get());
                        connection_handler_.CloseConnection(close_connection->fd(), true);
                        //Server::connection_map.CopyToWillBeClosed(close_connection->fd());
                    }
                }
                connection->SetId(-1);
                Server::connection_map.AddNewConnection(connection);
                if (!RegisterToEpoll(connection_fd)) {
                    PLOG(ERROR) << "Failed to Register to Epoll fd " << connection_fd;//NO_SPELL
                    status = false;
                    break;
                }
                statistics_manager_.IncrementOpenConnections();
                number_connections = Server::connection_map.TotalConnectionCount(true);
                LOG(INFO) << " CURRENT NUMBER OF CONNECTIONS " << number_connections;
                if (number_connections > max_connections_so_far_) {
                    max_connections_so_far_= number_connections;
                    //LOG(INFO) << "Number of CONNECTIONS seen so far " << number_connections;
                }
                if (connection->state() != ConnectionState::SHOULD_BE_CLOSED) {
                    connection_handler_.SendSignOnMessage(connection);
                }
              }
              if (status == false) {
                  break;
              }
            } else {
                //Data from Clients
                //Disarm kineticd idle;
                // Only RemoveFromEpoll if we are in state where user_store has been loaded,
                // successfully or otherwise
                while (!IsCommandReady()) {
                    sleep(1);
                }
                kineticd_idle = false;
                if (RemoveFromEpoll(events[i].data.fd)) {
                    std::shared_ptr<Connection> connection =  Server::connection_map.GetConnection(events[i].data.fd);
                    if (connection != nullptr) {
                        if (connection->should_be_closed() ||
                            connection->state() ==  ConnectionState::SHOULD_BE_CLOSED) {
                            connection->SetState(ConnectionState::SHOULD_BE_CLOSED);
                            connection_handler_.Enqueue(connection);
                        } else if (connection->state() == ConnectionState::IDLE) {
                            connection->SetState(ConnectionState::BUSY);
                            connection_handler_.Enqueue(connection);
                        }
                    }
                } else {
                    LOG(INFO) << "FAILED TO REMOVE";
                }
            }
        }
    }
 EXIT:
    delete[] events;
    // Close all data and listening sockets
    LOG(INFO) << "Finished accept loop; closing listen sockets";

    std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
    it = Server::connection_map.begin();
    while (it != Server::connection_map.end()) {
        int connFd = it->second->fd();
        connection_handler_.CloseConnection(connFd);
        it = Server::connection_map.begin();
    }
    status = port_listener.Close(socket_fd) &&
             port_listener.Close(ssl_fd) &&
             port_listener.Close(ipv6_socket_fd) &&
             port_listener.Close(ipv6_ssl_socket_fd);
    if (close(signal_pipe_select_fd) == -1) {
        status = false;
    }
    close(Server::epollfd);
    StateChanged(StateEvent::DOWN);
    return;
}

void Server::SpawnWorkerThread() {
    VLOG(1) << "Spawning a new worker thread";
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t STK_SIZE = 8 * 1024 * 1024;
    CHECK(!pthread_attr_setstacksize(&attr, STK_SIZE));
    CHECK(!pthread_create(&thread, &attr, connection_thread_worker, &connection_handler_));
    worker_threads_.push_back(thread);
    CHECK(!pthread_attr_destroy(&attr));
}

void Server::SpawnResponseThread() {
    LOG(INFO) << "Spawning a new repsonse thread";
    pthread_t thread;
    CHECK(!pthread_create(&thread, NULL, connection_response_worker, &connection_handler_));
    response_threads_.push_back(thread);
}

void Server::SpawnCmdIngestThread() {
    VLOG(1) << "Spawning a command ingest  worker thread";
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t STK_SIZE = 4 * 1024 * 1024;
    CHECK(!pthread_attr_setstacksize(&attr, STK_SIZE));
    CHECK(!pthread_create(&thread, NULL, command_ingest_worker, &connection_handler_));
    cmd_ingest_threads_.push_back(thread);
    CHECK(!pthread_attr_destroy(&attr));
}

bool Server::AcceptConnection(int socket_fd, int *fd) {
    struct sockaddr_storage client_address;
    socklen_t client_address_length = sizeof(struct sockaddr_storage);
    memset(&client_address, 0, client_address_length);

    int connection_fd = accept(socket_fd, (struct sockaddr*) &client_address,
            &client_address_length);
    *fd = connection_fd;
    if (connection_fd == -1) {
        return false;
    }

    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    if (int res = getnameinfo((struct sockaddr*) &client_address, client_address_length, host,
            sizeof(host), service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        LOG(ERROR) << "Could not get name info for client connection: " << gai_strerror(res);
    } else {
        LOG(INFO) << "Got a connection from " << string(host) << " on " << string(service) << " fd " << connection_fd;
        VLOG(1) << "Connection fd =  " << connection_fd;//NO_SPELL
    }
    int val = 1;
#ifdef TCP_CORK
    if (setsockopt(connection_fd, IPPROTO_TCP, TCP_CORK, &val, sizeof(val))) {
        PLOG(WARNING) << "Unable to set TCP_CORK";//NO_SPELL
    }
#endif
    // Enable TCP_NODELAY for all incoming connections (disable Nagel)
    if (setsockopt(connection_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val))) {
            PLOG(WARNING) << "Unable to set TCP_NODELAY";//NO_SPELL
    }

    return true;
}

void Server::SendPoisonPills() {
    // Send poison pills to tell all the little worker bees
    // to stop
    VLOG(1) << "Sending poison pills to " << worker_threads_.size() << " " << "Workers";
    VLOG(1) << "Sending poison pills to " << response_threads_.size() << " " << "Responsers";
    connection_handler_.SendPoisonPills(worker_threads_.size(), response_threads_.size(), cmd_ingest_threads_.size());
    connection_handler_.RemoveAllFromPendingMap();

    for (std::list<pthread_t>::iterator it = cmd_ingest_threads_.begin();
        it != cmd_ingest_threads_.end(); ++it) {
        LOG(INFO) << "Joining cmd ingest thread... " << *it;
        CHECK(!pthread_join(*it, NULL));
        LOG(INFO) << "Cmd ingest thread joined " << *it;
    }

    for (std::list<pthread_t>::iterator it = worker_threads_.begin();
        it != worker_threads_.end(); ++it) {
        LOG(INFO) << "Joining worker thread... " << *it;
        CHECK(!pthread_join(*it, NULL));
        LOG(INFO) << "Worker thread joined " << *it;
    }

    for (std::list<pthread_t>::iterator it = response_threads_.begin();
        it != response_threads_.end(); ++it) {
        LOG(INFO) << "Joining response thread... " << *it;
        CHECK(!pthread_join(*it, NULL));
        LOG(INFO) << "Response thread joined " << *it;
    }
    LOG(INFO) << "All threads joined";
    LOG(INFO) << "Closing dbase...";
    Server::_shuttingDown = true;
    CloseDB();
    LOG(INFO) << "Closed dbase";
}

bool Server::IsSupportable(Message_AuthType authType, const Command& command, string& stateName) {
    CHECK(!pthread_mutex_lock(&currentStateMutex_));
    bool supportable = currentState_->IsSupportable(authType,
                                      command.header().messagetype());
    stateName = currentState_->GetName();
    CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    return supportable;
}
bool Server::IsSupportable(const Message& msg, const Command& command, string& stateName) {
    CHECK(!pthread_mutex_lock(&currentStateMutex_));
    bool supportable = currentState_->IsSupportable(msg.authtype(),
                                      command.header().messagetype());
    stateName = currentState_->GetName();
    CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    return supportable;
}

bool Server::IsSupportable(const Message_AuthType& authType, const Command_MessageType& cmdType,
                           string& stateName, bool lock) {
    if (lock) {
        CHECK(!pthread_mutex_lock(&currentStateMutex_));
    }
    bool supportable = currentState_->IsSupportable(authType, cmdType);
    stateName = currentState_->GetName();
    if (lock) {
        CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    }
    return supportable;
}

bool Server::IsClusterSupportable() {
    CHECK(!pthread_mutex_lock(&currentStateMutex_));
    bool supportable = currentState_->IsClusterSupportable();
    CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    return supportable;
}

bool Server::IsHmacSupportable() {
    CHECK(!pthread_mutex_lock(&currentStateMutex_));
    bool supportable = currentState_->IsSupportable(proto::Message_AuthType_HMACAUTH);
    CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    return supportable;
}

bool Server::IsCommandReady() {
    CHECK(!pthread_mutex_lock(&currentStateMutex_));
    bool command_ready = currentState_->ReadyForValidation();
    CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    return command_ready;
}

/***
* int Server::StateChanged(StateEvent event, bool success, void* data)
* Params:
*    event:  StateEvent
*    success:  bool to indicate the preprocess was successful or not
*    data: *void points to data which is set to the new state
* Returns:
*    int:  0 for state unchanged, 1 for state moved
***/
int Server::StateChanged(StateEvent event, bool success, void* data, bool lock) {
    int res = 0;
    if (lock) {
        CHECK(!pthread_mutex_lock(&currentStateMutex_));
    }
    KineticState* nextState = currentState_->GetNextState(event, success);

    if (nextState) {
        KineticState* prevState = currentState_;
        currentState_ = nextState;
        prevState->DelPrevState();
        currentState_->SetPrevState(prevState);
        currentState_->SetData(data);
        res = 1;
        LOG(INFO) << "========== In " << currentState_->GetName();
    }

    if (lock) {
        CHECK(!pthread_mutex_unlock(&currentStateMutex_));
    }
    return res;
}

/***
* int Server::SupportableStateChanged
* Params:
*    event: StateEvent
*    authType: Auth type checked against IsSupportable()
*    cmdType: Command type checked against IsSupportable()
*    command_response: pointer to response modified if IsSupportable() is false
* Returns:
*    int:  0 for state unchanged, 1 for state moved
***/
int Server::SupportableStateChanged(StateEvent event,
                                    Message_AuthType authType,
                                    Command_MessageType cmdType,
                                    proto::Command *command_response,
                                    void* data) {
    int ret = -1;

    CHECK(!pthread_mutex_lock(&currentStateMutex_));
    std::string state_name;
    if (!IsSupportable(authType, cmdType, state_name, false)) {
        // command is not supportable
        if (GetStateEnum() == StateEnum::HIBERNATE) {
            command_response->mutable_status()->set_code(Command_Status_StatusCode_HIBERNATE);
        } else {
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
        }

        command_response->mutable_status()->set_statusmessage(std::string("Drive is in ") + state_name);
    } else {
        // have state lock and command is still supportable
        ret = StateChanged(event, true, data, false);
    }
    CHECK(!pthread_mutex_unlock(&currentStateMutex_));

    return ret;
}

/***
* Factory Method for creating announcers.
* Currently, there's only one concrete class for AnnouncerInterface so this will always
* create a vector of MulticastAnnouncers.
*
* In the future when another concrete class is added, this method should be modified
* to create different concrete announcers based on parameter(s) provided.
***/
void Server::CreateAnnouncers(std::vector<std::unique_ptr<AnnouncerInterface>> &announcers) {
    std::string vendor_file;
    std::string ipv6_vendor_file;
    std::vector<DeviceNetworkInterface> interfaces;

    announcers.clear();

    // This is a shared pointer because each announcer needs a reference to it
    std::shared_ptr<GetLogMulticastor> multicastor(new GetLogMulticastor(
                                                        port_,
                                                        ssl_port_,
                                                        network_interfaces_,
                                                        device_information_));

    network_interfaces_.GetExternallyVisibleNetworkInterfaces(&interfaces);

    // Loop over interfaces creating an announcer for each one and adding it to the vector
    for (std::vector<DeviceNetworkInterface>::iterator it = interfaces.begin();
         it != interfaces.end(); ++it) {
        vendor_file = FormatVendorOptions(kVendorOptionsFile, FLAGS_vendor_option_path, it->name);
        ipv6_vendor_file = FormatVendorOptions(kIPv6VendorOptionsFile, FLAGS_vendor_option_path,
                                               it->name);

        announcers.push_back(
            std::unique_ptr<AnnouncerInterface> (new MulticastAnnouncer(
                                                    it->name,
                                                    vendor_file,
                                                    ipv6_vendor_file,
                                                    multicastor,
                                                    network_interfaces_)));
    }
}

std::string Server::FormatVendorOptions(const std::string &file, const std::string &path,
                                        const std::string &name) {
    std::stringstream ss;
    ss << path << file << name << kVendorFileExtension;
    return ss.str();
}

} // namespace kinetic
} // namespace seagate
} // namespace com
