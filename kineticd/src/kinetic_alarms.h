#ifndef KINETIC_KINETIC_ALARMS_H_
#define KINETIC_KINETIC_ALARMS_H_

namespace com {
namespace seagate {
namespace kinetic {

class KineticAlarms {
    public:
    static KineticAlarms* Instance();
    ~KineticAlarms();
    bool get_max_kineticd_idle_alarm();
    bool get_system_alarm();
    int max_kineticd_idle_alarm_elasped_time();
    int system_alarm_elasped_time();
    void set_max_kineticd_idle_alarm(bool arm);
    void set_system_alarm(bool arm);
    void reset_max_kineticd_idle_alarm();
    void reset_system_alarm();


    private:
    KineticAlarms();
    bool max_idle_time_alarm_on_;
    bool system_alarm_on_;
    bool max_kineticd_idle_alarm_on_;
    int system_alarm_seconds_;
    int max_kineticd_idle_seconds_;
    static KineticAlarms* stop_watch_instance_;
    // DISALLOW_COPY_AND_ASSIGN(KineticAlarms);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_KINETIC_ALARMS_H_
