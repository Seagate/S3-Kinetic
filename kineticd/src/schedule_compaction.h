#ifndef KINETIC_SCHEDULE_COMPACTION_H_
#define KINETIC_SCHEDULE_COMPACTION_H_

#include "key_value_store_interface.h"
#include "runnable_interface.h"

using namespace com::seagate::common; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {

class ScheduleCompaction : public RunnableInterface {
    public:
        ScheduleCompaction(KeyValueStoreInterface* keyValueStore, int level = 0):
            keyValueStore_(keyValueStore), level_(level) {}
        virtual ~ScheduleCompaction() {}
        void run();

    private:
        KeyValueStoreInterface* keyValueStore_;
        int level_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SCHEDULE_COMPACTION_H_
