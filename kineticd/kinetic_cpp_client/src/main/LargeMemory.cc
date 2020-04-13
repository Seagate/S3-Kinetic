/*
 * LargeMemory.cc
 *
 *  Created on: Apr 11, 2018
 *      Author: tri
 */

#include "LargeMemory.h"

namespace kinetic {

LargeMemory::LargeMemory() {
    size_ = 0;
    blocks_ = NULL;
    nBlocks_ = 0;
    maxBlockSize_ = 0;
    blockIdx_ = 0;
}

LargeMemory::~LargeMemory() {
    for (int i = 0; i < nBlocks_; ++i) {
        if (blocks_[i]) {
            free(blocks_[i]);
        }
    }
    delete []blocks_;
}

bool LargeMemory::allocate(int size) {
    if (size <= 0) {
        return false;
    }
    maxBlockSize_ = 1024*1024;
    nBlocks_ = ROUNDUP(size, maxBlockSize_)/maxBlockSize_;
    blocks_ = new char*[nBlocks_];
    // Initilize all blocks with NULL
    for (int i = 0; i < nBlocks_; ++i) {
        blocks_[i] = NULL;
    }
    // Allocate all memory blocks except the last one
    bool bSuccess = true;
    for (int i = 0; i < nBlocks_ - 1 && bSuccess; ++i) {
        int s = posix_memalign((void**)&(blocks_[i]), 4096, ROUNDUP(maxBlockSize_,4096));
        if (s != 0) {
            blocks_[i] = NULL;
            bSuccess = false;
        }
    }
    if (bSuccess) {
        // Allocate the last memory block
        blocks_[nBlocks_ - 1] = new char[size - (nBlocks_ - 1)*maxBlockSize_];
        int s = posix_memalign((void**)&(blocks_[nBlocks_ -1]), 4096, ROUNDUP((size - (nBlocks_ - 1)*maxBlockSize_),4096));
        if (s != 0) {
            blocks_[nBlocks_ - 1] = NULL;
            bSuccess = false;
        }
    }
    size_ = size;
    return bSuccess;
}

char* LargeMemory::getNext(int &size) {
    if (blockIdx_ >= nBlocks_) {
        size = 0;
        return NULL;
    }
    if (blockIdx_ < nBlocks_ - 1) {  // Not the last block
        size = maxBlockSize_;
    } else {
        size = size_ - blockIdx_*maxBlockSize_;
    }
    return blocks_[blockIdx_++];
}

} /* namespace kinetic */
