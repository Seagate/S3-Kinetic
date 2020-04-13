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

#include "kinetic/ssl_reader_writer.h"

#include <unistd.h>

#include "glog/logging.h"

namespace kinetic {

SslReaderWriter::SslReaderWriter(SSL *ssl) : ssl_(ssl) {}

bool SslReaderWriter::Read(void *buf, size_t n, int* err) {
    if (n == 0) {
        // SSL_read with 0 bytes causes openssl to get really upset in mysterious ways
        return true;
    }

    // To be able to move the pointed as data arrives we need to cast it to a complete
    // c type.
    uint8_t* byte_buffer = static_cast<uint8_t*>(buf);

    while (n > 0) {
        int bytes_read = SSL_read(ssl_, byte_buffer, n);

        if (bytes_read > 0) {
            // If SSL_read succeeds it returns the number of bytes read
            n -= bytes_read;
            byte_buffer += bytes_read;
        } else {
            // Return values of 0 or <0 indicate an error in SSL_read
            // Call SSL_get_error to find out what went wrong
            bool retry_read = false;
            switch (SSL_get_error(ssl_, bytes_read)) {
                case SSL_ERROR_NONE:
                    if (bytes_read <= 0) break;
                    LOG(INFO) << "SSL_ERROR_NONE and ret > 0";//NO_SPELL
                    break;
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    // Underlying BIO is not ready for read, set retry_read bool to true
                    retry_read = true;
                    break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                    LOG(INFO) << "SSL_ERROR_WANT_X509_LOOKUP";//NO_SPELL
                    break;
                case SSL_ERROR_WANT_ACCEPT:
                    LOG(INFO) << "SSL_ERROR_WANT_ACCEPT";//NO_SPELL
                    break;
                case SSL_ERROR_WANT_CONNECT:
                    break;
                case SSL_ERROR_SYSCALL:
                    if (bytes_read != 0) {
                        PLOG(INFO) << "SSL_ERROR_SYSCALL";//NO_SPELL
                    } else {
                        LOG(WARNING) << "Unexpected EOF. Socket (TX) may be closed by Peer " << ssl_;
                        *err = 0xFF;
                        return false;
                    }
                    break;
                case SSL_ERROR_SSL:
                    LOG(INFO) << "SSL_ERROR_SSL";//NO_SPELL
                    break;
                case SSL_ERROR_ZERO_RETURN:
                    LOG(INFO) << "SSL_ERROR_ZERO_RETURN";//NO_SPELL
                    break;
                default:
                    break;
                }

            if (retry_read) {
                // If the error was SSL_ERROR_WANT_READ, try to do the read again
                continue;
            } else {
                LOG(WARNING) << "Failed to read " << n << " bytes over SSL connection";
                return false;
            }
        }
    }

    return true;
}

bool SslReaderWriter::Write(const void *buf, size_t n) {
    if (n == 0) {
        // It's not clear whether SSL_write can handle a write of 0 bytes
        return true;
    }

    // To be able to move the pointed as data arrives we need to cast it to a complete
    // c type.
    const uint8_t* byte_buffer = static_cast<const uint8_t*>(buf);

    while (n > 0) {
        int bytes_written = SSL_write(ssl_, byte_buffer, n);

        if (bytes_written > 0) {
            // If SSL_write succeeds it returns the number of bytes written
            n -= bytes_written;
            byte_buffer += bytes_written;
        } else {
            // Return values of 0 or <0 indicate an error in SSL_write
            // Call SSL_get_error to find out what went wrong
            bool retry_write = false;
            switch (SSL_get_error(ssl_, bytes_written)) {
                case SSL_ERROR_NONE:
                    if (bytes_written <= 0) break;
                    LOG(INFO) << "SSL_ERROR_NONE and ret > 0";//NO_SPELL
                    break;
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    // Underlying BIO is not ready for write, set retry_write bool to true
                    retry_write = true;
                    break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                    LOG(INFO) << "SSL_ERROR_WANT_X509_LOOKUP";//NO_SPELL
                    break;
                case SSL_ERROR_WANT_ACCEPT:
                    LOG(INFO) << "SSL_ERROR_WANT_ACCEPT";//NO_SPELL
                    break;
                case SSL_ERROR_WANT_CONNECT:
                    break;
                case SSL_ERROR_SYSCALL:
                    PLOG(INFO) << "SSL_ERROR_SYSCALL";//NO_SPELL
                    break;
                case SSL_ERROR_SSL:
                    LOG(INFO) << "SSL_ERROR_SSL";//NO_SPELL
                    break;
                case SSL_ERROR_ZERO_RETURN:
                    LOG(INFO) << "SSL_ERROR_ZERO_RETURN";//NO_SPELL
                    break;
                default:
                    break;
            }

            if (retry_write) {
                // If the error was SSL_ERROR_WANT_WRITE, try to do the write again
                continue;
            } else {
                LOG(WARNING) << "Failed to write " << n << " bytes over SSL connection";
                return false;
            }
        }
    }

    return true;
}

} // namespace kinetic
