#include "outgoing_value.h"

#include "kinetic/reader_writer.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::ReaderWriter;

bool OutgoingFileValue::TransferToSocket(int fd) const {
    std::string s;
    if (!ToString(&s)) {
        return false;
    }
    ReaderWriter reader_writer(fd);
    return reader_writer.Write(s.data(), s.size());
}

} // namespace kinetic
} // namespace seagate
} // namespace com
