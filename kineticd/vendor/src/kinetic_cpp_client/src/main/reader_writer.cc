/*
 * kinetic-cpp-client
 * Copyright (C) 2014 Seagate Technology.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "kinetic/reader_writer.h"

#include <errno.h>
#include <unistd.h>

#include "glog/logging.h"
#include <iostream>

using namespace std;

const uint32_t SOCKET_TIMEOUT = 10000000/150; // 10 sec
namespace kinetic {

ReaderWriter::ReaderWriter(int fd) : fd_(fd) {}

bool ReaderWriter::Read(void *buf, size_t n, int* err) {
    size_t bytes_read = 0;
    uint32_t socket_timeout = 0;
    if (n ==0) return true;
    while (bytes_read < n && socket_timeout < SOCKET_TIMEOUT) {
        int status = read(fd_, reinterpret_cast<char *>(buf) + bytes_read, n - bytes_read);
        if (status == -1 && errno == EINTR) {
            continue;
        } else if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK )) {
	    //Wait for 150us;
	    usleep(150);
            socket_timeout++;
            continue;
        } else if (status < 0) {
            *err = errno;
            PLOG(WARNING) << "Failed to read from socket " << fd_;
            return false;
        }
        if (status == 0) {
            LOG(WARNING) << "Unexpected EOF. Socket (TX) may be closed by Peer " << fd_ << " " <<  strerror(errno) << " " << errno;
            *err = 0xFF;
            return false;
        }
        bytes_read += status;
    }
    if (socket_timeout >= SOCKET_TIMEOUT) {
         LOG(INFO) << "Peer is slow to transmit " << fd_;
         *err = 0xFE;
        return false;
    }
    return true;
}

bool ReaderWriter::Write(const void *buf, size_t n) {
    size_t bytes_written = 0;
    uint32_t socket_timeout = 0;
    if (n == 0) return true;
    while (bytes_written < n && socket_timeout < SOCKET_TIMEOUT) {
        int status = write(fd_, reinterpret_cast<const char *>(buf) + bytes_written,
            n - bytes_written);
        if (status == -1 && errno == EINTR) {
            continue;
        } else if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    usleep(150);
	    socket_timeout++;
            continue;
        } else if (status < 0) {
            PLOG(WARNING) << "Failed to write to socket";
            return false;
        }
        if (status == 0) {
            LOG(WARNING) << "Unexpected EOF, Socket(RX) may be closed by Peer";
            return false;
        }
        bytes_written += status;

    }
    if (socket_timeout >= SOCKET_TIMEOUT) {
        LOG(INFO) << " Peer is slow to receive";
        return false;
    }
    return true;
}

} // namespace kinetic
