#ifndef KINETIC_THREAD_H_
#define KINETIC_THREAD_H_

#include <pthread.h>
#include "runnable_interface.h"

namespace com {
namespace seagate {
namespace common {

class Thread {
    public:
        explicit Thread(RunnableInterface* threadObj) {
            threadObj_ = threadObj;
            threadId_ = 0;
        }

        virtual ~Thread() {}
        virtual void start(bool detached = false);

        pthread_t getThreadId() const {
            return threadId_;
        }

     protected:
        static void* threadProc(void *arg) {
            ((RunnableInterface*)arg)->run();
            return NULL;
        }

     private:
        RunnableInterface* threadObj_;
        pthread_t threadId_;
};

inline void Thread::start(bool detached) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t STK_SIZE = 4 * 1024 * 1024;
    if (detached) {
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, STK_SIZE);
        pthread_create(&threadId_, &attr, threadProc, threadObj_);
        pthread_attr_destroy(&attr);
    } else {
        pthread_attr_setstacksize(&attr, STK_SIZE);
        pthread_create(&threadId_, &attr, threadProc, threadObj_);
        pthread_attr_destroy(&attr);
    }
}

} // namespace common
} // namespace seagate
} // namespace com

#endif  // KINETIC_THREAD_H_


