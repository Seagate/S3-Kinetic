/*
 * Superblock.h
 *
 *  Created on: Nov 18, 2016
 *      Author: tri
 */

#ifndef SUPERBLOCK_H_
#define SUPERBLOCK_H_

#include <vector>

#include "leveldb/slice.h"
#include "db/log_writer.h"
#include "smrdisk/FileMap.h"
#include "DriveEnv.h"
#include "db/filename.h"
#include "util/mutexlock.h"
#include "common/Listener.h"
#include "smrdisk/ReplaySegment.h"
#include "smrdisk/ReplayFileInfo.h"
#include "smrdisk/ReplayValueFileInfo.h"

using namespace com::seagate::common;
using namespace leveldb;

namespace smr {

class Superblock {
public:
    static const int ELEMENT_ALIGNMENT = 4096;

public:
    Superblock(Disk* disk, int superblockNumber);
    virtual ~Superblock();

    Status create();
    Status load();
    Status persist();

    void segmentCompleted(ReplaySegment* seg) {
        MutexLock l(&mu_);
        seg->ref();
        seg->tag(ReplayObj::Tag::kNewSegment);
        replayObjs_.push_back(seg);
    }
    void segmentUpdated(ReplaySegment* seg) {
        MutexLock l(&mu_);
        seg->ref();
        seg->tag(ReplayObj::Tag::kUpdateSegment);
        replayObjs_.push_back(seg);
    }
    void segmentDeallocated(ReplaySegment* seg) {
        MutexLock l(&mu_);
        seg->ref();
        seg->tag(ReplayObj::Tag::kDeallocatedSegment);
        replayObjs_.push_back(seg);
    }
    void fileDeleted(FileInfo* finfo) {
        fileDeleted(finfo->getNumber(), finfo->getType());
    }
    void fileDeleted(uint64_t fnumber, FileType ftype) {
        MutexLock l(&mu_);
        ReplayFileInfo* replayFileInfo = new ReplayFileInfo();
        replayFileInfo->init(ReplayObj::Tag::kDeletedFile, fnumber, ftype);
        replayObjs_.push_back(replayFileInfo);
    }
    void fileCreated(FileInfo* finfo);
    bool isGood() const {
        return bGood_;
    }
    void good(bool bGood) {
        bGood_ = bGood;
    }
    bool isValid() const {
        return (bGood_ && seqNum_ > 0);
    }
    uint64_t address();
    Status persistSnapshot();
    Status persistProgress();
    int number() const {
         return number_;
     }
    Status markStatus(bool status);
    void clearUpdates();
    void transferDiskInfoFromDisk(Disk* disk);
    void transferDiskInfoToDisk(Disk* disk);
    uint64_t seqNum() const {
        return seqNum_;
    }
    void seqNum(uint64_t seqNum) {
        seqNum_ = seqNum;
    }
    void clear();
    void updateFileValueInfo(FileInfo* finfo) {
        MutexLock l(&mu_);
        ReplayValueFileInfo* replayFileInfo = new ReplayValueFileInfo(finfo);
        replayObjs_.push_back(replayFileInfo);
    }

private:
    int decode(Slice& src);
    bool shouldBeGood();

private:
    int number_;
    bool bGood_;
    uint64_t seqNum_;
    uint64_t destAddr_;
    Disk* disk_;
    vector<ReplayObj*> replayObjs_;
    port::Mutex mu_;
    // Disk info
    uint32_t numZones_;
    uint64_t  zoneStartAddr_;
    std::string *current_;
    FileMap* fileMap_;
    FileMap* valueFileMap_;
    unsigned char *zoneUseMap_;
    int zoneUseMapSize_;
    FileInfo* manifestInfo_;
    Level* levels_[10];
    int lastAllocatedZoneMapIdx_;
    int lastAllocatedZone_;
    int lastAllocatedConvZoneMapIdx_;
    int lastAllocatedConvZone_;
    uint32_t currentLogZone_;
    int nBads_;
};

} /* namespace smr */

#endif /* SUPERBLOCK_H_ */
