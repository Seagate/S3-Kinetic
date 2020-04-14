#include "spliceable_value.h"
#include "value_factory.h"

namespace com {
namespace seagate {
namespace kinetic {

ValueFactory::ValueFactory() {}

IncomingValueInterface *ValueFactory::NewValue(int fd, size_t n) {
    return new SpliceableValue(fd, n);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
