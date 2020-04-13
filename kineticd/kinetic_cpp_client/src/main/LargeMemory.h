/*
 * LargeMemory.h
 *
 *  Created on: Apr 11, 2018
 *      Author: tri
 */

#ifndef LARGEMEMORY_H_
#define LARGEMEMORY_H_

#include <cstddef>
#include <cstdlib>
#include <assert.h>

//#include "smrdb/smrdisk/Util.h"

//using namespace smr;

namespace kinetic {

#define ROUNDUP(x, y)  ((((x) + (y) - 1) / (y)) * (y))
#define TRUNCATE(x, y) ((x) - ((x) & ((y) - 1)))

class LargeMemory {
    public:
        LargeMemory();
        virtual ~LargeMemory();
        bool allocate(int size);
        char* getNext(int &size);

        int size() {
            return size_;
        }

        char* getStart(int& size) {
            blockIdx_ = 0;
            return getNext(size);
        }

    private:
        int maxBlockSize_;
        int size_;
        int nBlocks_;
        int blockIdx_;
        char** blocks_;
};

} /* namespace kinetic */

#endif /* LARGEMEMORY_H_ */
