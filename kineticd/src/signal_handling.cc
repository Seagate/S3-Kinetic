#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include <list>

#include "glog/logging.h"
#include "server.h"
#include "signal_handling.h"
#include <sys/syscall.h>
#include "thread.h"
#include "schedule_compaction.h"

#ifdef __cplusplus
extern "C" {
#endif
    void signalHandler();
    void resetMsgCount();

#ifdef __cplusplus
}
#endif

namespace com {
namespace seagate {
namespace kinetic {

struct pipe_fds {
    int fds[2];
};

pthread_mutex_t signal_notification_pipes_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static std::list<pipe_fds> signal_notification_pipes;

static sigset_t signal_set;

static pthread_t signal_thread;
static KineticAlarms* kinetic_alarms_;
static KeyValueStoreInterface* keyValueStore_ = NULL;


int get_signal_notification_pipe() {
    struct pipe_fds fds;
    CHECK_NE(pipe(fds.fds), -1);

    // Make read end nonblocking
    int flags;
    CHECK_NE(flags = fcntl(fds.fds[0], F_GETFL), -1);
    flags |= O_NONBLOCK;
    CHECK_NE(fcntl(fds.fds[0], F_SETFL, flags), -1);

    // Make write end nonblocking
    CHECK_NE(flags = fcntl(fds.fds[1], F_GETFL), -1);
    flags |= O_NONBLOCK;
    CHECK_NE(fcntl(fds.fds[1], F_SETFL, flags), -1);


    CHECK(!pthread_mutex_lock(&signal_notification_pipes_mutex));
    signal_notification_pipes.push_back(fds);
    CHECK(!pthread_mutex_unlock(&signal_notification_pipes_mutex));

    return fds.fds[0];
}

void* signal_waiter(void* arg) {
    sigset_t* signal_set = (sigset_t*)arg;
    int signal_number;
    pid_t tid;
    tid = syscall(SYS_gettid);
    cout << " SIGNAL THREAD ID " << tid << endl;

    while (true) {
        CHECK(!sigwait(signal_set, &signal_number));
        VLOG_EVERY_N(4, 10) << "Received " << strsignal(signal_number) << " signal";
        if (signal_number == SIGUSR1) {
            VLOG(4) << "HANDLE SIGNAL USR1 " <<  signal_number;//NO_SPELL
            signalHandler();
        } else if (signal_number == SIGALRM) {
            VLOG_EVERY_N(4, 10) << "HANDLE SIGNAL ALARM " <<  signal_number;
            ConnectionHandler::_batchSetCollection.clockTick(1);
            if ((*kinetic_alarms_).get_system_alarm()) {
                int system_alarm_elasped_time =
                    (*kinetic_alarms_).system_alarm_elasped_time();
                if (system_alarm_elasped_time < 1) {
                    (*kinetic_alarms_).reset_system_alarm();
                    LogRingBuffer::Instance()->makeStaleEntryDataPersistent();
                    LogRingBuffer::Instance()->makeHistoPersistent();
                }
            }
            alarm(1);
        } else if (signal_number == SIGINT) {
            VLOG(2) << "HANDLE SIGNAL INT " <<  signal_number;
            CHECK(!pthread_mutex_lock(&signal_notification_pipes_mutex));
            int originalErrno = errno;
            for (std::list<pipe_fds>::iterator it = signal_notification_pipes.begin();
                it != signal_notification_pipes.end();
                ++it) {
                int status = write(it->fds[1], "x", 1);
                CHECK_NE(status, -1);

                if (close(it->fds[1]) != 0) {
                    PLOG(ERROR) << "Failed to close write end of signal handling pipe " << it->fds[1];
                }
            }
            errno = originalErrno;

            CHECK(!pthread_mutex_unlock(&signal_notification_pipes_mutex));
            return NULL;
        }
    }
}


void SetKeyValueStoreInSignal(KeyValueStoreInterface* keyValueStore) {
        keyValueStore_ = keyValueStore;
    }

void configure_signal_handling(KineticAlarms* kinetic_alarms) {
    // Mask all signal threads so that we can designate 1 signal
    // handling thread

    kinetic_alarms_ = kinetic_alarms;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGUSR1);
    sigaddset(&signal_set, SIGALRM);

    CHECK(!pthread_sigmask(SIG_BLOCK, &signal_set, NULL));

    size_t STK_SIZE = 1 * 1024 * 1024;
    // Create a thread whose only job is to handle signals
    pthread_attr_t thread_attr;
    CHECK(!pthread_attr_init(&thread_attr));
    CHECK(!pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED));
    CHECK(!pthread_attr_setstacksize(&thread_attr, STK_SIZE));
    CHECK(!pthread_create(&signal_thread, &thread_attr, signal_waiter, (void*)&signal_set));
    CHECK(!pthread_attr_destroy(&thread_attr));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
