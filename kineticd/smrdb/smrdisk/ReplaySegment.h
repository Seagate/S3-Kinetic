/*
 * ReplaySegment.h
 *
 *  Created on: Nov 28, 2016
 *      Author: tri
 */

#ifndef REPLAYSEGMENT_H_
#define REPLAYSEGMENT_H_

#include "smrdisk/Segment.h"
#include "leveldb/status.h"
#include "db/filename.h"
#include "ReplayObj.h"

namespace smr {

class FileInfo;

class ReplaySegment : public ReplayObj {
public:
    friend ostream& operator<<(ostream& out, ReplaySegment& src);
    ReplaySegment(ReplayObj::Tag tag) : ReplayObj(tag, -1) {
        idx_ = -1;
        fnumber_ = -1;
        ftype_ = FileType::kIllegalType;
        segment_ = NULL;
    }
    ReplaySegment(ReplayObj::Tag tag, Segment* seg, int idx);
    virtual ~ReplaySegment() {
        delete segment_;
    }
    int getIdx() {
        return idx_;
    }
    bool deserialize(Slice& src, Disk* disk);
    Status replayDeallocatedSegment(Slice& src, FileMap* fileMap);
    char* serialize(char* dest);
    uint64_t getFileNumber() {
        return fnumber_;
    }
    FileType getFileType() {
        return ftype_;
    }
    Segment* segment() {
        return segment_;
    }
    void segment(Segment* seg) {
        segment_ = seg;
    }
    uint64_t address() {
        return id();
    }
private:
    ReplaySegment(): idx_(-1), segment_(NULL) {
        fnumber_ = -1;
        ftype_ = FileType::kIllegalType;
    }

    int idx_;
    uint64_t fnumber_;
    FileType ftype_;
    Segment* segment_;
};

} /* namespace smr */

#endif /* REPLAYSEGMENT_H_ */
