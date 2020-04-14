#include "connection_time_handler.h"
namespace com {
namespace seagate {
namespace kinetic {
using namespace std::chrono; //NOLINT
using std::chrono::milliseconds; //NOLINT

/////////////////////////////////////
///----Connection Time Handler-----
/// See header file for documentation
/// *Note: Ignoring Lint protocol on
///  some short methods. Allowing for
///  inline speed
/////////////////////////////////////
std::chrono::milliseconds ConnectionTimeHandler::max_response_time_ = std::chrono::milliseconds{0}; //NOLINT
std::chrono::milliseconds ConnectionTimeHandler::maxTime_in_queue_ = std::chrono::milliseconds{0};  //NOLINT
std::chrono::milliseconds ConnectionTimeHandler::max_start_end_batch_rcv_time_ = std::chrono::milliseconds{0}; //NOLINT
std::chrono::milliseconds ConnectionTimeHandler::max_start_end_batch_response_time_ = std::chrono::milliseconds{0}; //NOLINT

ConnectionTimeHandler::ConnectionTimeHandler(
    uint64_t timeOut,
    uint64_t timeQuanta):
    time_out_(timeOut),
    time_quanta_(timeQuanta),
    expired_(false),
    interrupt_(false),
    started_(false) {
    if (time_quanta_ == 0) {
        time_quanta_ = DEFAULT_TIME_QUANTUM;
    }
}

ConnectionTimeHandler::~ConnectionTimeHandler() {}

bool ConnectionTimeHandler::IsTimeout() {
    std::chrono::high_resolution_clock::time_point tNow = high_resolution_clock::now();
    std::chrono::milliseconds time_span = duration_cast<milliseconds>(tNow - time_queued_);
    return ((time_out_ > 0) && ((uint64_t) time_span.count() > time_out_));
}

bool ConnectionTimeHandler::ShouldBeInterrupted() {
    std::chrono::high_resolution_clock::time_point tNow = high_resolution_clock::now();
    std::chrono::milliseconds time_span = duration_cast<milliseconds>(tNow - time_op_start_);
    return ((time_quanta_ > 0) && ((uint64_t) time_span.count() > time_quanta_));
}

bool ConnectionTimeHandler::TimeElapsedGTMax() {
    std::chrono::milliseconds time_span =
        duration_cast<milliseconds>(time_responded_ - time_dequeued_);
    return time_span.count() > max_response_time_.count();
}

bool ConnectionTimeHandler::TimeInQueueGTMax() {
    std::chrono::milliseconds time_span =
        duration_cast<milliseconds>(time_dequeued_ - time_queued_);
    return time_span.count() > maxTime_in_queue_.count();
}

void ConnectionTimeHandler::SetNewMaxResponse() {
    max_response_time_ = duration_cast<milliseconds>(time_responded_ - time_dequeued_);
}

void ConnectionTimeHandler::SetNewMaxStartEndBatchResp(std::chrono::milliseconds time) { max_start_end_batch_response_time_ = time; }

std::chrono::milliseconds ConnectionTimeHandler::GetMaxStartEndBatchResp() { return max_start_end_batch_response_time_; }

void ConnectionTimeHandler::SetNewMaxStartEndBatchRcv(std::chrono::milliseconds time) { max_start_end_batch_rcv_time_ = time; }

std::chrono::milliseconds ConnectionTimeHandler::GetMaxStartEndBatchRcv() { return max_start_end_batch_rcv_time_; }

void ConnectionTimeHandler::SetNewMaxQueueTime() {
    maxTime_in_queue_ = duration_cast<milliseconds>(time_dequeued_ - time_queued_);
}

//Getters / Accessors
int ConnectionTimeHandler::GetTimeElapsed() {
    return duration_cast<milliseconds>(time_responded_ - time_dequeued_).count();
}

int ConnectionTimeHandler::GetTimeProcessElapsed() {
    return duration_cast<milliseconds>(time_processed_ - time_dequeued_).count();
}

std::chrono::milliseconds ConnectionTimeHandler::GetResponseTimeForOperation() {
    std::chrono::milliseconds operation_response_time = duration_cast<milliseconds>(time_responded_ - time_dequeued_);
    return operation_response_time;
}

bool ConnectionTimeHandler::HasMadeProgress() { return started_; }

uint64_t ConnectionTimeHandler::GetTimeout() { return time_out_; }

uint64_t ConnectionTimeHandler::GetTimeQuanta() { return time_quanta_; }

std::chrono::high_resolution_clock::time_point ConnectionTimeHandler::GetDequeuedTime() { return time_dequeued_; }//NOLINT

std::chrono::high_resolution_clock::time_point ConnectionTimeHandler::GetEnqueuedTime() { return time_queued_; }//NOLINT

std::chrono::milliseconds ConnectionTimeHandler::GetMaxResponseTime() { return max_response_time_; }//NOLINT

std::chrono::milliseconds ConnectionTimeHandler::GetMaxQueueTime() { return maxTime_in_queue_; }//NOLINT

bool ConnectionTimeHandler::GetExpired() { return expired_; }

bool ConnectionTimeHandler::GetInterrupt() { return interrupt_; }

bool ConnectionTimeHandler::GetStarted() { return started_; }

void ConnectionTimeHandler::SetTimeQueued(std::chrono::high_resolution_clock::time_point qTime) { time_queued_ = qTime; }

void ConnectionTimeHandler::SetTimeOpStart() { time_op_start_ = high_resolution_clock::now(); }

void ConnectionTimeHandler::SetTimeDequeued() { time_dequeued_ = high_resolution_clock::now(); }

void ConnectionTimeHandler::SetTimeResponded() { time_responded_ = high_resolution_clock::now(); }

std::chrono::high_resolution_clock::time_point ConnectionTimeHandler::GetTimeResponded() { return time_responded_; }//NOLINT

void ConnectionTimeHandler::SetTimeProcessed() { time_processed_ = high_resolution_clock::now(); }

void ConnectionTimeHandler::SetTimeout(uint64_t value) {
    time_out_ = value;
}

void ConnectionTimeHandler::SetTimeQuanta(uint64_t value) {
    (value > 0) ? (time_quanta_ = value):(time_quanta_ = DEFAULT_TIME_QUANTUM);
}

void ConnectionTimeHandler::SetExpired() { expired_ = true; }

void ConnectionTimeHandler::SetInterrupt() { interrupt_ = true; }

void ConnectionTimeHandler::SetStarted() { started_ = true; }

void ConnectionTimeHandler::ResetExpired() { expired_ = false; }

void ConnectionTimeHandler::ResetInterrupt() { interrupt_ = false; }

void ConnectionTimeHandler::ResetStarted() { started_ = false; }

void ConnectionTimeHandler::ResetMaxResponseTime() { max_response_time_= std::chrono::milliseconds(0); }//NOLINT

void ConnectionTimeHandler::ResetMaxQueueTime() { maxTime_in_queue_= std::chrono::milliseconds(0); }//NOLINT

} // namespace kinetic
} // namespace seagate
} // namespace com
