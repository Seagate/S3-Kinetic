/*
 * ReplayObj.h
 *
 *  Created on: Dec 9, 2016
 *      Author: tri
 */

#ifndef REPLAYOBJ_H_
#define REPLAYOBJ_H_

#include <stdint.h>
#include <iostream>

#include "util/coding.h"
#include "leveldb/slice.h"

using namespace leveldb;

namespace smr {

class Disk;

class ReplayObj {
public:
    enum Tag {
        kUnknown            = 0,
        kMagicNumber        = 1,
        kStatus             = 2,
        kCurLogZone         = 3,
        kZoneStartAddr      = 4,
        kNumZones           = 5,
        kZoneSize           = 6,
        kCurrent            = 7,
        kZoneInventory      = 8,
        kFileMap            = 9,
        kNewFile            = 10,
        kDeletedFile        = 11,
        kNewSegment         = 12,
        kUpdateSegment      = 13,
        kDeallocatedSegment = 14,
        kSeqNum             = 15,
        kValueFileMap       = 16,
        kValueFileInfo      = 17
    };

public:
    friend ostream& operator<<(ostream& out, ReplayObj& src);

    ReplayObj(): tag_(Tag::kUnknown), id_(-1) {
        ref_ = 1;
    }
    ReplayObj(Tag tag, uint64_t id): tag_(tag), id_(id) {
        ref_ = 1;
    }
    virtual ~ReplayObj() {
    }

    Tag tag() {
        return tag_;
    }
    uint64_t id() {
        return id_;
    }
    virtual char* serialize(char* dest) {
        dest = PutVarint32InBuff(dest, tag_);
        dest = PutVarint64InBuff(dest, id_);
        return dest;
    }
    virtual bool deserialize(Slice& src, Disk* disk) {
        return GetVarint64(&src, &id_);
    }
    void tag(Tag tag) {
        tag_ = tag;
    }
    void id(uint64_t id) {
        id_ = id;
    }
    void ref() {
        ++ref_;
    }
    void unref() {
        --ref_;
        if (!ref_) {
            delete this;
        }
    }

protected:
    Tag tag_;
    uint64_t id_;
    int ref_;
};

} /* namespace smr */

#endif /* REPLAYOBJ_H_ */
