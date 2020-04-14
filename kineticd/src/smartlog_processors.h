#ifndef KINETIC_SMARTLOG_PROCESSORS_H_
#define KINETIC_SMARTLOG_PROCESSORS_H_

#include "glog/logging.h"

#include "popen_wrapper.h"

#include <map>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

struct smart_values {
    int value;
    int worst;
    int threshold;
};

class SMARTLogProcessor : public LineProcessor {
    public:
    static const char SMART_ATTRIBUTE_START_STOP_COUNT[];
    static const char SMART_ATTRIBUTE_POWER_ON_HOURS[];
    static const char SMART_ATTRIBUTE_POWER_CYCLE_COUNT[];
    static const char SMART_ATTRIBUTE_HDA_TEMPERATURE[];
    static const char SMART_ATTRIBUTE_SPIN_UP_TIME[];
    static const char SMART_ATTRIBUTE_POWER_OFF_RETRACT_COUNT[];
    static const char SMART_ATTRIBUTE_LOAD_CYCLE_COUNT[];
    static const char* ATTRIBUTES_INTERESTED[];
    explicit SMARTLogProcessor(std::map<string, smart_values>* smart_attributes);
    virtual void ProcessLine(const string& line);
    virtual void ProcessPCloseResult(int pclose_result);
    virtual bool Success();

    private:
    std::map<string, smart_values>* smart_attributes_;
    int number_of_attributes_;
};

class DriveIdSMARTLogProcessor : public LineProcessor {
    public:
    explicit DriveIdSMARTLogProcessor(std::string* drive_wwn,
            std::string* drive_sn,
            std::string* drive_model,
            std::string* drive_fv_);
    virtual void ProcessLine(const string& line);
    virtual void ProcessPCloseResult(int pclose_result);
    virtual bool Success();

    private:
    string* drive_wwn_;
    string* drive_sn_;
    string* drive_model_;
    string* drive_fv_;
    bool found_wwn_;
    bool found_sn_;
    bool found_model_;
    bool found_fv_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SMARTLOG_PROCESSORS_H_
