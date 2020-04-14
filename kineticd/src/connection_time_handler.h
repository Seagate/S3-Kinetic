#ifndef KINETIC_CONNECTION_TIME_HANDLER_H_
#define KINETIC_CONNECTION_TIME_HANDLER_H_
#include <chrono>
#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {
using namespace std::chrono; //NOLINT
using std::chrono::milliseconds; //NOLINT

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/// Connection Time Handler
///     -ParentClass / Owner: @ConnectionQueue's Connection Struct
/// -------------------------------------------
/// @Summary:
/// -Responsible for tracking all time related events for a connection
/// -Serves as a member variable within a Connection Struct
/// -Allows the timer to be passed around by reference
/// -Objects that intend to check time related events for a
/// -Connection will use the methods contained within this Class */
/// -------------------------------------------
/// @Member Variables
/// -time_point time_queued_ -- time command placed in connection queue
/// -time_point time_dequeued -- time command dequeued for execution
/// -time_point time_op_start_ -- time operation began executing(!= time of dequeue)
/// -time_responded_ -- time response message was sent to client
/// -chrono::milliseconds max_response_time_ -- largest response time (so far)
/// -chrono::milliseconds maxTime_in_queue_ -- longest time spent in queue(so far)
/// -int time_out_ --  time alloted for command completion by client
/// -int time_quanta_ -- time alloted for command to run before yielding by client
/// -bool expired_ --  bool indicating if command has expired (timed out)
/// -bool interrupt_ -- bool indicating if command has been interrupted (quanta)
/// -bool started_ -- bool indicating command has started, used to check state
///-----------------------------------
class ConnectionTimeHandler {
    public:
    static const uint64_t DEFAULT_TIME_QUANTUM = 20;  // milliseconds

    public:
    static std::chrono::milliseconds max_response_time_;
    static std::chrono::milliseconds maxTime_in_queue_;
    static std::chrono::milliseconds max_start_end_batch_rcv_time_;
    static std::chrono::milliseconds max_start_end_batch_response_time_;
    ConnectionTimeHandler(uint64_t timeOut = 0, uint64_t timeQuanta = DEFAULT_TIME_QUANTUM);
    virtual ~ConnectionTimeHandler();

    /// Check if Connection has timed out (expired)
    /// (if total time elapsed since command enqueued > time_out_)
    /// *NOTE: Commands can timeout regardless of it's progress
    bool IsTimeout();

    /// Check if Connection needs to be interrupted
    /// (if total time elapsed since op start > time_quanta_)
    bool ShouldBeInterrupted();

    /// Check current Response Time vs Max Response Seen So far
    bool TimeElapsedGTMax();

    /// Compare current time spent in queue vs previous max
    /// (time_dequeued_ - time_queued_) > maxTime_in_queue_ ?
    bool TimeInQueueGTMax();

    /// @max_response_time_ = time_responded_ - time_dequeued_
    void SetNewMaxResponse();

    /// @maxTime_in_queue_ = time_dequeued_ - time_queued_
    void SetNewMaxQueueTime();

    void SetNewMaxStartEndBatchResp(std::chrono::milliseconds time);

    std::chrono::milliseconds GetMaxStartEndBatchResp();

    void SetNewMaxStartEndBatchRcv(std::chrono::milliseconds time);

    std::chrono::milliseconds GetMaxStartEndBatchRcv();

    //////////////////////////////////////
    /// Getters / Accessors
    //////////////////////////////////////
    int GetTimeElapsed();
    int GetTimeProcessElapsed();
    bool HasMadeProgress();
    uint64_t GetTimeout();
    uint64_t GetTimeQuanta();
    std::chrono::high_resolution_clock::time_point GetDequeuedTime();
    std::chrono::high_resolution_clock::time_point GetEnqueuedTime();
    std::chrono::milliseconds GetMaxResponseTime();
    std::chrono::milliseconds GetMaxQueueTime();
    std::chrono::milliseconds GetResponseTimeForOperation();
    bool GetExpired();
    bool GetInterrupt();
    bool GetStarted();

    //////////////////////////////////////
    /// Setters / Mutators
    /// -----
    /// The Mutators for the following vars
    /// all use high_resolution_clock::now()
    /// to set the value.
    /// -@time_queued_
    /// -@time_op_start_
    /// -@time_dequeued_
    /// -@time_responded_
    /// -@time_processed_
    //////////////////////////////////////
    void SetTimeQueued(std::chrono::high_resolution_clock::time_point qTime);
    void SetTimeOpStart();
    void SetTimeDequeued();
    void SetTimeResponded();
    std::chrono::high_resolution_clock::time_point GetTimeResponded();
    void SetTimeProcessed();
    void SetTimeout(uint64_t value);
    void SetTimeQuanta(uint64_t value);
    void SetExpired();
    void SetInterrupt();
    void SetStarted();
    void ResetExpired();
    void ResetInterrupt();
    void ResetStarted();
    void ResetMaxResponseTime();
    void ResetMaxQueueTime();

    private:
    std::chrono::high_resolution_clock::time_point time_queued_;
    std::chrono::high_resolution_clock::time_point time_dequeued_;
    std::chrono::high_resolution_clock::time_point time_responded_;
    std::chrono::high_resolution_clock::time_point time_processed_;
    std::chrono::high_resolution_clock::time_point time_op_start_;
    uint64_t time_out_;
    uint64_t time_quanta_;
    bool expired_;
    bool interrupt_;
    bool started_;
};
} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_CONNECTION_TIME_HANDLER_H_
