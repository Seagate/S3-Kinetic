/*
 * ReplaySegment.cc
 *
 *  Created on: Nov 28, 2016
 *      Author: tri
 */

#include "ReplaySegment.h"
#include "db/filename.h"
#include "FileInfo.h"
#include "FileMap.h"
#include "Disk.h"

namespace smr {

ostream& operator<<(ostream& out, ReplaySegment& src) {
    out << "{idx " << src.idx_ << ", fnumber  " << src.fnumber_ << ", ftype " << src.ftype_;
    if (src.segment_) {
        out << ", " << *src.segment_;
    }
    out << ", tag " << src.tag() << ", id " << src.id();
    out << "}";
    return out;
}
ReplaySegment::ReplaySegment(ReplayObj::Tag tag, Segment* seg, int idx): ReplayObj(tag, seg->getAddr()) {
    idx_ = idx;
    fnumber_ = seg->getFileInfo()->getNumber();
    ftype_ = seg->getFileInfo()->getType();
    segment_ = new Segment(*seg);
}
bool ReplaySegment::deserialize(Slice& src, Disk* disk) {
    bool success = false;
    if (ReplayObj::deserialize(src, disk)) {
       if (GetVarint64(&src, &fnumber_) && GetVarint32(&src, reinterpret_cast<uint32_t *>(&ftype_)) &&
          GetVarint32(&src, reinterpret_cast<uint32_t *>(&idx_))) {
          if (!segment_) {
             segment_ = new Segment();
          }
          Level* level = NULL;
          success = segment_->deserialize(src, level);
       }
    }
    return success;
}
Status ReplaySegment::replayDeallocatedSegment(Slice& src, FileMap* fileMap) {
    uint64_t fnumber = -1;
    FileType ftype;
    int idx = -1;
    GetVarint64(&src, &fnumber);
    GetVarint32(&src, reinterpret_cast<uint32_t *>(&ftype));
    GetVarint32(&src, reinterpret_cast<uint32_t *>(&idx));
    FileInfo* finfo = fileMap->getFileInfo(fnumber, ftype);
    if (finfo == NULL) {
        return Status::NotFound("File not found for segment");
    }
    Segment* seg = finfo->remove(idx);
    delete seg;
    return Status::OK();
}
char* ReplaySegment::serialize(char* dest) {
    dest = ReplayObj::serialize(dest);
    dest = PutVarint64InBuff(dest, fnumber_);
    dest = PutVarint32InBuff(dest, ftype_);
    dest = PutVarint32InBuff(dest, idx_);
    dest = this->segment_->serialize(dest);
    return dest;
}
} /* namespace smr */
