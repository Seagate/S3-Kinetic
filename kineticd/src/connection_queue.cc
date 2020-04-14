#include "connection_queue.h"

#include "glog/logging.h"
#include "connection_handler.h"
#include "server.h"

namespace com {
namespace seagate {
namespace kinetic {
using proto::Message_AuthType_PINAUTH;

int ConnectionQueue::thread_workers_idle = Server::kInitialThreadPoolSize;
pthread_mutex_t ConnectionQueue:: mtx_thread_workers_idle = PTHREAD_MUTEX_INITIALIZER;
unsigned int ConnectionQueue::incoming_queue_limit = 200;
unsigned int ConnectionResponseQueue::outgoing_queue_limit =  15;

ConnectionQueue::ConnectionQueue() {
}

ConnectionQueue::~ConnectionQueue() {}

void ConnectionQueue::LogStaleEntry(int level) {
    MutexLock lock(&mu_);
    LogRingBuffer::Instance()->recordStaleDataToBuffer(level);
}

void ConnectionQueue::LogLatency(unsigned char flag) {
    MutexLock lock(&mu_);
    typedef std::map<int64_t, unsigned char>::iterator it_type;
    for (it_type iterator = latency_map_.begin(); iterator != latency_map_.end(); iterator++) {
        // iterator->first = key
        // iterator->second = value
        // Repeat if you also want to iterate through the second map.
        latency_map_[iterator->first] = latency_map_[iterator->first] | flag;
    }
}

unsigned char *ConnectionQueue::GetLatencyCauseBitMask(int64_t id) {
    MutexLock lock(&mu_);
    unsigned char *bit_mask;
    bit_mask = &latency_map_[id];
    return bit_mask;
}

void ConnectionQueue::RemoveConnectionLatency(int64_t id) {
    MutexLock lock(&mu_);
    latency_map_.erase(id);
}

std::map<int64_t, unsigned char> *ConnectionQueue::latency_map() {
    return &latency_map_;
}

ConnectionResponseQueue::ConnectionResponseQueue(): cv_(&mu_) {
}

ConnectionResponseQueue::~ConnectionResponseQueue() {
}

bool ConnectionResponseQueue::Enqueue(std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection, bool bConditional) {
    MutexLock lock(&mu_);
    bool success = false;

    if (bConditional) {
        while (response_connections_.size() >= ConnectionResponseQueue::outgoing_queue_limit && !Server::_shuttingDown) {
            cv_.Wait();
        }
        if (!Server::_shuttingDown) {
            response_connections_.push(connection);
            success = true;
            cv_.Signal();
        }
    } else {
        response_connections_.push(connection);
        cv_.Signal();
    }
    return success;
}
void ConnectionResponseQueue::WaitUntilEmpty() {
    MutexLock specialLock(&wait_until_empty_mu_);
    MutexLock lock(&mu_);
    while (!response_connections_.empty()) {
        cv_.SignalAll();
        cv_.TimedWait();
    }
}

std::tuple<std::shared_ptr<Connection>, uint64_t, bool> ConnectionResponseQueue::Dequeue() {
    MutexLock lock(&mu_);

    while (response_connections_.size() <= 0) {
        cv_.Wait();
    }
    std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection;
    connection = response_connections_.front();
    response_connections_.pop();
    cv_.Signal();
    return connection;
}

bool ConnectionResponseQueue::Empty() {
    int queue_size = response_connections_.size();
    return (queue_size == 0);
}

unsigned int ConnectionResponseQueue::Size() {
    int queue_size = response_connections_.size();
    return queue_size;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
