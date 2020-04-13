#include "ring_buffer_log_sink.h"

namespace com {
namespace seagate {
namespace kinetic {

RingBufferLogSink::RingBufferLogSink(size_t max_message_len) {
    max_message_len_ = max_message_len;
}

void RingBufferLogSink::send(int severity, const char *full_filename, const char *base_filename,
    int line, const struct ::tm *tm_time, const char *message, size_t message_len) {
    pid_t thread_id = this->GetTID();
    LogRingBuffer::Instance()->push(severity, full_filename, base_filename, line, tm_time, message,
            ::std::min(message_len, max_message_len_), thread_id);
}

void RingBufferLogSink::copyBufferInto(std::vector<LogRingBufferEntry> &dest) {
    LogRingBuffer::Instance()->copyBuffer(dest);
}

// GetTID from glog; it's not meant to be exposed to consumers of glog so we copy it here.
pid_t RingBufferLogSink::GetTID() {
    // On Linux and MacOSX, we try to use gettid().
#if defined OS_LINUX || defined OS_MACOSX
#ifndef __NR_gettid
#ifdef OS_MACOSX
#define __NR_gettid SYS_gettid
#elif !defined __i386__
#error "Must define __NR_gettid for non-x86 platforms"
#else
#define __NR_gettid 224
#endif
#endif
    static bool lacks_gettid = false;
    if (!lacks_gettid) {
        pid_t tid = syscall(__NR_gettid);
        if (tid != -1) {
            return tid;
        }
        // Technically, this variable has to be volatile, but there is a small
        // performance penalty in accessing volatile variables and there should
        // not be any serious adverse effect if a thread does not immediately see
        // the value change to "true".
        lacks_gettid = true;
    }
#endif  // OS_LINUX || OS_MACOSX

    // If gettid() could not be used, we use one of the following.
#if defined OS_LINUX
  return getpid();  // Linux:  getpid returns thread ID when gettid is absent
#elif defined OS_WINDOWS || defined OS_CYGWIN
  return GetCurrentThreadId();
#else
    // If none of the techniques above worked, we use pthread_self().
    return (pid_t)(uintptr_t)pthread_self();
#endif
}

} // namespace kinetic
} // namespace seagate
} // namespace com
