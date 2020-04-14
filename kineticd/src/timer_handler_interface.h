#ifndef KINETIC_TIMER_HANDLER_INTERFACE_H_
#define KINETIC_TIMER_HANDLER_INTERFACE_H_

namespace com {
namespace seagate {
namespace kinetic {

class TimerHandlerInterface {
    public:
    virtual ~TimerHandlerInterface() {}
    virtual void ServiceTimer(bool toSST)=0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_TIMER_HANDLER_INTERFACE_H_
