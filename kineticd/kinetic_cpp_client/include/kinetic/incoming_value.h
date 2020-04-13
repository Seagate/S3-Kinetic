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

#ifndef KINETIC_CPP_CLIENT_INCOMING_VALUE_H_
#define KINETIC_CPP_CLIENT_INCOMING_VALUE_H_

#include <string>

#include "kinetic/common.h"
#include "openssl/ssl.h"
#include "LargeMemory.h"

namespace kinetic {

/*
 * IncomingValueInterface represents a byte-array value arriving in a PUT
 * request. This can take the form of either a string in memory or a socket
 * file descriptor from which we can read a specified number of bytes.
 */
class IncomingValueInterface {
    public:
    virtual ~IncomingValueInterface() {}

    virtual size_t size() = 0;

    /*
     * TransferToFile transfers the contents of the value to a file represented
     * by the given file descriptor. As soon as this method has been called,
     * the object should be considered defunct and all further calls to
     * TransferToFile and ToString will fail.
     */
    virtual bool TransferToFile(int fd) = 0;
    virtual char *GetUserValue() {return NULL;};
    virtual void SetBuffValueToNull() {};
    virtual void  FreeUserValue() {};

    /*
     * ToString copies the value to the string pointed to by the result
     * parameter.
     */
    virtual bool ToString(std::string *result) {return false;};

    /*
     * Consume does whatever is necessary to consume the resources underlying
     * the value. In the SpliceableValue implementation this consists of
     * reading the appropriate bytes from the socket and throwing them away.
     * After this method has been called, the object should be considered
     * defunct and all further calls to TransferToFile and ToString will fail.
     */
    virtual void Consume() = 0;
};

/*
 * IncomingStringValue represents a value stored internally as a plain string.
 * It's preferable to use SpliceableValue whenever possible because of its
 * performance benefits.
 */
class IncomingStringValue : public IncomingValueInterface {
    public:
    explicit IncomingStringValue(const std::string &s);
    size_t size();
    bool TransferToFile(int fd);
    bool ToString(std::string *result);
    void Consume();

    private:
    const std::string s_;
    bool defunct_;
    DISALLOW_COPY_AND_ASSIGN(IncomingStringValue);
};

class IncomingValueFactoryInterface {
    public:
    virtual ~IncomingValueFactoryInterface() {}
    virtual IncomingValueInterface *NewValue(int fd, size_t n) = 0;
    virtual IncomingValueInterface *SslNewValue(SSL *ssl, size_t n) = 0;
};


class IncomingBuffValue : public IncomingValueInterface {
    public:
    IncomingBuffValue(char *s, size_t size);
    IncomingBuffValue(LargeMemory* largeValue);
    virtual ~IncomingBuffValue() {}
    bool TransferToFile(int fd);
    void Consume();
    char *GetUserValue();
    LargeMemory* getUserLargeValue() {
        return largeValue_;
    }
    void SetBuffValueToNull();
    void FreeUserValue();
    size_t size();

    private:
    char *s_;
    LargeMemory* largeValue_;
    size_t size_;
    bool defunct_;
    DISALLOW_COPY_AND_ASSIGN(IncomingBuffValue);
};

/*

IncomingBuffValue::IncomingBuffValue(char *s, size_t size)
    : s_(s), size_(size), defunct_(false) {}

size_t IncomingBuffValue::size() {
    return size_;
}

char *IncomingBuffValue::GetData() {
    return s_;
}
bool IncomingBuffValue::TransferToFile(int fd) {
    if (defunct_) {
        return false;
    }
    defunct_ = true;

    ReaderWriter reader_writer(fd);
    return reader_writer.Write(s_, size_);
}



void IncomingBuffValue::Consume() {
    defunct_ = true;
}

*/
} // namespace kinetic

#endif  // KINETIC_CPP_CLIENT_INCOMING_VALUE_H_
