#include "connection_map.h"

namespace com {
namespace seagate {
namespace kinetic {

ConnectionMap::ConnectionMap()
    : cv_(&mu_) {
}

ConnectionMap::~ConnectionMap() {
  // DESTROY ALL CONNECTIONS
}

void ConnectionMap::AddNewConnection(std::shared_ptr<Connection> connection) {
    // Create new connection and add it to the list
    MutexLock lock(&mu_);
    provisional_connections_.insert(std::pair<int, std::shared_ptr<Connection>> (connection->fd(), connection));
}

bool ConnectionMap::RemoveConnection(int fd) {
    //Find that connection, remove it from the list
    MutexLock lock(&mu_);
    bool erase_success = provisional_connections_.erase(fd);
    if (!erase_success) {
        erase_success = established_connections_.erase(fd);
    }
    return erase_success;
}

void ConnectionMap::ValidateConnection(int fd) {
    //Move connection from preliminary to validated
    MutexLock lock(&mu_);
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
    it = provisional_connections_.find(fd);
    if (it != provisional_connections_.end()) {
        established_connections_.insert(std::pair<int, std::shared_ptr<Connection>> (it->first, it->second));
        provisional_connections_.erase(fd);
    }
}

std::shared_ptr<Connection> ConnectionMap::GetConnection(int fd) {
    //Return the connection with that fd
    MutexLock lock(&mu_);
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
    it = provisional_connections_.find(fd);
    if (it == provisional_connections_.end()) {
        it = established_connections_.find(fd);
        if (it == established_connections_.end()) {
            //return null pointer if you can't find it
            return nullptr;
        }
    }
    return it->second;
}

size_t ConnectionMap::TotalConnectionCount(bool only_active_connections) {
    MutexLock lock(&mu_);
    if (only_active_connections) {
        return (provisional_connections_.size()+established_connections_.size());
    } else {
        size_t active_connection_count = 0;
        std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
        for (it = provisional_connections_.begin();
            it != provisional_connections_.end(); ++it) {
            if (it->second->state() != ConnectionState::SHOULD_BE_CLOSED && it->second->state() != ConnectionState::CLOSED) {
                active_connection_count++;
            }
        }
        for (it = established_connections_.begin();
            it != established_connections_.end(); ++it) {
            if (it->second->state() != ConnectionState::SHOULD_BE_CLOSED && it->second->state() != ConnectionState::CLOSED) {
                active_connection_count++;
            }
        }
        return active_connection_count;
    }
}

std::unordered_map<int, std::shared_ptr<Connection>>::iterator ConnectionMap::begin() {
    if (provisional_connections_.begin() != provisional_connections_.end()) {
        return provisional_connections_.begin();
    } else {
        return established_connections_.begin();
    }
}

std::unordered_map<int, std::shared_ptr<Connection>>::iterator ConnectionMap::end() {
    return provisional_connections_.end();
}


int ConnectionMap::SizeWillBeClosed() {
    MutexLock lock(&mu_);
    int n = will_be_closed_connections_.size();
    return n;
}

void ConnectionMap::MoveToWillBeClosed(int fd) {
    MutexLock lock(&mu_);
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
    it = provisional_connections_.find(fd);
    if (it != provisional_connections_.end()) {
        will_be_closed_connections_.insert(std::pair<int, std::shared_ptr<Connection>> (it->first, it->second));
        provisional_connections_.erase(fd);
        return;
    }
    it = established_connections_.find(fd);
    if (it != established_connections_.end()) {
        will_be_closed_connections_.insert(std::pair<int, std::shared_ptr<Connection>> (it->first, it->second));
        established_connections_.erase(fd);
    }
}

void ConnectionMap::CloseConnections() {
    MutexLock lock(&mu_);
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
    it = will_be_closed_connections_.begin();
    while (it != will_be_closed_connections_.end()) {
        if ((it->second->state() == ConnectionState::SHOULD_BE_CLOSED || it->second->state() == ConnectionState::CLOSED)) {
            //&& (it->second->get_cmds_count() == 0)) {
            Connection *connection = it->second.get();
            mu_.Unlock();
            connectionHandler_->CloseConnection(connection->fd(), true);
            mu_.Lock();
            will_be_closed_connections_.erase(connection->fd());
            it = will_be_closed_connections_.begin();
            } else {
               ++it;
            }
    }
}

std::shared_ptr<Connection> ConnectionMap::FindConnectionToClose() {
    // Finds the connection to close when the max connection count is reached
    MutexLock lock(&mu_);
    if (provisional_connections_.size() >= PROVISIONAL_EXCLUSIVITY_THRESHOLD) {
        return FindLeastRecentlyAccessedConnection(false);
    } else {
        return FindLeastRecentlyAccessedConnection(true);
    }
}

std::shared_ptr<Connection> ConnectionMap::FindLeastRecentlyAccessedConnection(bool check_established) {
    std::shared_ptr<Connection> leastRecentlyAccessed = nullptr;
    std::unordered_map<int, std::shared_ptr<Connection>>::iterator it;
    // Find the least recently accessed connection in the provisional list
    for (it = provisional_connections_.begin();
        it != provisional_connections_.end(); ++it) {
        if (it->second->state() != ConnectionState::SHOULD_BE_CLOSED && it->second->state() != ConnectionState::CLOSED) {
            if (leastRecentlyAccessed == nullptr || (leastRecentlyAccessed->most_recent_access_time() > it->second->most_recent_access_time())) {
                leastRecentlyAccessed = it->second;
            }
        }
    }

    // If prompted, find the least recently accessed connection across both established and provisional lists
    if (check_established) {
        for (it = established_connections_.begin();
            it != established_connections_.end(); ++it) {
            if (it->second->state() != ConnectionState::SHOULD_BE_CLOSED && it->second->state() != ConnectionState::CLOSED) {
                if (leastRecentlyAccessed == nullptr || (leastRecentlyAccessed->most_recent_access_time() > it->second->most_recent_access_time())) {
                    leastRecentlyAccessed = it->second;
                }
            }
        }
    }

    return leastRecentlyAccessed;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
