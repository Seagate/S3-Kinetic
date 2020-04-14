/*
 * Disk.h
 *
 *  Created on: Apr 18, 2015
 *      Author: tri
 */

#ifndef DISK_H_
#define DISK_H_

#include <iostream>
#include <tuple>
#include <deque>
#include <map>
#include <sys/syscall.h>
#include <sstream>

#include <set>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <linux/fs.h>
#include "smrdisk/DriveEnv.h"
#include "../kmem/kernel_mem_mgr.h"

#include <limits.h>
#if defined(LEVELDB_PLATFORM_ANDROID)
#include <sys/stat.h>
#endif

#include "leveldb/slice.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/posix_logger.h"
#include "db/filename.h"
#include "util/coding.h"
#include "util/env_posix.h"
#include "Util.h"
#include "FileInfo.h"
#include "FileMap.h"
#include "Level.h"
#include "Zone.h"
#include "Superblock.h"

#include "zac_mediator.h"

using namespace std;
using namespace leveldb;
using namespace leveldb::port;
using ::zac_ha_cmd::ZacMediator;
using ::zac_ha_cmd::AtaCmdHandler;
using ::zac_ha_cmd::ZacZone;

const static size_t MAX_SUPERBLOCK_SIZE = 3*1024*1024;//Maximum superblock size
#define START_SUPER_BLOCK_ZONE 61  //supper block zones 61, 62 & 63
#define START_LOG_ZONE 65          //Log zones 65 & 66
#define NUM_LOG_ZONES 2

namespace smr {
class ReplaySegment;

struct DiskError { };
/*
static Status IOError(const std::string& context, int err_number) {
    return Status::IOError(context, strerror(err_number));
}
*/
struct AsyncState {
        int fd;
        uint64_t addr;
        uint64_t size;
        int options;
        bool closeFD;
};

extern pthread_t bgasync_thread_;
extern pthread_mutex_t a_mu_;
extern pthread_cond_t a_signal_;
extern std::deque<void*> a_queue_;
extern bool started_async_thread_;

extern void* BGAsyncSync(void *arg);
extern void BGAsyncQueue(void* arg);

extern pthread_t bg_comp_async_thread_;
extern pthread_mutex_t a_c_mu_;
extern pthread_cond_t a_c_signal_;
extern std::deque<void*> a_c_queue_;
extern bool started_async_compact_thread_;

extern void* BGCompAsyncSync(void *arg);
extern void BGCompAsyncQueue(void* arg);

class Disk {
    static const int NUMBER_SUPERBLOCKS = 3;
public:
    enum class DiskStatus {
        NORMAL,
        NO_SPACE
    };

    static const off_t ZONE_SIZE = 256 * 1048576;               // 256 MB
    // DO NOT DELETE:  Constants for testing
    //static const uint64_t NO_SPACE_THRESHOLD = 7*Disk::ZONE_SIZE;

    // Constants for releases
    static const uint64_t NO_SPACE_THRESHOLD = (uint64_t)10*(1<<30);
    static const uint64_t AVAIL_SPACE_THRESHOLD = 2*NO_SPACE_THRESHOLD;
    static const uint64_t HIGH_DISK_USAGE_THRESHOLD = 10*NO_SPACE_THRESHOLD;


    friend class Superblock;
    private:
        static port::Mutex _mu;

    public:
        static DiskStatus _status;
        static bool isNoSpace() {
            MutexLock l(&_mu);
            return (_status == DiskStatus::NO_SPACE);
        }
        static void noSpace(DiskStatus status) {
            MutexLock l(&_mu);
//            _status = status;
            if (status == DiskStatus::NO_SPACE) {
                //smr::ValueMemory::getInstance()->usedBy(smr::ValueMemory::UsedBy::INTERNAL_ONLY);
            } else {
                //smr::ValueMemory::getInstance()->usedBy(smr::ValueMemory::UsedBy::ALL);
            }
            _status = status;
        }

    public:
        static bool superblockStatus[];
        static const uint64_t MAGIC_NUMBER = 0x534d524b5644534bull;
        static const off_t SUPER_ZONE_SIZE = 5 * 1048576;
        static const uint16_t END_CONV_ZONE = 63;
        // 69 zones are reserved: the 64 conventional zones for manifest, super block, and linux
        // and 1 SMR zone is reserved for the security band and two zones for log and two for pad around log
        static const uint16_t N_RESERVED_ZONES = 69;
        // there are 58 conventional zones that can be used for manifests (64 - 2 super zones - 4 linux (sda1,2,4,5))
        static const uint16_t N_CONV_MANIFEST_ZONES = 57;
        static const string LEVEL_INFO_DIR;                         // "/mnt/util/LevelDBInfo"
        static uint64_t SUPERBLOCK_0_ADDR;
        static uint64_t SUPERBLOCK_1_ADDR;
        static uint64_t SUPERBLOCK_2_ADDR;
        static void initializeSuperBlockAddr(string& diskName);

        friend ostream& operator<<(ostream& out, Disk& disk);

    public:
        inline Disk(const int fd, size_t page_size, const string& name, bool create_if_missing = false);

        ~Disk(){
#ifdef KDEBUG
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << endl;
            cout << *this << endl;
#endif
//            sync();
            for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
                delete *it;
            }
            superblocks_.clear();
            for (int i = 0; i < 10; ++i) {
                delete levels_[i];
            }

            if (zac_kin_->CloseDevice() != 0) {
                printf("Could not close device\n");
            }
            if (current_ != NULL) {
                delete current_;
            }
            if (zoneUseMap_ != NULL) {
                free(zoneUseMap_);
                zoneUseMap_ = NULL;
            }
            ::close(fd_);
            fd_ = -1;
            if (valueFileMap_) {
                valueFileMap_->clear();
                delete valueFileMap_;
            }
            if (fileMap_) {
               fileMap_->clear();
               delete fileMap_;
            }
            delete mu_;
            delete ata_cmd_handler_;
            delete zac_kin_;
            if (buff_) {
                free(buff_);
                buff_ = NULL;
            }
        }

        FileInfo* getFileInfo(uint64_t number, FileType type) {
            if (type == kValueFile) {
                return valueFileMap_->getFileInfo(number, type);
	        }
            return fileMap_->getFileInfo(number, type);
        }
        void obsoleteValueFile(uint64_t fnumber) {
            valueFileMap_->obsoleteFile(fnumber);
        }
        void removeObsoleteValueFile(uint64_t fnumber) {
            valueFileMap_->removeObsoleteFile(fnumber);
        }
        void getFiles(std::vector<std::string>* result){
            fileMap_->getFiles(result);
        }

        uint64_t getCURRENTsize() {
            return current_->size();
        }

        void printZoneUsedMap() {
    if (zoneUseMap_) {
       cout << "Zone use bit map: " << endl;
       //unsigned int* zoneBitMap = (unsigned int*)zoneUseMap_;
       unsigned char* charPtr = zoneUseMap_;
       int arrSize = (zoneUseMapSize_/sizeof(unsigned int));
       cout << " ARRAY SIZE: " << arrSize << endl;
       int j = 0;
       for (uint i = 0; i < sizeof(unsigned int)*arrSize; ++i) {
          printf(" i  %d  %02x ", i, *charPtr);
          ++charPtr;
          ++j;
          if (j % 20 == 0) {
             printf("\n");
             j = 0;
          }
       }
    cout << dec << endl;
    }
}

        void getCapacity(uint64_t* totalBytes, uint64_t* usedBytes) {
            int zonesUsed = 0;
            for (int i = 0; i < 10; ++i) {
                if (i == 8) {  // Exclude log level
                    continue;
                }
                if (i == 7) {
                    // only count manifests if they are in the SMR zones
                    zonesUsed += levels_[i]->getNumZones() > N_CONV_MANIFEST_ZONES ?
                                 (levels_[i]->getNumZones() - N_CONV_MANIFEST_ZONES) : 0;
                } else {
                    zonesUsed += levels_[i]->getNumZones();
                }
            }

            *usedBytes = uint64_t(zonesUsed)*Zone::ZONE_SIZE;
            *totalBytes = usableCapacity_;
        }
        int open(int flags) {
            int fd = ::open(name_.c_str(), flags); //O_RDWR);
            return fd;

        }
        inline Status sync(int fd = -1);
        /////////////////////////////////////////////////////////
        /// zoneUsageLogBuilder(std::string &)
        /// -------------------------------------------
        /// Populate @usage_str with data from function level -> @getZoneHistogram
        /// Using a stringstream as the formatting method, fill the stream with
        /// the zone condition counts from zac_mediator and the histogram data from level
        /// -------------------------------------------
        /// -EXAMPLE Histogram:
        ///    0   ->   [0,0,0]
        ///    1   ->   [0,0,0]
        ///    2   ->   [0,0,20]
        ///    3   ->   [0,421,1095]
        ///    4   ->   [0,8,113]
        ///    5   ->   [0,1,15]
        ///    ---------------------------------
        ///    -Effective SSTs: 6610  -Total Zones: 1673
        ///    ---------------------------------
        ///
        /// -PSEUDO CODE Used to Calculate Totals:
        ///    EffectiveSSTs = 0
        ///    FOREACH (WholeSST: 0 -> 5):
        ///      X = WholeSST Number (i.e. 0 to 5)
        ///      EffectiveSSTs += (X) * (Aggregate #Zones w/ X Whole)
        ///                            + ((#Zones w/ X whole & 1 partial) / 2)
        ///                            + (#Zones w/ X whole & 2 partial)
        ///
        /// -EXTENDED Example:
        ///    0   ->   [0,0,0]      ==   0
        ///    1   ->   [0,0,0]      ==   0
        ///    2   ->   [0,0,20]     ==   2 * (0 + 0 + 20)     + (0 / 2)   + (20)
        ///    3   ->   [0,421,1095] ==   3 * (0 + 421 + 1095) + (421 / 2) + (1095)
        ///    4   ->   [0,8,113]    ==   4 * (0 + 8 + 113)    + (8 / 2)   + (113)
        ///    5   ->   [0,1,15]     ==   5 * (0 + 1 + 15)     + (1 / 2)   + (15)
        ///    ===================================================================
        ///    EffectiveSSTs         ==   (summation of each line result above)
        /// -------------------------------------------
        /// - @param[in] usage_str -- string to populate with formated zone usage data
        /// - @return[out] bool    -- status of internal calls to zac mediator and level histogram
        bool zoneUsageLogBuilder(std::string &usage_str);
        inline Status bgSync(const int fd, uint64_t addr, uint64_t size, bool closeFD);

        inline FileInfo* allocateFile(uint64_t number, FileType type, int level = 0);
        inline Status deallocateFile(const std::string& fname, uint64_t number, FileType type);
        inline Status deallocateFile(uint64_t number, FileType type);

        void putCURRENT(const std::string* curr) {
            *current_ = *curr;
            if (current_->size() > 1) {
                string newCurrent = current_->substr(0, current_->size() - 1);  // remove '\n' char
                uint64_t number = -1;
                FileType type;

                ParseFileName(newCurrent, &number, &type);
                manifestInfo_ = this->fileMap_->getFileInfo(number, type);
                assert(manifestInfo_);
            } else {
                manifestInfo_ = NULL;
            }
        }

        void getCURRENT(size_t n, Slice* result, char* scratch) {
            size_t r = n;
            if (r > current_->size()) {
                r = current_->size();
            }
            memcpy(scratch, current_->data(), current_->size());
            *result = Slice(scratch, r);
        }

        Status defragment(int level);
        Status defragmentExternal(const Options& options, Env::put_func_t putFunc);

        size_t getPageSize() const {
            return pageSize_;
        }

        void freeZone(int index) {
            int superZone = Disk::SUPERBLOCK_0_ADDR/ZONE_SIZE;
            if (index >= superZone + 3 && index <= superZone + 6) {
                return;
            }
            MutexLock l(mu_);
            if (index >= 0 && index < (zoneUseMapSize_ * CHAR_BIT)) {
                unsigned int* array = reinterpret_cast<unsigned int*>(zoneUseMap_);
                array[(index)/(CHAR_BIT * sizeof(unsigned int))] &= ~(1 << ((index) % (CHAR_BIT * sizeof(unsigned int))));
                if (index > Disk::END_CONV_ZONE + 4) {
                   ++nFreeZones_;
                   --nUsedZones_;
                   if (isNoSpace() && uint64_t(nFreeZones_*Disk::ZONE_SIZE) >= Disk::AVAIL_SPACE_THRESHOLD) {
                       Disk::noSpace(Disk::DiskStatus::NORMAL);
                       cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": >>>>>>>>>> Disk space is back to normal" << endl;
                   }
                }
            }
        }
        Zone* allocateZone(FileType type);
        inline int writeToZone(uint64_t lba, void *data, size_t size);
        Level* getLevel(uint32_t level) {
            if ((int)level < 0 || (int)level >= 10) {
               return NULL;
            }
            return levels_[level];
        }
        bool isFragmented(int level) {
            return levels_[level]->isFragmented();
        }
        bool isValueFragmented() {
            return valueFileMap_->isFragmented(name_);
        }
        inline bool isHighDiskUsage();
        int GetNumFreeZones() {
            return this->nFreeZones_;
        }
        bool IsCorrupted() {
            return (this->numGoodSuperblocks_ == 0);
        }
	bool ISE();
        void clear();
        bool checkDiskInfoValid();
        void FillZoneMap();

        bool loadDB() {
            Status s;
            _status = DiskStatus::NORMAL;
            db_status_ = getDiskInfo(fd_);
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": db_status = " << db_status_.ToString() << endl;
            if (db_status_.ok()){
                fprintf(stdout, "Using existing DB\n");
            fsync(fd_);
            this->nFreeZones_ = this->numberSSTFreeZones();
            this->nUsedZones_ = this->numberSSTUsedZones();
            uint64_t total_bytes = 0;
            uint64_t used_bytes = 0;
            this->getCapacity(&total_bytes, &used_bytes);
            if (total_bytes - used_bytes < Disk::NO_SPACE_THRESHOLD) {
                _status = DiskStatus::NO_SPACE;
                //ValueMemory::getInstance()->usedBy(ValueMemory::UsedBy::INTERNAL_ONLY);
            } else {
                //ValueMemory::getInstance()->usedBy(ValueMemory::UsedBy::ALL);
            }
#ifdef KDEBUG
            cout << *this << endl;
            cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit" << endl;
#endif
                return true;
            }
            if (capacity_ < (SUPER_ZONE_SIZE + (5 * ZONE_SIZE))){
                close(fd_);
                fd_ = -1;
                return false;
            } else if ((getNumberGoodSuperblocks() == 0 && db_status_.IsInvalidArgument()) && create_if_missing_) {
                db_status_ = Status::OK();
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Create fresh dbase" << endl;
                zoneStartAddr_ = ZONE_SIZE;
                zoneSize_ = ZONE_SIZE;
                // leave last zone empty to prevent overwritting security band
                numZones_ = (capacity_ / ZONE_SIZE);
                int nIntegers = (numZones_ + sizeof(unsigned int)*CHAR_BIT - 1)/(sizeof(unsigned int)*CHAR_BIT);
                zoneUseMapSize_ = nIntegers*sizeof(unsigned int);
                zoneUseMap_ = (unsigned char *) calloc(zoneUseMapSize_, sizeof(char));
                clearZoneUseMap();
                int superZone = SUPERBLOCK_0_ADDR/ZONE_SIZE;;
                setUsedZone(superZone);
                setUsedZone(superZone + 1);
                setUsedZone(superZone + 2);
                setUsedZone(superZone + 3);  //PAD zone
                setUsedZone(superZone + 4);  //LOG zone
                setUsedZone(superZone + 5);  //LOG zone
                setUsedZone(superZone + 6);  //Pad zone
                currentLogZone_ = superZone + 4;
                current_ = new std::string();
                numGoodSuperblocks_ = 0;
                for (vector<Superblock*>::iterator it = superblocks_.begin(); numGoodSuperblocks_ < 2 && it != superblocks_.end(); ++it) {
                    Superblock* superblock = *it;
                    db_status_ = superblock->create();
                    if (superblock->isGood()) {
                        ++numGoodSuperblocks_;
                    }
                }
                fsync(fd_);
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": status = " << db_status_.code() << ", numGoodSuperblocks_ = " << numGoodSuperblocks_ << ": ";
                for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
                    Superblock* superblock = *it;
                    if (superblock->isGood()) {
                        cout << superblock->number() << " ";
                    }
                }
                cout << endl;
                if (getNumberGoodSuperblocks() == 0) {
                    close(fd_);
                    fd_ = -1;
                    return false;
                } else {
                    db_status_ = Status::OK();
                }
                this->nFreeZones_ = this->numberSSTFreeZones();
                this->nUsedZones_ = this->numberSSTUsedZones();
#ifdef KDEBUG
                cout << *this << endl;
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Exit" << endl;
#endif
                //ValueMemory::getInstance()->usedBy(ValueMemory::UsedBy::ALL);
                return true;
            } else {
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": All superblocks are bad.  " << endl;
                close(fd_);
                fd_ = -1;
                return false;
            }
        }
        int numberSSTFreeZones() {
            int free_zones = 0;
            unsigned int* array = reinterpret_cast<unsigned int*>(zoneUseMap_);

            for (uint32_t i = Disk::END_CONV_ZONE + 5; i < numZones_ - 1; i++) {
                int zone_index = (i)/(CHAR_BIT * sizeof(unsigned int));
                unsigned int flag = (1 << ((i) % (CHAR_BIT * sizeof(unsigned int))));
                if (!(array[zone_index] & flag)) {
                    free_zones += 1;
                }
            }
            return free_zones;

        }
        int numberSSTUsedZones() {
            int used_zones = 0;
            unsigned int* array = reinterpret_cast<unsigned int*>(zoneUseMap_);

            for (uint32_t i = Disk::END_CONV_ZONE + 5; i < numZones_ - 1; i++) {
                int zone_index = (i)/(CHAR_BIT * sizeof(unsigned int));
                unsigned int flag = (1 << ((i) % (CHAR_BIT * sizeof(unsigned int))));
                if (array[zone_index] & flag) {
                    used_zones += 1;
                }
            }
            return used_zones;
        }
        int numberSSTZones() {
            return numZones_ - Disk::N_RESERVED_ZONES;
        }
        int getZonesUsedInZoneMap() {
            int used_zones = 0;
            unsigned int* array = reinterpret_cast<unsigned int*>(zoneUseMap_);

            for (int i=0; i < (int)numZones_; i++) {
                int zone_index = (i-1)/(CHAR_BIT * sizeof(unsigned int));
                unsigned int flag = (1 << ((i-1) % (CHAR_BIT * sizeof(unsigned int))));
                if (array[zone_index] & flag) {
                    used_zones += 1;
                }
            }
            return used_zones;
        }
       void fileDeleted(uint64_t fnumber, FileType ftype) {
            for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
                if ((*it)->isGood()) {
                    (*it)->fileDeleted(fnumber, ftype);
                }
            }
        }
       void segmentCompleted(ReplaySegment* seg) {
           for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
               if ((*it)->isGood()) {
                   (*it)->segmentCompleted(seg);
               }
           }
       }
       void segmentUpdated(ReplaySegment* seg) {
           for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
               if ((*it)->isGood()) {
                   (*it)->segmentUpdated(seg);
               }
           }
       }
        void segmentDeallocated(ReplaySegment* seg) {
            for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
                if ((*it)->isGood()) {
                    (*it)->segmentDeallocated(seg);
                }
            }
        }
        uint32_t getNumZones() {
            return numZones_;
        }
        int getNumberGoodSuperblocks() {
            int nGoodSuperblocks = 0;
            for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
                Superblock* superblock = *it;
                if (superblock->isGood()) {
                    ++nGoodSuperblocks;
                }
            }
            return nGoodSuperblocks;
        }
        Status status() const {
            return status_;
        }
        void updateFileValueInfo(FileInfo* file_info) {
             for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
                 if ((*it)->isGood()) {
                     (*it)->updateFileValueInfo(file_info);
                 }
             }
         }
        bool isBlockedFile(uint64_t fnumber) {
            return valueFileMap_->isBlocked(fnumber);
        }
        void getObsoleteValueFiles(set<uint64_t>& obsoleteFiles) {
            valueFileMap_->getObsoleteFiles(obsoleteFiles);
        }

    private:
        void clearZoneUseMap();
        bool isSuperBlockGood(int fd, uint64_t addr);
//  Tri - Save      inline char *computeMD5(const char *str, int length);
        Status getDiskInfo(const int fd);
        inline int getFreeZone(FileType type);
        inline Status replayDeleteFile(const std::string& fname, uint64_t number, FileType type);

        void setUsedZone(int index) {
            if (index >= 0 && index < (zoneUseMapSize_ * CHAR_BIT)) {
                unsigned int* array = reinterpret_cast<unsigned int*>(zoneUseMap_);
                if (index <= END_CONV_ZONE) {
                    // zone is conventional, update conventional zone members
                    lastAllocatedConvZone_ = index;
                    lastAllocatedConvZoneMapIdx_ = (index)/(CHAR_BIT * sizeof(unsigned int));
                    array[lastAllocatedConvZoneMapIdx_] |= (1 << ((index) % (CHAR_BIT * sizeof(unsigned int))));
                } else {
                    // zone is nonconventional, update nonconventional zone members
                    lastAllocatedZone_ = index;
                    lastAllocatedZoneMapIdx_ = (index)/(CHAR_BIT * sizeof(unsigned int));
                    array[lastAllocatedZoneMapIdx_] |= (1 << ((index) % (CHAR_BIT * sizeof(unsigned int))));
                }
                if (index > Disk::END_CONV_ZONE + 4) {
                   ++nUsedZones_;
                   --nFreeZones_;
                }
            }

        }
        Status moveSegment(Segment* segment, Level* level, FileType type);
        inline uint64_t superBlockAddress(int iSuperblock);
    private:
        port::Mutex *mu_;
        uint64_t capacity_;
        uint64_t usableCapacity_;
        size_t pageSize_;
        uint32_t numZones_;
        int numGoodSuperblocks_;
        uint64_t zoneSize_;
        uint64_t  zoneStartAddr_;
        std::string *current_;
        FileMap* fileMap_;
	    FileMap* valueFileMap_;
        unsigned char *zoneUseMap_;
        int zoneUseMapSize_;
        string name_;
        FileInfo* manifestInfo_;
        Level* levels_[10];
        int lastAllocatedZoneMapIdx_;
        int lastAllocatedZone_;
        int lastAllocatedConvZoneMapIdx_;
        int lastAllocatedConvZone_;
        int fd_;
        ZacMediator *zac_kin_;
        uint32_t currentLogZone_;
        AtaCmdHandler *ata_cmd_handler_;
        Status db_status_;
        bool create_if_missing_;
        char* buff_;
        vector<Superblock*> superblocks_;
        Status status_;

    public:
        int nUsedZones_;
        int nFreeZones_;
};

inline Disk::Disk(const int fd, size_t page_size, const string& name, bool create_if_missing){
    mu_ = new port::Mutex();
    fileMap_ = NULL;
    valueFileMap_ = NULL;
    if(ioctl(fd, BLKGETSIZE64, &capacity_)<0) {
        close(fd);
        throw DiskError();
    }
    else if (capacity_ < 5*ZONE_SIZE) {
        close(fd);
    }
    buff_ = NULL;
    int s = posix_memalign((void**)&buff_, page_size, Disk::SUPER_ZONE_SIZE);
    if (buff_ == NULL || s != 0) {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__
           << ":CAN NOT ALLOCATE MEMORY IN WRITABLE";
        Status::IOError(ss.str());
    } else {
        memset(buff_, 0, MAX_SUPERBLOCK_SIZE);
    }
    numGoodSuperblocks_ = 0;
    fileMap_ = new FileMap();
    valueFileMap_ = new FileMap();
    // DO NOT DELETE the following line. This is used to test defrag and storage full
    //capacity_ = (uint64_t)138*Disk::ZONE_SIZE;  //170 zones => 5 integers. 138 zones, 4 integers.  110 zones
    //capacity_ = (uint64_t)1*1024*1024*1024*1024;
    //capacity_ = (uint64_t)200*1024*1024*1024;  //170 zones => 5 integers. 138 zones, 4 integers.  110 zones
    //capacity_ = (uint64_t)138*256*1024*1024;  //170 zones => 5 integers. 138 zones, 4 integers.  110 zones
    // capacity_ = (uint64_t)2*1024*1024*1024*1024;
    usableCapacity_ = capacity_ - (uint64_t(N_RESERVED_ZONES) * Zone::ZONE_SIZE);
    fd_ = fd;
    name_ = name;
    manifestInfo_ = NULL;
    pageSize_ = page_size;
    lastAllocatedZoneMapIdx_ = (END_CONV_ZONE)/(CHAR_BIT * sizeof(unsigned int));
    lastAllocatedZone_ = END_CONV_ZONE;
    lastAllocatedConvZoneMapIdx_ = 0;
    lastAllocatedConvZone_ = 0;
    create_if_missing_ = create_if_missing;
    ata_cmd_handler_ = new AtaCmdHandler();
    zac_kin_ = new ZacMediator(ata_cmd_handler_);
    uint64_t lba = 0;
    // Make sure that log pad zones have write pointer reset
    current_ = NULL;
    zoneUseMap_ = NULL;
    if (zac_kin_->OpenDevice(name_) < 0) {
        printf("Could not open device\n");
        throw DiskError();
    }
    zac_kin_->AllocateZone(START_LOG_ZONE - 1, &lba);
    zac_kin_->AllocateZone(START_LOG_ZONE + NUM_LOG_ZONES, &lba);
    // Initialize levels
    for (uint32_t i = 0; i < 10; ++i) {
        levels_[i] = new Level(i, this);
    }
    nUsedZones_ = 0;
    nFreeZones_ = numZones_;
    for (int i = 0; i < Disk::NUMBER_SUPERBLOCKS; ++i) {
        Superblock* superblock = new Superblock(this, i);
        superblocks_.push_back(superblock);
    }
}

inline uint64_t Disk::superBlockAddress(int iSuperblock) {
    uint64_t addr = -1;
    if (iSuperblock == 0) {
        addr = Disk::SUPERBLOCK_0_ADDR;
    } else if (iSuperblock == 1) {
        addr = Disk::SUPERBLOCK_1_ADDR;
    } else if (iSuperblock == 2) {
        addr = Disk::SUPERBLOCK_2_ADDR;
    }
    return addr;
}

inline int Disk::getFreeZone(FileType type) {
    int n_conv = END_CONV_ZONE / (sizeof(unsigned int) * CHAR_BIT);
    int n = zoneUseMapSize_ / sizeof(unsigned int);
    unsigned int* array = reinterpret_cast<unsigned int*>(zoneUseMap_);
    int result = -1;
    int i;

    if (type == kDescriptorFile) {
        // Try to allocate a conventional zone
        if (END_CONV_ZONE % (sizeof(unsigned int) * CHAR_BIT) != 0) {
            // we have an element in the zoneUseMap that has both conventional and SMR zones
            // so increment the number of conventional zoneUseMap indicies
            ++n_conv;
        }
        i = (lastAllocatedConvZoneMapIdx_ % n_conv);

        for (int j = 0; j < 2; ++j) {  // Loop back to begin of zoneMap_
            i %= n_conv;
            for (; i < n_conv; ++i) {
                unsigned int arrElement = array[i];
                int bitIdx = lastAllocatedConvZone_ - (i * sizeof(unsigned int) * CHAR_BIT);
                //The following 3 lines are for fixing X86 Compiler's bug
                if(bitIdx == 32) {
                    arrElement = 0;
                } else {
                    arrElement = ~arrElement;
                    arrElement >>= bitIdx;
                }
                result = __builtin_ffs(arrElement);
                if (result != 0) {
                    result += bitIdx;
                    result += (i * sizeof(unsigned int) * CHAR_BIT);
                    if (result > END_CONV_ZONE) {
                        continue;
                    }
                    return result - 1;
                }
                lastAllocatedConvZone_ = ((i + 1) * sizeof(unsigned int) * CHAR_BIT);
            }
            lastAllocatedConvZoneMapIdx_ = 0;
            lastAllocatedConvZone_ = 0;
        }
    } else if (type == kLogFile) {
        if (currentLogZone_ == START_LOG_ZONE) {
            currentLogZone_ = START_LOG_ZONE + 1;
        } else {
            currentLogZone_ = START_LOG_ZONE;
        }
        return (currentLogZone_);

    }
    if (type != kUnknown && nFreeZones_ == 1) {
        return -1;
    }
    // Non-conventional zone allocation
    i = (lastAllocatedZoneMapIdx_ % n);

    for (int j = 0; j < 2; ++j) {  // Loop back to begin of zoneMap_
        i %= n;
        if (i < int(END_CONV_ZONE / (sizeof(unsigned int) * CHAR_BIT))) {
            i = int((END_CONV_ZONE + 4 ) / (sizeof(unsigned int) * CHAR_BIT));
        }
        for (; i < n; ++i) {
            unsigned int arrElement = array[i];
            int bitIdx = lastAllocatedZone_ - (i * sizeof(unsigned int) * CHAR_BIT);
            //The following 3 lines are for fixing X86 Compiler's bug
            if (bitIdx == 32) {
                arrElement = 0;
            } else {
                arrElement = ~arrElement;
                arrElement >>= bitIdx;
            }
            result = __builtin_ffs(arrElement);
            if (result != 0) {
                result += bitIdx;
                result = (i * sizeof(unsigned int) * CHAR_BIT) + result;
                return result - 1;
            }
            lastAllocatedZone_ = ((i + 1) * sizeof(unsigned int) * CHAR_BIT);
        }
        lastAllocatedZoneMapIdx_ = END_CONV_ZONE/ (CHAR_BIT * sizeof(unsigned int));
        lastAllocatedZone_ = END_CONV_ZONE + 4;
    }
    return result;
}

/* Tri - Save
inline char *Disk::computeMD5(const char *str, int length) {
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char *out = (char*)malloc(33);

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, str, 512);    cout << " SB SIZE " << sb_size << endl;

        } else {
            MD5_Update(&c, str, length);
        }
        length -= 512;
        str += 512;
    }

    MD5_Final(digest, &c);

    for (n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 2*MD5_DIGEST_LENGTH, "%02x", (unsigned int)digest[n]);
    }
    cout << "MD5 = " << (void*)digest << endl;
    return out;
}
*/
inline Status Disk::sync(int fd) {
    MutexLock l(mu_);
    if (fd < 0) {
        fd = fd_;
    }
#ifndef NDEBUGW
        cout << " ***1. FSYNC DISK "  << endl;
        uint64_t start, end;
        start = DriveEnv::getInstance()->NowMicros();
#endif

    fdatasync(fd); //Flush any pending writes (SST/Manifest) prior to writing superblocks
#ifndef NDEBUGW
        end = DriveEnv::getInstance()->NowMicros();
        cout << "  ***1. FSYNC END DISK  " <<  (end-start) << endl;
        start = end;
#endif
    bool superblockStatusChanged = false;
    Superblock* goodSuperblock = NULL;
    Status status;
    Superblock* newBadSuperblock = NULL;
    bool persisted = false;  // Flag to indicate we have an up to date superblock.
    uint64_t curSeqNum = 0;
    if (numGoodSuperblocks_ == 2) {
        for (vector<Superblock*>::iterator it = superblocks_.begin(); numGoodSuperblocks_ == 2 && it != superblocks_.end(); ++it) {
            Superblock* superblock = *it;
            if (superblock->isGood()) {
                curSeqNum = superblock->seqNum();
                status = superblock->persistProgress();
                if (!status.ok()) {
                    --numGoodSuperblocks_;
                    newBadSuperblock = superblock;
                    superblockStatusChanged = true;
                } else {
                    persisted = true;
                }
            }
        }
    }
    if (numGoodSuperblocks_ == 2) {
        // Delta was persisted for 2 superblocks -- Nothing to do
    } else {
       if (numGoodSuperblocks_ > 0 && numGoodSuperblocks_ < 2) {
           // Get a good superblock
           for (vector<Superblock*>::iterator it = superblocks_.begin(); goodSuperblock == NULL && it != superblocks_.end(); ++it) {
               Superblock* superblock = *it;
               if (superblock->isGood()) {
                   curSeqNum = superblock->seqNum();
                   goodSuperblock = superblock;
               }
           }
           // There is less than 2 good superblocks; persist snapshot onto old bad superblocks until there are 2 good ones.
           for (vector<Superblock*>::iterator it = superblocks_.begin(); numGoodSuperblocks_ < 2 && it != superblocks_.end(); ++it) {
               Superblock* superblock = *it;
               if (!superblock->isGood() && superblock != newBadSuperblock) {
                   superblock->seqNum(curSeqNum);
                   status = superblock->persistSnapshot();
                   if (status.ok()) {
                       persisted = true;
                       ++numGoodSuperblocks_;
                       superblockStatusChanged = true;
                   }
               }
            }
        }
    }
    if (numGoodSuperblocks_ == 2 && goodSuperblock != NULL) {
        // Snapshot was created on a bad superblock; create snapshot onto the good superblock too
        goodSuperblock->seqNum(curSeqNum);
        status = goodSuperblock->persistSnapshot();
        if (!status.ok()) {
            --numGoodSuperblocks_;
            superblockStatusChanged = true;
        }
    }
    if (persisted) {
        status_ = Status::OK();
    } else {
        if (numGoodSuperblocks_ == 1) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to persist superblocks." << endl << status.ToString();
            status_ = Status::SuperblockIO(ss.str());
        } else {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": All superblock are bad" << endl << status.ToString();
            status_ = Status::Corruption(ss.str());
        }
    }
    status = status_;
    if (superblockStatusChanged) {
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": status = " << status_.code() << ", numGoodSuperblocks_ = " << numGoodSuperblocks_ << ": ";
        for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
            Superblock* superblock = *it;
            if (superblock->isGood()) {
                cout << superblock->number() << " ";
            }
        }
        cout << endl;
    }
    return status;
}

inline Status Disk::bgSync(const int fd, uint64_t addr, uint64_t size, bool closeFD) {
    Status s;
    AsyncState *state = new AsyncState;
    state->fd = fd;
    state->addr = addr;
    state->size = size;
    state->options = 1;
    state->closeFD = closeFD;
    BGAsyncQueue(state);
    return s;
}

inline FileInfo* Disk::allocateFile(uint64_t number, FileType type, int level) {
    if (type == kDescriptorFile) {
        level = 7;
    } else if (type == kLogFile) {
        level = 8;
    } else if (type == kValueFile) {
        level = 9;
    }
    FileInfo* finfo = levels_[level]->allocateFile(number, type);
    if (finfo == NULL) {
        #ifdef KDEBUG
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": finfo == NULL" << endl;
        #endif //KDEBUG
        return NULL;
    }
    // Value files and table files may share the same level. This allows 'left-over' capacity
    // in value file zones to be used by table files.
    if (type == kValueFile) {
      valueFileMap_->addFileInfo(finfo->getNumber(), finfo);
    }
    else {
      fileMap_->addFileInfo(finfo->getNumber(), finfo);
    }
    for (vector<Superblock*>::iterator it = superblocks_.begin(); it != superblocks_.end(); ++it) {
        if ((*it)->isGood()) {
            (*it)->fileCreated(finfo);
        }
    }
    return finfo;
}

inline int Disk::writeToZone(uint64_t lba, void *data, size_t size) {
    MutexLock l(mu_);
    return (zac_kin_->WriteZone(lba, data, size));
}

inline Status Disk::deallocateFile(const std::string& fname, uint64_t number, FileType type) {
    FileInfo* finfo;
    if ( type == kValueFile) {
        finfo = valueFileMap_->getFileInfo(number, type);
    } else {
        finfo = fileMap_->getFileInfo(number, type);
    }
    if (finfo == NULL) {
        stringstream context;
        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
        return Status::NotFound(context.str());
    }
    if (type == kValueFile) {
        valueFileMap_->removeFileInfo(number, type);
    } else {
        fileMap_->removeFileInfo(number, type);
    }
    uint32_t level = finfo->getLevel()->getNumber();
    levels_[level]->deallocateFile(finfo);
    this->fileDeleted(number, type);
    delete finfo;
    return Status::OK();
}
inline Status Disk::replayDeleteFile(const std::string& fname, uint64_t number, FileType type) {
    FileInfo* finfo;
    if ( type == kValueFile) {
        finfo = valueFileMap_->getFileInfo(number, type);
    } else {
        finfo = fileMap_->getFileInfo(number, type);
    }
    if (finfo == NULL) {
        stringstream context;
        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
        return Status::NotFound(context.str());
    }
    if (type == kValueFile) {
        valueFileMap_->removeFileInfo(number, type);
    } else {
        fileMap_->removeFileInfo(number, type);
    }
    uint32_t level = finfo->getLevel()->getNumber();
    levels_[level]->deallocateFile(finfo);
    delete finfo;
    return Status::OK();
}

inline bool Disk::isHighDiskUsage() {
    uint64_t totBytes = 0;
    uint64_t usedBytes = 0;
    getCapacity(&totBytes, &usedBytes);
    return (totBytes - usedBytes < Disk::HIGH_DISK_USAGE_THRESHOLD);
}

}  // namespace smr

#endif /* DISK_H_ */
