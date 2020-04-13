#ifndef KINETIC_RUNNABLE_INTERFACE_H_
#define KINETIC_RUNNABLE_INTERFACE_H_

namespace com {
namespace seagate {
namespace common {

class RunnableInterface {
   public:
      RunnableInterface() {}
      virtual ~RunnableInterface() {}
      virtual void run() = 0;
};

} // namespace common
} // namespace seagate
} // namespace com

#endif  // KINETIC_RUNNABLE_INTERFACE_H_
