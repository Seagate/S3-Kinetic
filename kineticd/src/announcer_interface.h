#ifndef KINETIC_ANNOUNCER_INTERFACE_H_
#define KINETIC_ANNOUNCER_INTERFACE_H_

namespace com {
namespace seagate {
namespace kinetic {

class AnnouncerInterface {
    public:
        virtual ~AnnouncerInterface() {}
        virtual bool Configure()=0;
        virtual void Announce()=0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_ANNOUNCER_INTERFACE_H_
