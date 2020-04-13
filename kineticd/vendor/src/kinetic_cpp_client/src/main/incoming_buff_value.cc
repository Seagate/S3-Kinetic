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

//#ifndef KINETIC_CPP_CLIENT_INCOMING_VALUE_H_
//#define KINETIC_CPP_CLIENT_INCOMING_VALUE_H_

#include <string>
#include <iostream>

#include "kinetic/common.h"
#include "kinetic/reader_writer.h"
#include "kernel_mem_mgr.h"
#include "kinetic/incoming_value.h"

using namespace std;
namespace kinetic {

/*
class IncomingBuffValue : public IncomingValueInterface {
    public:
    explicit IncomingBuffValue(char *s, size_t size);
    bool TransferToFile(int fd);
    void Consume();
    char *GetData();
    uint32_t size();

    private:
    char *s_;
    size_t size_;
    bool defunct_;
    DISALLOW_COPY_AND_ASSIGN(IncomingBuffValue);
};
*/

IncomingBuffValue::IncomingBuffValue(char *s, size_t size)
    : s_(s), size_(size), defunct_(false) {
    largeValue_ = NULL;
}

IncomingBuffValue::IncomingBuffValue(LargeMemory* largeValue) {
    s_ = NULL;
    largeValue_ = largeValue;
    size_ = largeValue_->size();
    defunct_ = false;
}

size_t IncomingBuffValue::size() {
    return size_;
}

char *IncomingBuffValue::GetUserValue() {
    return s_;
}

void IncomingBuffValue::SetBuffValueToNull() {
    s_ = NULL;
    largeValue_ = NULL;
}

void IncomingBuffValue::FreeUserValue() {
    if (s_) {
        free(s_);
        s_ = NULL;
    }
    delete largeValue_;
    largeValue_ = NULL;
    size_ = 0;
}

bool IncomingBuffValue::TransferToFile(int fd) {
    if (defunct_) {
        return false;
    }
    defunct_ = true;
    bool bSuccess = true;
    ReaderWriter reader_writer(fd);
    if (s_) {
        bSuccess = reader_writer.Write(s_, size_);
    } else if (largeValue_) {
        int size = 0;
        char* buf = largeValue_->getStart(size);
        while (buf && bSuccess) {
            bSuccess = reader_writer.Write(buf, size);
            buf = largeValue_->getNext(size);
        }
    }
    return bSuccess;
}



void IncomingBuffValue::Consume() {
    defunct_ = true;
}


} // namespace kinetic

//#endif  // KINETIC_CPP_CLIENT_INCOMING_VALUE_H_
