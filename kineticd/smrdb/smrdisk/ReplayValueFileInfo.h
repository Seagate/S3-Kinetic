/*
 * ReplayValueFileInfo.h
 *
 *  Created on: May 5, 2017
 *      Author: tri
 */

#ifndef REPLAYVALUEFILEINFO_H_
#define REPLAYVALUEFILEINFO_H_

#include <iostream>

#include "FileInfo.h"
#include "ReplayFileInfo.h"

namespace smr {

class ReplayValueFileInfo : public ReplayObj {
public:
friend ostream& operator<<(ostream& out, ReplayValueFileInfo& src);

    ReplayValueFileInfo() {
        this->tag_ = ReplayObj::Tag::kValueFileInfo;
    }
    ReplayValueFileInfo(FileInfo* fInfo): ReplayObj(ReplayObj::Tag::kValueFileInfo, fInfo->getNumber()) {
        fileValueInfo_ = fInfo->values();
    }
    virtual ~ReplayValueFileInfo() {
    }
    char* serialize(char* dest) {
        dest = ReplayObj::serialize(dest);
        dest = fileValueInfo_.serialize(dest);
        return dest;
    }
    bool deserialize(Slice& src, Disk* disk) {
        if (!ReplayObj::deserialize(src, disk)) {
            return false;
        }

        fileValueInfo_.deserialize(src);
        return true;
    }
    FileValueInfo& fileValueInfo() {
        return fileValueInfo_;
    }
    uint64_t getFileNumber() {
        return this->id();
    }
private:
    FileValueInfo fileValueInfo_;
};

} /* namespace smr */

#endif /* REPLAYVALUEFILEINFO_H_ */
