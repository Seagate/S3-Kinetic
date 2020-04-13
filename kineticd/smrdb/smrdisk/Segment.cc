/*
 * Segment.cc
 *
 *  Created on: May 11, 2015
 *      Author: tri
 */

#include "Segment.h"
#include <iostream>

#include "FileInfo.h"
#include "Zone.h"
#include "Util.h"
#include "Level.h"
#include "DriveEnv.h"
#include "ReplaySegment.h"

using namespace std;

namespace smr {

ostream& operator<<(ostream& out, Segment& src) {
    out << "[addr " << src.addr_ << ", f# ";
    if (src.finfo_) {
        out << src.finfo_->getNumber();
    } else {
        out << "NULL";
    }
    out << ", z# ";
    if (src.zone_) {
        out << src.zone_->getNumber();
    } else {
        out << "NULL";
    }
    out << ", size " << src.size_ << "]";
    return out;
}

int Segment::getZoneNumber() {
    if (zone_) {
        return zone_->getNumber();
    }
    return TRUNCATE(addr_, Zone::ZONE_SIZE)/Zone::ZONE_SIZE;
}
bool Segment::deserialize(Slice& src, Level* level ) {
    if (!GetVarint64(&src, reinterpret_cast<uint64_t *>(&addr_))) {
        return false;
    }
    if (!GetVarint64(&src, reinterpret_cast<uint64_t *>(&size_))) {
        return false;
    }
    if (level) {
        int zoneNum = getZoneNumber();
        zone_ = level->addZone(zoneNum);
        if (zone_) {
            zone_->addSegment(this);
        } else {
            return false;
        }
    }
    return true;
}
void Segment::addSize(off_t n) {
    int curUsage = ROUNDUP(size_, 4096);
    size_ += n;
    int newUsage = ROUNDUP(size_, 4096);
    zone_->addUsage(newUsage - curUsage);
}
void Segment::reduceSize(off_t n) {
    int curUsage = ROUNDUP(size_, 4096);
    size_ -= n;
    int newUsage = ROUNDUP(size_, 4096);
    zone_->reduceUsage(curUsage - newUsage);
}
uint32_t Segment::getSpaceLeft() {
    uint64_t zoneBoundAddr = TRUNCATE(addr_ + Zone::ZONE_SIZE, Zone::ZONE_SIZE);
    return zoneBoundAddr - ROUNDUP(addr_ + size_, 4096);
}
void Segment::complete(int idx) {
    assert(zone_);
    zone_->setAsWritable();
    log(idx);
}
void Segment::update(int idx) {
    assert(zone_);
    zone_->setAsWritable();
    logUpdate(idx);
}

void Segment::log(int idx) {
    ReplaySegment* replaySegment = new ReplaySegment(ReplayObj::Tag::kNewSegment, this, idx);
    DriveEnv::getInstance()->segmentCompleted(replaySegment);
    replaySegment->unref();
}
void Segment::logUpdate(int idx) {
    ReplaySegment* replaySegment = new ReplaySegment(ReplayObj::Tag::kUpdateSegment, this, idx);
    DriveEnv::getInstance()->segmentUpdated(replaySegment);
    replaySegment->unref();
}

}



