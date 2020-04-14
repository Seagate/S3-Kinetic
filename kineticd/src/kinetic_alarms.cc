#include "kinetic_alarms.h"
#include <stdio.h>

namespace com {
namespace seagate {
namespace kinetic {

KineticAlarms* KineticAlarms::stop_watch_instance_ = NULL;
static int system_alarm_seconds = 7200;
static int max_kineticd_idle = 10;


bool KineticAlarms::get_max_kineticd_idle_alarm() {
    return KineticAlarms::max_kineticd_idle_alarm_on_;
}

bool KineticAlarms::get_system_alarm() {
    return KineticAlarms::system_alarm_on_;
}

int KineticAlarms::max_kineticd_idle_alarm_elasped_time() {
    KineticAlarms::max_kineticd_idle_seconds_--;
    return KineticAlarms::max_kineticd_idle_seconds_;
}

int KineticAlarms::system_alarm_elasped_time() {
    KineticAlarms::system_alarm_seconds_--;
    return KineticAlarms::system_alarm_seconds_;
}

void KineticAlarms::reset_max_kineticd_idle_alarm() {
    KineticAlarms::max_kineticd_idle_seconds_ = max_kineticd_idle;
}

void KineticAlarms::reset_system_alarm() {
    KineticAlarms::system_alarm_seconds_ = system_alarm_seconds;
}


void KineticAlarms::set_max_kineticd_idle_alarm(bool arm) {
    KineticAlarms::max_kineticd_idle_alarm_on_ = arm;
    KineticAlarms::max_kineticd_idle_seconds_ = max_kineticd_idle;
}

void KineticAlarms::set_system_alarm(bool arm) {
    KineticAlarms::system_alarm_on_ = arm;
    KineticAlarms::system_alarm_seconds_ = system_alarm_seconds;
}


KineticAlarms* KineticAlarms::Instance() {
    if (!stop_watch_instance_) {
        stop_watch_instance_ = new KineticAlarms();
    }
    return stop_watch_instance_;
}

KineticAlarms::KineticAlarms()
    : system_alarm_on_(false) {}

} // namespace kinetic
} // namespace seagate
} // namespace com
