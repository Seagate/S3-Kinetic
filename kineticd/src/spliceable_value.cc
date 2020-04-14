//#include "flags.h"
#include "flag_vars.h"

#include "spliceable_value.h"

#include <fcntl.h>
#include <unistd.h>

#include "kinetic/incoming_value.h"
#include "kinetic/reader_writer.h"

#include "glog/logging.h"
#include <sys/time.h>
#include <iostream>
using namespace std; // NOLINT

//uint32_t SOCKET_TIMEOUT;

namespace com {
namespace seagate {
namespace kinetic {

static const int NUM_RETRIES_BEFORE_SLEEP = 5;
using ::kinetic::ReaderWriter;

SpliceableValue::SpliceableValue(int fd, size_t size)
    : fd_(fd), size_(size), defunct_(false) {}

size_t SpliceableValue::size() {
    return size_;
}

bool SpliceableValue::TransferToFile(int fd) {
    if (defunct_) {
        return false;
    }
    defunct_ = true;
    loff_t out_offset = 0;
    size_t bytes_transferred = 0;
    bool success = true;
    uint32_t socket_timeout = 0;
    int retries = 0;

    while (bytes_transferred < size_ && socket_timeout < Flag_vars::FLG_socket_timeout) {
        int status = splice(fd_, NULL, fd, &out_offset,
            std::min(static_cast<size_t>(kBlockSize), size_ - bytes_transferred),
            SPLICE_F_NONBLOCK | SPLICE_F_MOVE);

        if (status == -1 && errno == EINTR) {
            continue;
        } else if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK )) {
            //Wait for 1ms;
            retries++;
            if (retries == NUM_RETRIES_BEFORE_SLEEP) {
                usleep(1000);
                retries = 0;
                socket_timeout++;
            }
            continue;
        }
        if (status < 0) {
            PLOG(WARNING) << "Failed to splice from socket to file";
            success = false;
            break;
        }
        if (status == 0) {
            LOG(WARNING) << "Failed to splice from socket to file";
            success = false;
            break;
        }
        bytes_transferred += status;
    }
    if (socket_timeout >= Flag_vars::FLG_socket_timeout) {
        LOG(INFO) << "TIMED OUT: Peer is slow to transmit";
        return false;
    }

    posix_fadvise64(fd, 0, 0, POSIX_FADV_DONTNEED);
    return success;
}

bool SpliceableValue::ToString(std::string *result) {
    if (defunct_) {
        return false;
    }
    defunct_ = true;

    int err;
    char *buf = new char[size_];
    ReaderWriter reader_writer(fd_);
    bool success = reader_writer.Read(buf, size_, &err);
    if (success) {
        *result = std::string(buf, size_);
    }
    delete[] buf;

    return success;
}

void SpliceableValue::Consume() {
    if (defunct_) {
        return;
    }

    std::string value;
    if (!ToString(&value)) {
        // If this fails, there's not much we can do other than log it.
        LOG(WARNING) << "Failed to consume value";
    }

    defunct_ = true;    // must set this only after ToString() is called
}

} // namespace kinetic
} // namespace seagate
} // namespace com
