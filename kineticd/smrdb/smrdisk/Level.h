/*
 * Level.h
 *
 *  Created on: Apr 24, 2015
 *      Author: tri
 */

#ifndef LEVEL_H_
#define LEVEL_H_
#include "FileMap.h"

#include <iostream>
#include <unordered_map>

#include "util/mutexlock.h"
#include "port/port_posix.h"
#include "Util.h"
#include "Zone.h"

using namespace std;
using namespace leveldb;
using namespace leveldb::port;

//namespace leveldb {
namespace smr {

class Disk;

class Level {
    static const uint32_t NUM_LEVEL = 10;

    public:
        friend ostream& operator<<(ostream& out, Level& level) {
            out << "--- LEVEL #" << level.number_ << ", #Zones = " << level.zoneMap_.size()
                                    << ", #AllocZones = " << level.writableZones_.size() << endl;
            unordered_map<int, Zone*>::iterator it;
            for (it = level.zoneMap_.begin(); it != level.zoneMap_.end(); ++it) {
                out << "** ZONE: " << *(it->second) << endl;
            }
            out << "#in writing zones = " << level.inWritingZones_.size() << ", #writable zones = " << level.writableZones_.size() << endl;

            return out;
        }

        Level(uint32_t number, Disk* disk): number_(number), disk_(disk) {
        }

        virtual ~Level() {
            unordered_map<int, Zone*>::iterator it;
            for (it = zoneMap_.begin(); it != zoneMap_.end(); ++it) {
                delete it->second;
            }
        }
        Segment* allocateSegment(FileType type);
        void deallocateSegment(Segment* segment);
        FileInfo* allocateFile(uint64_t number, FileType type);
        void deallocateZone(Zone* zone);
        bool deallocateFile(FileInfo* finfo);
        void getFragmentedZones(vector<Zone*>& zones) {
            unordered_map<int, Zone*>::iterator it;
            for (it = zoneMap_.begin(); it != zoneMap_.end(); ++it) {
                unordered_map<int, Zone*>::iterator allocZonesIt = writableZones_.find(it->first);
                unordered_map<int, Zone*>::iterator inWritingZonesIt = inWritingZones_.find(it->first);
                if (allocZonesIt == writableZones_.end() &&
                    inWritingZonesIt == inWritingZones_.end() && it->second->isFragmented()) {
                    zones.push_back(it->second);
                    if (zones.size() >= 2) {
                        break;
                    }
                }
            }
        }
        Zone* addZone(int zoneNum);

        // Pick the zone with the most fragmentation
        Zone* pickZoneForDefragmentation() {
            MutexLock l(&mu_);
            Zone* zone = NULL;
            for (auto it = zoneMap_.begin(); it != zoneMap_.end(); it++) {
                if(writableZones_.count(it->first) || inWritingZones_.count(it->first)) {
                    continue;
                }
                if(!zone || it->second->getUsage() < zone->getUsage()) {
                    zone = it->second;
                }
            }
            return zone;
        };

        void addZone(Zone* zone) {
            unordered_map<int, Zone*>::iterator it = zoneMap_.find(zone->getNumber());
            if (it == zoneMap_.end()) {
                zone->setLevel(this);
                zoneMap_[zone->getNumber()] = zone;
            }
        }
        uint32_t getNumber() const {
            return number_;
        }
        void setZoneAsWritable(Zone* zone) {
            inWritingZones_.erase(zone->getNumber());
            writableZones_[zone->getNumber()] = zone;
        }
        void setZoneAsNotWritable(Zone* zone) {
            MutexLock l(&mu_);
            writableZones_.erase(zone->getNumber());
            inWritingZones_.erase(zone->getNumber());
        }
        bool isZoneInWriting(Zone* zone) {
           MutexLock l(&mu_);
           return inWritingZones_.count(zone->getNumber());
        }
        bool isWritable(Zone* zone) {
            MutexLock l(&mu_);
            return !(inWritingZones_.find(zone->getNumber()) == inWritingZones_.end() && writableZones_.find(zone->getNumber()) == writableZones_.end());
        }
        bool isFragmented() {
            unordered_map<int, Zone*>::iterator it;
            uint64_t usage = 0;
            int nZones = 0;
            for (it = zoneMap_.begin(); it != zoneMap_.end() && nZones < 2; ++it) {
                assert(it->second);
                unordered_map<int, Zone*>::iterator allocZonesIt = writableZones_.find(it->first);
                unordered_map<int, Zone*>::iterator inWritingZonesIt = inWritingZones_.find(it->first);
                if (allocZonesIt == writableZones_.end() && inWritingZonesIt == inWritingZones_.end() &&
                    it->second->isFragmented()) {
                    usage += it->second->getUsage();
                    ++nZones;
                }
            }
            return (nZones > 1);
        }
        int getNumZones() {
            return zoneMap_.size();
        }
        void getZoneCounts(int& nDesignated, int&  nNonDesignated, int& nMisMatch);

        void clear() {
        	zoneMap_.clear();
        	writableZones_.clear();
        	inWritingZones_.clear();
        }
        void getZoneHistogram(vector<vector<uint16_t>>& histogram);
        Disk* disk() {
            return disk_;
        }
    private:
        void removeZone(Zone* zone) {
            if (zone == NULL) {
                return;
            }
            zone->setLevel(NULL);
            writableZones_.erase(zone->getNumber());
            inWritingZones_.erase(zone->getNumber());
            zoneMap_.erase(zone->getNumber());
        }

    private:
        port::Mutex mu_;
        uint32_t number_;
        unordered_map<int, Zone*> zoneMap_; // map of zone number to Zone*
        Disk* disk_;
        unordered_map<int, Zone*> writableZones_;
        unordered_map<int, Zone*> inWritingZones_;
};

} /* namespace smr */
//} /* namespace leveldb */

#endif /* LEVEL_H_ */
