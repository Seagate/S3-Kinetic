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

#include "kinetic/byte_stream.h"

#include <errno.h>
#include <unistd.h>

#include "glog/logging.h"

#include "kinetic/incoming_value.h"
#include "kinetic/outgoing_value.h"
#include "kinetic/reader_writer.h"
#include "kinetic/ssl_reader_writer.h"

namespace kinetic {

PlainByteStream::PlainByteStream(int fd, IncomingValueFactoryInterface &value_factory)
    : fd_(fd), value_factory_(value_factory) {}

bool PlainByteStream::Read(void *buf, size_t n, int* err) {
    ReaderWriter reader_writer(fd_);
    //int err;
    return reader_writer.Read(buf, n, err);
}

bool PlainByteStream::Write(const void *buf, size_t n) {
    ReaderWriter reader_writer(fd_);
    return reader_writer.Write(buf, n);
}

IncomingValueInterface *PlainByteStream::ReadValue(size_t n) {
    return value_factory_.NewValue(fd_, n);
}

bool PlainByteStream::WriteValue(const OutgoingValueInterface &value, int* err) {
    return value.TransferToSocket(fd_, err);
}

SslByteStream::SslByteStream(SSL *ssl , IncomingValueFactoryInterface &value_factory)
    : ssl_(ssl), value_factory_(value_factory) {}

SslByteStream::~SslByteStream() {}

bool SslByteStream::Read(void *buf, size_t n, int* err) {
    SslReaderWriter reader_writer(ssl_);
    return reader_writer.Read(buf, n, err);
}

bool SslByteStream::Write(const void *buf, size_t n) {
    SslReaderWriter reader_writer(ssl_);
    return reader_writer.Write(buf, n);
}

IncomingValueInterface *SslByteStream::ReadValue(size_t n) {
    return value_factory_.SslNewValue(ssl_, n);
}

bool SslByteStream::WriteValue(const OutgoingValueInterface &value, int* err) {
    std::string s;
    if (!value.ToString(&s, err)) {
        return false;
    }
    return Write(s.data(), s.size());
}

} // namespace kinetic
