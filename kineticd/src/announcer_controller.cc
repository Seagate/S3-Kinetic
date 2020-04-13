#include "gmock/gmock.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "announcer_controller.h"
#include "popen_wrapper.h"


#include <signal.h>
#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {
namespace announcer {

DiscoveryState AnnouncerController::state_ = NO_DHCP;

extern "C" void signalHandler(void) {
    AnnouncerController::state_ = HAS_DHCP;
    AnnouncerController::bSignal_ = true;
}

bool AnnouncerController::bSignal_ = false;

AnnouncerController::AnnouncerController(
        int multicast_interval,
        std::vector<std::unique_ptr<AnnouncerInterface>>* announcers) :
        ScheduledBackgroundTask("Kinetic Discovery Controller", multicast_interval),
        announcers_(announcers) {
    bSignal_ = false;
}

void AnnouncerController::DoWork() {
    switch (AnnouncerController::state_) {
        case NO_DHCP: {
            VLOG(1) << "No DHCP";//NO_SPELL

            for (auto &announcer : *announcers_) {
                if (announcer->Configure()) {
                    announcer->Announce();
                    AnnouncerController::state_ = MULTICAST;
                }
            }

            break;
        }
        case HAS_DHCP: {
            if (AnnouncerController::bSignal_) {
                AnnouncerController::bSignal_ = false;
#ifdef KDEBUG
                DLOG(INFO) << "Received DHCP heartbeat configuration";//NO_SPELL
#endif
                for (auto &announcer : *announcers_) {
                    if (announcer->Configure()) {
                        announcer->Announce();
                        AnnouncerController::state_ = MULTICAST;
                    }
                }
            }
            break;
        }
        case MULTICAST: {
            for (auto &announcer : *announcers_) {
                announcer->Announce();
            }
            break;
        }
        default: {
            LOG(WARNING) << "Invalid announcer state";
            break;
        }
    } // End switch
}

bool AnnouncerController::Init() {
    state_ = NO_DHCP;
    string system_command = "killall -s SIGUSR1 udhcpc";
    if (!execute_command(system_command)) {
        LOG(WARNING) << "Failed to Renew lease ";
        return false;
    } else {
        return true;
    }
}
} // namespace announcer
} // namespace kinetic
} // namespace seagate
} // namespace com
