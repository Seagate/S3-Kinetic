/*
 * Zone.h
 *
 *  Created on: Apr 24, 2015
 *      Author: tri
 */

#ifndef ZONE_H_
#define ZONE_H_


#include "FileMap.h"

#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>

#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "Util.h"

using namespace std;
using namespace leveldb;
using namespace leveldb::port;

//namespace leveldb {
namespace smr {

class Level;

class Zone {
    public:
        static const uint32_t ZONE_SIZE = 256*1024*1024;  // 256 MB

    public:
        friend ostream& operator<<(ostream& out, Zone& zone) {
            out << "(Z#" << zone.number_ << ", #s " << zone.segMap_.size()
                << ", u " << zone.usage_ << ", full "
                << 1.0 * zone.usage_/Zone::ZONE_SIZE << ")";

            map<uint64_t, Segment*>::iterator it;
            for (it = zone.segMap_.begin(); it != zone.segMap_.end(); ++it) {
                out << *(it->second);
            }
            return out;
        }
        Zone(): number_(-1), usage_(0), level_(NULL) {
        }

        Zone(int number): number_(number), usage_(0), level_(NULL) {
        }

        virtual ~Zone() {
            segMap_.clear();
        }

        bool isFull() {
            return (usage_ == Zone::ZONE_SIZE);
        }

        bool isFragmented() {
            if (segMap_.size() == 0) {
                return true;
            }
            return (usage_ < Zone::ZONE_SIZE);
        }
        bool isDeallocatable() {
            return segMap_.size() == 0;
        }

        int getNumber() const {
            return number_;
        }

        void setLevel(Level* level) {
            // TRI -- ERROR    MutexLock l(mu_);
            level_ = level;
        }

        uint64_t getUsage() {
            return usage_;
        }
        void addUsage(off_t n) {
            assert(n % 4096 == 0);
            MutexLock l(&mu_);
            usage_ += n;
            assert(usage_ <= Zone::ZONE_SIZE);
        }
        void reduceUsage(off_t n) {
            MutexLock l(&mu_);
            usage_ -= n;
        }

        void setAsWritable();
        void setAsNotWritable(); 

        bool isWritable();
        Segment* allocateSegment() {
            MutexLock l(&mu_);
            uint64_t nextAddr = getNewSegmentAddr();
            if (nextAddr == 0 || segMap_.find(nextAddr) != segMap_.end()) {
                return NULL;
            }
            Segment* segment = new Segment(nextAddr, 0, NULL, this);
            segMap_[nextAddr] = segment;
            return segment;

        }
        void deallocateSegment(Segment* segment) {
            segMap_.erase(segment->getAddr());
            usage_ -= ROUNDUP(segment->getSize(), 4096);
            segment->setZone(NULL);
        }
        void addSegment(Segment* segment) {
            MutexLock l(&mu_);
            segMap_[segment->getAddr()] = segment;
            usage_ += ROUNDUP(segment->getSize(), 4096);
            segment->setZone(this);
        }

        Segment* getLastSegment() {
            Segment* lastSegment = NULL;
            map<uint64_t, Segment*>::reverse_iterator it = segMap_.rbegin();
            if (it != segMap_.rend()) {
                lastSegment = it->second;
            }
            return lastSegment;
        }
        map<uint64_t, Segment*>& getSegmentMap() {
            return segMap_;
        }
        uint64_t getAddr() {
        	return uint64_t(number_)*ZONE_SIZE;
        }
        bool operator()(const Zone* lhs, const Zone* rhs) const {
            return (lhs->usage_ <= rhs->usage_);
        }
        Level* getLevel() const {
            return level_;
        }
        bool isInWriting();

    private:
        uint64_t getNewSegmentAddr() {
            uint64_t nextAddr = 0;
            if (segMap_.size() == 0) {
                nextAddr = getAddr();
                return nextAddr;
            }
            map<uint64_t, Segment*>::reverse_iterator it = segMap_.rbegin();
            Segment* lastSegment = it->second;
            nextAddr = lastSegment->getAddr() + lastSegment->getSize();
	        nextAddr = ROUNDUP(nextAddr, 4096);

            if (nextAddr >= getAddr() + ZONE_SIZE) {
                nextAddr = 0;
            }
            return nextAddr;
        }
    private:
        int number_;
        uint64_t usage_;
        Level* level_;
        port::Mutex mu_;
        map<uint64_t, Segment*> segMap_;
};

} /* namespace smr */
//} /* namespace leveldb */

#endif /* ZONE_H_ */
