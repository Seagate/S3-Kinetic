#ifndef KINETIC_CONNECTION_MAP_H_
#define KINETIC_CONNECTION_MAP_H_

#include <map>
#include <memory>
#include <unordered_map>

#include "limits.h"
#include "connection.h"
#include "util/mutexlock.h"
#include "kinetic/common.h"
#include "glog/logging.h"
#include "connection_handler.h"

using namespace std; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

using namespace com::seagate::kinetic;//NOLINT

class ConnectionMap {
    public:
    explicit ConnectionMap();
    ~ConnectionMap();
    void AddNewConnection(std::shared_ptr<Connection> connection);
    bool RemoveConnection(int fd);
    void ValidateConnection(int fd);
    void MoveToWillBeClosed(int fd);
    std::shared_ptr<Connection> GetConnection(int fd);
    size_t TotalConnectionCount(bool only_active_connections = false);
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator begin();
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator end();
    std::shared_ptr<Connection> FindConnectionToClose();
    void SetConnectionHandler(ConnectionHandler* connHandler) {
        connectionHandler_ = connHandler;
    }
    int SizeWillBeClosed();
    void CloseConnections();

    private:
    port::Mutex mu_;
    port::CondVar cv_;
    ConnectionHandler* connectionHandler_;

    // If the provisional list is greater than, or equal to, this threshold then
    // least recently accessed will only look within the provisional_connections_ list
    static const unsigned PROVISIONAL_EXCLUSIVITY_THRESHOLD = 20;
    // Connections that have been created but haven't sent a valid command
    std::unordered_map<int, std::shared_ptr<Connection>> provisional_connections_;
    // Connections that have been created and sent valid commands
    std::unordered_map<int, std::shared_ptr<Connection>> established_connections_;
    // connections that will be closed.
    std::unordered_map<int, std::shared_ptr<Connection>> will_be_closed_connections_;

    std::shared_ptr<Connection> FindLeastRecentlyAccessedConnection(bool check_established);
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_CONNECTION_MAP_H_
