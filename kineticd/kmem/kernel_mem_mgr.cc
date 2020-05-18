#include "kernel_mem_mgr.h"

#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

using namespace std;


pthread_mutex_t KernelMemMgr::memMgrMutex = PTHREAD_MUTEX_INITIALIZER;
KernelMemMgr* KernelMemMgr::pInstance_ = NULL;

KernelMemMgr* KernelMemMgr::GetInstance() {
	if (!pInstance_) {
		pInstance_ = new KernelMemMgr();
		pInstance_->Init();
	}
	return pInstance_;
}

KernelMemMgr::KernelMemMgr() {
	fd_ = -1;
	// Clear all data members
	pPhysMemAddresses_ = NULL;
	for (int i = 0; i < NUM_BLOCKS; i++) {
		memBlockInUse_[i] = FALSE;
		virtMemAddresses_[i] = NULL;
	}
	num_allocated_ = 0;
        max_num_allocated_ = 0;
}

KernelMemMgr::~KernelMemMgr() {
	munmap();
}


bool KernelMemMgr::Init() {
   pthread_mutex_lock(&memMgrMutex);
   if (fd_ >= 0) {
      pthread_mutex_unlock(&memMgrMutex);
      return true;
   }
   /* Open the device */
   fd_ = open("/dev/memMgr", O_RDWR);
   if (fd_ < 0) {
	perror("Failed to open /dev/memMgr.");
        pthread_mutex_unlock(&memMgrMutex);	
	return false;
   }

   /* Map the Physical Adresses Table */
   pPhysMemAddresses_ = (unsigned long*) mmap(0, NUM_BLOCKS*sizeof(unsigned long), //void *), 
		    PROT_READ, MAP_SHARED, fd_, 0);
  
   if (pPhysMemAddresses_ == MAP_FAILED) {
      perror("Failed to map physical memory.");
      pthread_mutex_unlock(&memMgrMutex);     
      return false;
   }

   /* Map virtual memory addresses */
   unsigned long * pPhysAddr = pPhysMemAddresses_;

   for(int i=0; i<NUM_BLOCKS; i++) {
      memBlockInUse_[i] = FALSE;
      //off_t offset = (off_t)*pPhysAddr;
      unsigned long offset = *pPhysAddr;
#ifdef defined(__aarch64__)
      virtMemAddresses_[i] = mmap((void*)offset, BLOCK_SIZE_1, PROT_READ|PROT_WRITE,
                                 MAP_SHARED, fd_, 0);
#else
      virtMemAddresses_[i] = mmap(0, BLOCK_SIZE_1, PROT_READ|PROT_WRITE,
                                MAP_SHARED, fd_, offset);
#endif
      if (virtMemAddresses_[i] == MAP_FAILED) {
	  virtMemAddresses_[i] = NULL;
	  perror("Failed to map virtual memory address");
          pthread_mutex_unlock(&memMgrMutex);	  
	  return false;
      }
      ++pPhysAddr;
   }
   pthread_mutex_unlock(&memMgrMutex);
   return true;
}

string KernelMemMgr::ToString() const {
	string s("fd_ = ");
	s += fd_ + "\n";
	s += string("memBlockInUse: ") + (memBlockInUse_[0]==TRUE? "TRUE": "FALSE");
	for (int i = 1; i < NUM_BLOCKS; i++) {
		s += string(", ") + (memBlockInUse_[i]==TRUE? "TRUE" : "FALSE");
	}
	s += "\n";
	return s;
}

void KernelMemMgr::FreeMem(void *logicalAddr) {
   pthread_mutex_lock(&memMgrMutex);

   /* Search for the logicalAddr */
   int i;
   for ( i = 0; i < NUM_BLOCKS; i++) {
      if ((logicalAddr >= virtMemAddresses_[i]) && ((char*)logicalAddr < (char*)virtMemAddresses_[i]+ BLOCK_SIZE_1)) {
         memBlockInUse_[i] = FALSE;
         num_allocated_--; 
         break;
      }
   }
//   if( num_allocated_ == 0) {
//       cout << "KERNEL FREE " << num_allocated_ << endl;
//   }
   pthread_mutex_unlock(&memMgrMutex);
}

void* KernelMemMgr::AllocMem() {   
   pthread_mutex_lock(&memMgrMutex);
   void* logicalAddress = NULL;

   int i;
   /* Search an unused block */
   for(i=0; i < NUM_BLOCKS; i++) {
      if (memBlockInUse_[i] == FALSE) {
         logicalAddress = virtMemAddresses_[i];
         memBlockInUse_[i] = TRUE;
         num_allocated_++;
	 break;
      }
   }
   if(num_allocated_ > max_num_allocated_) {
       max_num_allocated_ = num_allocated_;
      cout << " MAX KERNEL MEM USED " << max_num_allocated_ << endl;
   }
   pthread_mutex_unlock(&memMgrMutex);
   return logicalAddress;
}

bool KernelMemMgr::munmap() {
   pthread_mutex_lock(&memMgrMutex);
   if (fd_ < 0) {
      pthread_mutex_unlock(&memMgrMutex);
      return true;
   }
   int status= 0;

   /* Free all blocks */
   for (int i=0;i<NUM_BLOCKS;i++) {
      if (virtMemAddresses_[i] != NULL) {
          status = ::munmap(virtMemAddresses_[i] , BLOCK_SIZE_1);
          if (status != 0) {
             perror("Failed to unmap virtual memory.");
             pthread_mutex_unlock(&memMgrMutex);	     
             return false;
          }
          virtMemAddresses_[i] = NULL;
      }
      memBlockInUse_[i] = FALSE;
   }

   /* Unmap the Physical Adresses Table */
   status = ::munmap(pPhysMemAddresses_, NUM_BLOCKS*sizeof(unsigned long)); //void *));
  
   if (status != 0) {
      perror("Failed to unmap physical memory");
      pthread_mutex_unlock(&memMgrMutex);     
      return false;
   }

   pPhysMemAddresses_ = NULL;
   /* close the device */
   close(fd_);
   fd_ = -1;
   pthread_mutex_unlock(&memMgrMutex);
   return true;
}

