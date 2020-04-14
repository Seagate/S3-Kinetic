#ifndef KINETIC_SERVER_H_
#define KINETIC_SERVER_H_
#include <queue>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <map>
#include <unordered_map>
#include "kinetic/common.h"

#include "connection_handler.h"
#include "connection_map.h"
#include "kinetic.pb.h"
#include "pthread.h"
#include "device_information_interface.h"
#include "statistics_manager.h"
#include "network_interfaces.h"
#include "p2p_request_manager.h"
#include "kinetic_state.h"
#include "status_interface.h"
#include "runnable_interface.h"
#include "key_value_store_interface.h"
#include "kinetic_alarms.h"
#include "announcer_interface.h"

using namespace com::seagate::common;//NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class Server : public StatusInterface, public RunnableInterface {
    public:
    static const int kInitialThreadPoolSize = 1;
    static int select_fd[2];
    static int epollfd;
    static bool _shuttingDown;
    static bool SetActiveFD(int fd);
    static ConnectionMap connection_map;

    bool RemoveFromEpoll(int fd);
    bool RegisterToEpoll(int fd);
    void ShowCertificate(SSL* ssl);
    SSL * SetSSL(int sslfd);

    Server(
        ConnectionHandler &connection_handler,
        const string &listen_ipv4_address,
        uint port, uint ssl_port,
        const string &listen_ipv6_address,
        int receive_buffer_size,
        int signal_pipe_fd,
        SSL_CTX *ssl_context,
        NetworkInterfaces& network_interfaces,
        DeviceInformationInterface& device_information,
        StatisticsManager& statistics_manager,
        Limits& limits,
        const int heartbeat_frequency_seconds,
        P2PRequestManagerInterface& p2p_request_manager,
        KineticAlarms* kinetic_alarms);
    virtual ~Server() {
        delete currentState_;
        pthread_mutex_destroy(&currentStateMutex_);
    }
    void run();
    void Initialize();

    bool IsSupportable(const Message& msg, const Command& command, string& stateName);
    bool IsSupportable(Message_AuthType authType, const Command& command, string& stateName);
    int StateChanged(StateEvent event, bool success = true, void* data = NULL, bool lock = true);
    int SupportableStateChanged(
        StateEvent event,
        const Message_AuthType authType,
        const Command_MessageType cmdType,
        proto::Command *command_response,
        void* data = NULL);
    bool IsSupportable(const Message_AuthType& authType, const Command_MessageType& cmdType,
                       string& stateName, bool lock = true);
    bool IsClusterSupportable();
    bool IsHmacSupportable();
    bool IsCommandReady();

    void SetSkinnyWaist(SkinnyWaist* skinnyWaist) {
        skinnyWaist_ = skinnyWaist;
    }
    void CloseDB() {
        skinnyWaist_->CloseDB();
    }
    void OpenDB() {
        skinnyWaist_->InitUserDataStore();
    }
    void SetKeyValueStore(KeyValueStoreInterface* keyValueStore) {
        keyValueStore_ = keyValueStore;
    }
    StateEnum GetStateEnum() {
        return currentState_->GetEnum();
    }
    std::string GetStateName() {
        return currentState_->GetName();
    }

    bool IsDown() {
        return currentState_->IsDownState();
    }

    bool HasStarted() {
        return currentState_->IsStarted();
    }

    private:
    void SpawnWorkerThread();
    void SpawnResponseThread();
    void SpawnCmdIngestThread();
    bool AcceptConnection(int socket_fd, int *fd);
    void SendPoisonPills();
    void CreateAnnouncers(std::vector<std::unique_ptr<AnnouncerInterface>> &announcers);
    std::string FormatVendorOptions(
        const std::string &file,
        const std::string &path,
        const std::string &name);

    ConnectionHandler &connection_handler_;
    const std::string listen_ipv4_address_;
    const int port_;
    const int ssl_port_;
    const string listen_ipv6_address_;
    const int receive_buffer_size_;
    const int signal_pipe_fd_;

    SSL_CTX *ssl_context_;
    NetworkInterfaces& network_interfaces_;
    DeviceInformationInterface& device_information_;
    std::list<pthread_t> worker_threads_;
    std::list<pthread_t> response_threads_;
    std::list<pthread_t> cmd_ingest_threads_;
    std::list<std::shared_ptr<Connection>> connections_;
    StatisticsManager& statistics_manager_;
    Limits& limits_;
    unsigned int max_connections_so_far_;
    const int heartbeat_frequency_seconds_;
    P2PRequestManagerInterface& p2p_request_manager_;
    KineticAlarms* kinetic_alarms_;

    //=== Attributes used for State Management
    KineticState* currentState_;
    pthread_mutex_t currentStateMutex_;
    //--- End of attributes used for State Management
    SkinnyWaist* skinnyWaist_;
    KeyValueStoreInterface* keyValueStore_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SERVER_H_
