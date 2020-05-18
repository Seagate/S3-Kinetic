#include "DynamicMemory.h"
#include <iostream>
using namespace std;

namespace smr {

void AtomicCounter::IncrementBy(uint32_t count) {
    MutexLock l(&mu_);
    count_ += count;
}
void AtomicCounter::DecrementBy(uint32_t count) {
    MutexLock l(&mu_);
    count_ -= count;
}
uint32_t AtomicCounter::Read() {
    MutexLock l(&mu_);
    return count_;
}
void AtomicCounter::Reset() {
    MutexLock l(&mu_);
    count_ = 0;
}

DynamicMemory* DynamicMemory::_instance = NULL;
const uint32_t D_MEMORY_SIZE = 192*1024*1024;

uint32_t DynamicMemory::GetClientMemUsage() {
        MutexLock l(&mu_);
        uint32_t usage = clientUsage_;
        return clientUsage_;
}

char* DynamicMemory::allocate(uint32_t nBytes, bool defrag) {
        MutexLock l(&mu_);
        while ( clientUsage_ >= D_MEMORY_SIZE && !defrag){
            if (clientUsage_ >= D_MEMORY_SIZE) {
               cv_.TimedWait();
            }
        }
        char *buf = NULL;
        int s = posix_memalign((void**)&buf, 4096, ROUNDUP(nBytes,4096));
        if (s != 0) {
            buf = NULL;
        } else {
            totalUsage_ += ROUNDUP(nBytes, 4096);
            if (!defrag) {
                clientUsage_ += ROUNDUP(nBytes, 4096);
            } else {
                defragUsage_ += ROUNDUP(nBytes, 4096);
            }
        }
        return buf;
}
void DynamicMemory::deallocate(uint32_t nBytes, bool defrag) {
        MutexLock l(&mu_);
        totalUsage_ -= ROUNDUP(nBytes, 4096);
        if (!defrag) {
            clientUsage_ -= ROUNDUP(nBytes, 4096);
        } else {
            defragUsage_ -= ROUNDUP(nBytes, 4096);
        }
        cv_.SignalAll();
    }
} /* namespace smr */
