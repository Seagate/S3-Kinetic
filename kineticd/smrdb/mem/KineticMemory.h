/*
 * KineticMemory.h
 *
 *  Created on: Apr 11, 2016
 *      Author: tri
 */

#ifndef KINETIC_MEMORY_H_
#define KINETIC_MEMORY_H_

#include <cstdint>
#include "DynamicMemory.h"

namespace smr {

class KineticMemory {
public:
    KineticMemory(): usage_(0) {
    }
    virtual ~KineticMemory() {
    }
    virtual char* allocate(uint32_t nBytes) {
        char* buff = NULL;
        buff = smr::DynamicMemory::getInstance()->allocate(nBytes);
        if (buff) {
            usage_ += nBytes;
        }
        return buff;
    }
    uint32_t usage() {
        return usage_;
    }
protected:
    uint32_t usage_;
};

} /* namespace smr */

#endif /* KINETIC_MEMORY_H_ */
