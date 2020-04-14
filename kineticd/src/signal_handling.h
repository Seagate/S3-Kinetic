#ifndef KINETIC_SIGNAL_HANDLING_H_
#define KINETIC_SIGNAL_HANDLING_H_
#include "kinetic_alarms.h"
#include "log_ring_buffer.h"
#include "key_value_store.h"
#include <sys/syscall.h>
#include "thread.h"
#include "schedule_compaction.h"

namespace com {
namespace seagate {
namespace kinetic {

void configure_signal_handling(KineticAlarms* kinetic_alarms);
void SetKeyValueStoreInSignal(KeyValueStoreInterface* keyValueStore);


int get_signal_notification_pipe();


} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SIGNAL_HANDLING_H_
