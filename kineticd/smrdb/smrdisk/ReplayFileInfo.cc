/*
 * ReplayFileInfo.cc
 *
 *  Created on: Dec 12, 2016
 *      Author: tri
 */

#include "ReplayValueFileInfo.h"
#include "ReplayFileInfo.h"
#include "FileInfo.h"
#include "Level.h"

namespace smr {
ostream& operator<<(ostream& out, ReplayValueFileInfo& src) {
    out << "(tag " << src.tag_ << ", id " << src.id_ << ", " << src.fileValueInfo_ << ")";
    return out;
}

ostream& operator<<(ostream& out, ReplayFileInfo& src) {
    out << "{ftype " << src.type_ << ", level " << src.level_;
    if (src.finfo_) {
        out << ", " << *src.finfo_;
    }
    out << ", tag " << src.tag() << ", id " << src.id();
    out << "}";
    return out;
}

bool ReplayFileInfo::deserialize(Slice& src, Disk* disk) {
    bool success = false;
    finfo_ = new FileInfo();

    if (tag_ == ReplayObj::Tag::kNewFile) {
        if (finfo_->deserialize(src, disk, false)) {
           id(finfo_->getNumber());
           type_ = finfo_->getType();
           level_ = finfo_->getLevel()->getNumber();
           success = true;
        }
    } else {
        if (ReplayObj::deserialize(src, disk)) {
           if (GetVarint32(&src, reinterpret_cast<uint32_t *>(&type_))) {
               finfo_->setNumber(id());
               finfo_->setType(type_);
               success = true;
           }
        }
    }
    if (!success) {
        delete finfo_;
        finfo_ = NULL;
    }
    return success;
}

} /* namespace smr */
