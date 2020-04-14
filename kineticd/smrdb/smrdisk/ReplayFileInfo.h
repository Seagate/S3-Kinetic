/*
 * ReplayFileInfo.h
 *
 *  Created on: Dec 12, 2016
 *      Author: tri
 */

#ifndef REPLAYFILEINFO_H_
#define REPLAYFILEINFO_H_

#include "FileInfo.h"
#include "ReplayObj.h"

namespace smr {

class ReplayFileInfo: public ReplayObj {
public:
    friend ostream& operator<<(ostream& out, ReplayFileInfo& src);

    ReplayFileInfo() {
        type_ = FileType::kIllegalType;
        level_ = -1;
        finfo_ = NULL;
    }
    ReplayFileInfo(ReplayObj::Tag tag): ReplayObj(tag, -1) {
        type_ = FileType::kIllegalType;
        level_ = -1;
        finfo_ = NULL;
    }        

    virtual ~ReplayFileInfo() {
        if (tag() == ReplayObj::Tag::kDeletedFile) {
            delete finfo_;
        }
    }
    bool init(ReplayObj::Tag aTag, uint64_t fnumber, FileType ftype) {
        if (aTag != ReplayObj::Tag::kDeletedFile) {
            return false;
        }
        tag(aTag);
        id(fnumber);
        type_ = ftype;
        return true;
    }
    bool init(ReplayObj::Tag aTag, uint64_t fnumber, FileType ftype, int level) {
        if (aTag != ReplayObj::Tag::kNewFile) {
            return false;
        }
        tag(aTag);
        id(fnumber);
        type_ = ftype;
        level_ = level;
        return true;
    }
    char* serialize(char* dest) {
        dest = ReplayObj::serialize(dest);
        dest = PutVarint32InBuff(dest, type_);
        if (tag_ == ReplayObj::Tag::kNewFile) {
            dest = PutVarint32InBuff(dest, level_);
        }
        return dest;
    }
    bool deserialize(Slice& src, Disk* disk);
    FileInfo* fileInfo() {
        return finfo_;
    }

private:
    FileType type_;
    int level_;
    FileInfo* finfo_;
};

} /* namespace smr */

#endif /* REPLAYFILEINFO_H_ */
