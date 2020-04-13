#include "outgoing_value.h"
#include "flag_vars.h"
#include <errno.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include "glog/logging.h"
#include <algorithm>


namespace com {
namespace seagate {
namespace kinetic {

static const int NUM_RETRIES_BEFORE_SLEEP = 5;
bool OutgoingFileValue::TransferToSocket(int fd, int* err) const {
    off_t offset = 0;
    uint32_t socket_timeout = 0;
    size_t transferred = 0, value_size = size();
    int retries = 0;

    while (transferred < value_size && socket_timeout < Flag_vars::FLG_socket_timeout) {
        int status = sendfile(fd, fd_, &offset,
            std::min(static_cast<size_t>(kBlockSize), value_size - transferred));
        if (status == -1 && errno == EINTR) {
            continue;
        } else if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            retries++;
            if (retries == NUM_RETRIES_BEFORE_SLEEP) {
                usleep(1000);
                retries = 0;
                socket_timeout++;
            }
            continue;
        }
        if (status < 0) {
            *err = errno;
            return false;
        }
        transferred += status;
    }
    if (socket_timeout >= Flag_vars::FLG_socket_timeout) {
        LOG(INFO) << "TIMED OUT: Peer is slow to receive";
        return false;
    }
    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
