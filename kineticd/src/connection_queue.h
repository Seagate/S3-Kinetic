#ifndef KINETIC_CONNECTION_QUEUE_H_
#define KINETIC_CONNECTION_QUEUE_H_

#include <queue>
#include <vector>
#include <utility>
#include <stdint.h>
#include <tuple>
#include <arpa/inet.h>
#include <memory>
#include "kinetic/message_stream.h"
#include "pthread.h"
#include "kinetic.pb.h"
#include "connection_time_handler.h"
#include "log_ring_buffer.h"
#include "connection.h"
#include "util/mutexlock.h"
#include "port/port_posix.h"

using namespace leveldb; // NOLINT

/* This class implements a threadsafe blocking queue holding file descriptors
 * for incoming connections.
 */

namespace com {
namespace seagate {
namespace kinetic {

using namespace proto;//NOLINT
using namespace com::seagate::kinetic;//NOLINT
using ::kinetic::MessageStreamInterface;
using ::kinetic::IncomingValueInterface;




//////////////////////////////////////
// Used by Priority Queue
// To sort contents for dequeue
struct ConnectionComparator {
    bool operator() (std::shared_ptr<Connection> arg1, std::shared_ptr<Connection> arg2) {
        return  (*arg1) < (*arg2);
    }
};

struct ConnectionResponseComparator {
    bool operator() (std::tuple<std::shared_ptr<Connection>, uint64_t, bool> arg1, std::tuple<std::shared_ptr<Connection>, uint64_t, bool> arg2) {
        return  (*std::get<0>(arg1)) < (*std::get<0>(arg2));
    }
};

class ConnectionQueue {
    public:
    static unsigned int incoming_queue_limit;
    static int thread_workers_idle;
    static pthread_mutex_t mtx_thread_workers_idle;

    ConnectionQueue();
    ~ConnectionQueue();
    void LogStaleEntry(int level);
    void LogLatency(unsigned char flag);
    unsigned char *GetLatencyCauseBitMask(int64_t id);
    void RemoveConnectionLatency(int64_t id);
    std::map<int64_t, unsigned char> *latency_map();
    void clearLatency(int64_t connId) {
        MutexLock lock(&mu_);
        latency_map_[connId] = 0;
    }
    private:
    std::map<int64_t, unsigned char> latency_map_;
    std::map<int, int> stale_entry_map_;
    port::Mutex mu_;

    DISALLOW_COPY_AND_ASSIGN(ConnectionQueue);
};

class ConnectionResponseQueue {
    public:
    static unsigned int outgoing_queue_limit;
    ConnectionResponseQueue();
    ~ConnectionResponseQueue();
    bool Enqueue(std::tuple<std::shared_ptr<Connection>, uint64_t, bool> connection, bool bConditional = false);
    std::tuple<std::shared_ptr<Connection>, uint64_t, bool> Dequeue();
    bool Empty();
    unsigned int Size();
    void WaitUntilEmpty();
    private:
    std::queue<std::tuple<std::shared_ptr<Connection>, uint64_t, bool>> response_connections_;
    port::Mutex mu_;
    port::Mutex wait_until_empty_mu_;
    port::CondVar cv_;

    DISALLOW_COPY_AND_ASSIGN(ConnectionResponseQueue);
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_CONNECTION_QUEUE_H_
