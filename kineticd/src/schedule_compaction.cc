#include "schedule_compaction.h"
#include <iostream>

using namespace std; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

void ScheduleCompaction::run() {
     keyValueStore_->BGSchedule();
}

} // namespace kinetic
} // namespace seagate
} // namespace com
