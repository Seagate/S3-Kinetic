#ifndef KINETIC_AGING_TIMER_H_
#define KINETIC_AGING_TIMER_H_
#include "timer_mutex_guard.h"
#include "timer_handler_interface.h"
#include <pthread.h>
#include <unistd.h>
#include <chrono>
namespace com {
namespace seagate {
namespace kinetic {
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/// AgingTimer                                                                 //
///     -Used By: @Kineticd(Creator) & @ConnectionHandler (arming)             //
/// -------------------------------------------                                //
/// @Summary:                                                                  //
/// - Background Timer Thread which triggers a flush() call once expired       //
///   Triggered only if timer has been armed by Write-Through Put/Delete       //
///   If Mem-Table Compaction is triggered via other components, timer will    //
///   be disarmed                                                              //
/// -------------------------------------------                                //
/// @Member Variables                                                          //
/// - running_ -- is thread alive (started by constructor)                     //
/// - armed_ -- is the Timer armed (Armed by valid WriteThrough Put or Delete) //
/// - sleep_milisec_ -- time thread will sleep each iteration                  //
/// - def_timeout_window_ -- default timeout window if none supplied           //
/// - time_out_ -- cmd supplied timeout (remains 0 until explicitly provided)  //
/// - time_finish_ -- point in time when timer must finish & issue flush       //
/// - thread_start_ -- debug time point for measuring run time                 //
/// - thread_stop_ -- debug time point for measuring run time                  //
/// - thread_ -- the thread tasked with carrying out timer work                //
/// - running_mutex_ -- mutex for @running_                                    //
/// - timer_mutex_ -- mutex for calculation of time points / events            //
/// - armed_mutex_ -- mutex for @armed_                                        //
/// - *msg_proc_ptr_ -- interface to message processor to call Flush()         //
///----------------------------------------------------------------------------//
/////////////////////////////////////////////////////////////////////////////////
class AgingTimer {
    public:
    /// Excluding the Starting & Stopping of the timer (done by kineticd)
    /// ArmTimer() is the only method that must be publicly available
    explicit AgingTimer(TimerHandlerInterface *time_handler);
    ~AgingTimer();
    void StartTimerThread();
    void StopTimerThread();

    /// Set @armed_ = True if not already. Pass supplied time parameter to @UpdateFinishTime()
    /// determines if timer can be constrained. if(!(supplied time > 0)) : pass @def_timeout_window_
    /// Put or Delete Write-Through cmd triggers ConnHandler call to ArmTimer
    /// Command's @timeout field passed for @int64_t time parameter
    void ArmTimer(uint64_t time);

    /// Timer can be disarmed in two ways:
    ///  - The timer expires (timecheck returns true)
    ///  - Timer does not expire but a compaction occurs (external flush, memtable full)
    /// Sets @time_out_ member variable == 0
    void DisarmTimer();

    /// Needs to be public for internal pthread creation
    /// If @running_ == true, thread is in an Infinite work Loop
    /// The following occurs on Every iteration:
    ///    > if IsArmed() == true, then do a TimeCheck()
    ///      - if TimeCheck() returns true; disarm, flush, continue
    ///      - if TimeCheck() returns false; continue
    ///    > if IsArmed() == false, continue
    ///    > thread sleeps for @sleep_milisec_
    void TimeLoop();

    private:
    ///////////////////////////////////////////////
    /// Core timer functions are private to avoid
    /// invalid manipulation of critical timepoints
    ///////////////////////////////////////////////

    /// Check if current time is equal to
    /// or greater than @time_finish_
    bool TimeCheck();

    /// Compare @time_finish_ vs. tfinish_candidate
    ///  - NOTE: tfinish_candidate == (provided value + current time) - @sleep_milisec_
    /// if(tfinish_candidate < @time_finish_) OR (@time_out_ == 0):
    ///     time_finish_ = tfinish_candidate
    ///     time_out_ = value
    /// else: continue
    /// - NOTE: if @time_out_ == 0: an attempt to set valid finishtime has not yet occured
    void UpdateFinishTime(uint64_t value);
    bool IsArmed();
    bool Running();
    void PrintThreadRunTime(); //testing only

    //////////////////////////////////////
    /// Member Variables
    //////////////////////////////////////
    bool running_;
    bool armed_;
    int sleep_milisec_;
    uint64_t def_timeout_window_;
    uint64_t time_out_;
    std::chrono::high_resolution_clock::time_point time_finish_;
    std::chrono::high_resolution_clock::time_point thread_start_;
    std::chrono::high_resolution_clock::time_point thread_stop_;
    pthread_t thread_;
    pthread_mutex_t mutex_;
    TimerHandlerInterface *timer_handler_ptr_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_AGING_TIMER_H_
