#ifndef KINETIC_ANNOUNCER_CONTROLLER_H_
#define KINETIC_ANNOUNCER_CONTROLLER_H_

#include "device_information_interface.h"
#include "scheduled_background_task.h"
#include "device_information.h"
#include "announcer_interface.h"
#include <memory>

namespace com {
namespace seagate {
namespace kinetic {
namespace announcer {

enum DiscoveryState {
    NO_DHCP, HAS_DHCP, MULTICAST
};

class AnnouncerController : public ScheduledBackgroundTask {
    public:
        static DiscoveryState state_;
        static bool bSignal_;

    public:
        AnnouncerController(
                int multicast_interval,
                std::vector<std::unique_ptr<AnnouncerInterface>>* announcers);
        virtual ~AnnouncerController() {
        }

        virtual void DoWork();
        bool Init();

    private:
        std::vector<std::unique_ptr<AnnouncerInterface>>* announcers_;
        DISALLOW_COPY_AND_ASSIGN(AnnouncerController);
};

} // namespace announcer
} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_ANNOUNCER_CONTROLLER_H_
