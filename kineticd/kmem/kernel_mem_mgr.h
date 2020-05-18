#ifndef KERNEL_MEM_MGR_H_
#define KERNEL_MEM_MGR_H_

#include <string>
#include <pthread.h>

#define TRUE 1
#define FALSE 0

#define KERNEL_MEM_SIZE (1024*1024 + 4096)
#define NUM_ALLOCATED_BLOCK 20

using namespace std;

class KernelMemMgr {
	private:
		KernelMemMgr();

		bool munmap();
	public:
		static KernelMemMgr* pInstance_;
		bool Init();

	public: 
		virtual ~KernelMemMgr();

	public:
		static KernelMemMgr* GetInstance();
	
	public:
		void* AllocMem();
		void FreeMem(void* pLogicalAddr);
		string ToString() const;

	private:
		static const int NUM_BLOCKS = NUM_ALLOCATED_BLOCK;
		static const size_t BLOCK_SIZE_1 = KERNEL_MEM_SIZE;
		static pthread_mutex_t memMgrMutex;
                int num_allocated_;
                int max_num_allocated_;
		int fd_;
		bool memBlockInUse_[NUM_BLOCKS];
		unsigned long* pPhysMemAddresses_;
		void* virtMemAddresses_[NUM_BLOCKS];
		
};

#endif
