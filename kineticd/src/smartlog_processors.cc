#include <stdlib.h>

#include "smartlog_processors.h"
#include "popen_wrapper.h"
#include <utility>
#include <functional>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
const char SMARTLogProcessor::SMART_ATTRIBUTE_START_STOP_COUNT[] = "Start_Stop_Count";
const char SMARTLogProcessor::SMART_ATTRIBUTE_POWER_ON_HOURS[] = "Power_On_Hours";
const char SMARTLogProcessor::SMART_ATTRIBUTE_POWER_CYCLE_COUNT[] = "Power_Cycle_Count";
const char SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE[] = "Temperature_Celsius";
const char SMARTLogProcessor::SMART_ATTRIBUTE_SPIN_UP_TIME[] = "Spin_Up_Time";
const char SMARTLogProcessor::SMART_ATTRIBUTE_POWER_OFF_RETRACT_COUNT[] = "Power-Off_Retract_Count";
const char SMARTLogProcessor::SMART_ATTRIBUTE_LOAD_CYCLE_COUNT[] = "Load_Cycle_Count";
const char* SMARTLogProcessor::ATTRIBUTES_INTERESTED[] {
                    SMARTLogProcessor::SMART_ATTRIBUTE_START_STOP_COUNT,
                    SMARTLogProcessor::SMART_ATTRIBUTE_POWER_ON_HOURS,
                    SMARTLogProcessor::SMART_ATTRIBUTE_POWER_CYCLE_COUNT,
                    SMARTLogProcessor::SMART_ATTRIBUTE_HDA_TEMPERATURE,
                    SMARTLogProcessor::SMART_ATTRIBUTE_POWER_OFF_RETRACT_COUNT,
                    SMARTLogProcessor::SMART_ATTRIBUTE_LOAD_CYCLE_COUNT,
                    SMARTLogProcessor::SMART_ATTRIBUTE_SPIN_UP_TIME };

SMARTLogProcessor::SMARTLogProcessor(
    std::map<string, smart_values>* smart_attributes)
    : smart_attributes_(smart_attributes) {
        number_of_attributes_ = 0;
    }

void SMARTLogProcessor::ProcessLine(const string& line) {
    char attribute_name[50];
    int id, value, worst, threshold;
    if (sscanf(line.c_str(), "%d %s %*x %d %d %d", &id,
        attribute_name, &value, &worst, &threshold) == 5) {
        VLOG(2) << "smartctl reports " << attribute_name << " value=" << value//NO_SPELL
                << " threshold=" << threshold
                << " worst=" << worst;

        number_of_attributes_++;
        struct smart_values attribute_values_;
        attribute_values_.value = value;
        attribute_values_.worst = worst;
        attribute_values_.threshold = threshold;
        string key = attribute_name;
        (*smart_attributes_)[key] = attribute_values_;
    }
}

void SMARTLogProcessor::ProcessPCloseResult(int pclose_result) {}

bool SMARTLogProcessor::Success() {
    int size_of_atrributes_interested = 7;
    for (int i = 0; i < size_of_atrributes_interested; i++) {
        if ((*smart_attributes_).find(ATTRIBUTES_INTERESTED[i]) == (*smart_attributes_).end()) {
            return false;
        }
    }
    // If we get to here than we know that we got all the SMART attributes that we are interested in
    return true;
}

DriveIdSMARTLogProcessor::DriveIdSMARTLogProcessor(
    string* drive_wwn,
    string* drive_sn,
    string* drive_model,
    string* drive_fv)
    : drive_wwn_(drive_wwn),
    drive_sn_(drive_sn),
    drive_model_(drive_model),
    drive_fv_(drive_fv),
    found_wwn_(false),
    found_sn_(false),
    found_model_(false),
    found_fv_(false) {}

void DriveIdSMARTLogProcessor::ProcessLine(const string& line) {
    string wwn_sentinel("LU WWN Device Id:");
    string sn_sentinel("Serial Number:");
    string model_sentinel("Device Model:");
    string firmware_version_sentinel("Firmware Version:");

    if (line.compare(0, wwn_sentinel.length(), wwn_sentinel) == 0) {
        // Drop the sentinel and trailing newline to get just the WWN
        *drive_wwn_ =
            line.substr(wwn_sentinel.length(), line.size() - wwn_sentinel.length() - 1);
        drive_wwn_->erase(0, drive_wwn_->find_first_not_of(' '));
        drive_wwn_->erase(drive_wwn_->find_last_not_of(' ') + 1);
        VLOG(2) << "smartctrl reports WWN <" << *drive_wwn_ << ">";//NO_SPELL
        found_wwn_ = true;
    } else if (line.compare(0, sn_sentinel.length(), sn_sentinel) == 0) {
        *drive_sn_ = line.substr(sn_sentinel.length(), line.size() - sn_sentinel.length() - 1);
        drive_sn_->erase(0, drive_sn_->find_first_not_of(' '));
        drive_sn_->erase(drive_sn_->find_last_not_of(' ') + 1);
        VLOG(2) << "smartctrl reports SN <" << *drive_sn_ << ">";//NO_SPELL
        found_sn_ = true;
    } else if (line.compare(0, model_sentinel.length(), model_sentinel) == 0) {
        *drive_model_ = line.substr(model_sentinel.length(),
                line.size() - model_sentinel.length() - 1);
        drive_model_->erase(0, drive_model_->find_first_not_of(' '));
        drive_model_->erase(drive_model_->find_last_not_of(' ') + 1);
        VLOG(2) << "smartctrl reports Model <" << *drive_model_ << ">";//NO_SPELL
        found_model_ = true;
    } else if (line.compare(0, firmware_version_sentinel.length(),
                            firmware_version_sentinel) == 0) {
        // Drop the sentinel and trailing newline to get just the FV
        *drive_fv_ = line.substr(firmware_version_sentinel.length(),
                                 line.size() - firmware_version_sentinel.length() - 1);
        drive_fv_->erase(0, drive_fv_->find_first_not_of(' '));
        drive_fv_->erase(drive_fv_->find_last_not_of(' ') + 1);
        VLOG(2) << "smartctrl reports fv <" << *drive_fv_ << ">";//NO_SPELL
        found_fv_ = true;
    }
}

void DriveIdSMARTLogProcessor::ProcessPCloseResult(int pclose_result) {}

bool DriveIdSMARTLogProcessor::Success() {
    return found_wwn_ && found_sn_ && found_model_ && found_fv_;
}


} // namespace kinetic
} // namespace seagate
} // namespace com
