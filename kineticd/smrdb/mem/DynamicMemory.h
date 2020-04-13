#ifndef DYNAMICMEMORY_H_
#define DYNAMICMEMORY_H_
/*
 * DynamicMemory.h
 *
 *  Created on: Apr 11, 2016
 *      Author: tri
 */
#include <stddef.h>
#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "smrdisk/Util.h"
#include "common/Listener.h"
#include "common/Listenable.h"

using namespace leveldb;
using namespace leveldb::port;
using namespace com::seagate::common;
#define ALIGNED_MEM_SIZE_1M  1*1024*1024
#define ALIGNED_MEM_SIZE_5M  5*1024*1024


enum class MEMORYType : uint8_t {
  MEM_FOR_CLIENT,         // for clients.
  MEM_FOR_DEFRAG,         //  for defragmentation
  INVALID
};

namespace smr {

class AtomicCounter {
 private:
  port::Mutex mu_;
  uint32_t count_;
 public:
  AtomicCounter() : count_(0) {};
  void IncrementBy(uint32_t count);
  void DecrementBy(uint32_t count);
  uint32_t Read();
  void Reset();
};

class DynamicMemory : public Listenable {
private:
    static DynamicMemory* _instance;

public:

    static DynamicMemory* getInstance() {
        if (_instance == NULL) {
	    _instance = new DynamicMemory();
	}
	return _instance;
    }
    DynamicMemory(): max_so_far_(0), totalUsage_(0), clientUsage_(0), defragUsage_(0), cv_(&mu_) {
    };
    uint32_t GetClientMemUsage();
    char* allocate(uint32_t nBytes, bool defrag = false);
    void deallocate(uint32_t nBytes, bool defrag = false);
    uint32_t usage() {
        return totalUsage_;
    }

 private:
  uint32_t max_so_far_;
  uint32_t totalUsage_;
  uint32_t clientUsage_;
  uint32_t defragUsage_;
  port::Mutex mu_;
  port::CondVar cv_;
};
} /* namespace smr */

#endif /* DYNAMICMEMORY_H_ */
