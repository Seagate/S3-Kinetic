#ifndef KINETIC_STACK_TRACE_H_
#define KINETIC_STACK_TRACE_H_

#ifdef __cplusplus
extern "C" {
#endif
    void signalHandler();
    void resetMsgCount();
    void segFaultHandler(int sig);
#ifdef __cplusplus
}
#endif
#endif  // KINETIC_STACK_TRACE_H_
