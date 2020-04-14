#include "key_generator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>

namespace com {
namespace seagate {
namespace kinetic {

KeyGenerator::KeyGenerator(size_t key_size) : key_size_(key_size) {
    current_key_ = (unsigned char*) malloc(key_size_);
    memset((void*) current_key_, 0, key_size_);
}

KeyGenerator::~KeyGenerator() {
    free(current_key_);
}

void KeyGenerator::IncrementByte(unsigned char* byte) {
    bool carry = true;
    unsigned char mask = 0x01;
    while (carry) {
        carry = ((*byte & mask) != 0x00);
        *byte = *byte ^ mask;
        mask = mask << 1;
    }
}

std::string KeyGenerator::GetNextKey() {
    std::string result(reinterpret_cast<char*>(current_key_), key_size_);
    bool carry = true;
    size_t idx = key_size_ - 1;
    unsigned char* byte;

    while (carry) {
        byte = current_key_ + idx;
        carry = (*byte == 0xff);
        IncrementByte(byte);
        --idx;
    }

    return result;
}

// For debugging key generator
void PrintKey(unsigned char* key, size_t size) {
    uint64_t res = 0;
    int idx = size - 1;
    unsigned char* byte;
    for (unsigned int i = 0; i < size; ++i) {
        byte = key + idx;
        res += (static_cast<int>(*byte) << (8*i));
        --idx;
    }

    std::cout << "key: " << std::hex << res << std::endl;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
