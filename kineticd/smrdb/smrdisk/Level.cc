/*
 * Level.cc
 *
 *  Created on: May 7, 2015
 *      Author: tri
 */

#include "Level.h"
#include "Zone.h"
#include "Disk.h"

//namespace leveldb {
namespace smr {

bool Level::deallocateFile(FileInfo* finfo) {
    assert(finfo);
    MutexLock l(&mu_);
    vector<Segment*>* segments = finfo->getSegments();
    vector<Segment*>::reverse_iterator it;
    for (it = segments->rbegin(); it != segments->rend(); ++it) {
        Zone* zone = (*it)->getZone();
        Segment* lastSegment = zone->getLastSegment();
        zone->deallocateSegment(*it);
        if (zone->isDeallocatable()) {
            deallocateZone(zone);
            delete zone;
        } else if (lastSegment == *it) {
           writableZones_.erase(zone->getNumber());
        }
    }
    return true;
}

void Level::deallocateZone(Zone* zone) {
    assert(zone->isDeallocatable());
    removeZone(zone);
    disk_->freeZone(zone->getNumber());
}
void Level::deallocateSegment(Segment* segment) {
    int zoneNum = segment->getAddr()/Disk::ZONE_SIZE;
    unordered_map<int, Zone*>::iterator it = this->zoneMap_.find(zoneNum);
    if (it != zoneMap_.end()) {
        Zone* zone = it->second;
        Segment* lastSegment = zone->getLastSegment();
        zone->deallocateSegment(segment);
        if (zone->isDeallocatable()) {
            this->deallocateZone(zone);
            delete zone;
        } else if (lastSegment == segment) {
            this->setZoneAsNotWritable(zone);
        }
    } else {
        stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ":???? ERROR: Cannot find zone # " << zoneNum << " in zone map";
        Status::InvalidArgument(ss.str());
    }
}
Segment* Level::allocateSegment(FileType type) {
    MutexLock l(&mu_);
    Segment* segment = NULL;
    Zone* zone = NULL;
    unordered_map<int, Zone*>::iterator it;

    // Always attempt to allocate a new zone for value files. Fall back to standard behavior
    // in case of failure.
    if(type == kValueFile) {
        zone = disk_->allocateZone(type);
        if(zone) {
            segment = zone->allocateSegment();
            zone->setLevel(this);
            zoneMap_[zone->getNumber()] = zone;
        }
	if(segment ) {
            inWritingZones_[zone->getNumber()] = zone;
	}
	
 	return segment;
    }

    for (it = writableZones_.begin(); segment == NULL && it != writableZones_.end();
            it = writableZones_.begin()) {
        zone = it->second;
        segment = zone->allocateSegment();
        writableZones_.erase(it->first);
    }

    if (segment == NULL) {
        zone = disk_->allocateZone(type);
        assert(zone);
        if (zone == NULL) {
            stringstream ss;
            ss << __FILE__ << ":" << __LINE__ << ":" << __func__ << ":Failed to allocate zone OR reset WP";
            Status::NoSpaceAvailable(ss.str());
            return NULL;
        }
        segment = zone->allocateSegment();
        assert(segment);
        zone->setLevel(this);
        zoneMap_[zone->getNumber()] = zone;
    }
    inWritingZones_[zone->getNumber()] = zone;
    return segment;
}

FileInfo* Level::allocateFile(uint64_t number, FileType type) {
    Segment* segment = allocateSegment(type);
    if (segment == NULL) {
        #ifdef KDEBUG
        cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << ": segment == NULL" << endl;
        #endif // KDEBUG
        return NULL;
    }
    assert(segment);
    FileInfo* finfo = new FileInfo(number, type, segment);
    finfo->setLevel(this);
    return finfo;
}

void Level::getZoneHistogram(vector<vector<uint16_t>>& histogram) {
    histogram.assign(6, {0, 0, 0});

    // iterate through zone map
    for (auto zone_itr = zoneMap_.begin(); zone_itr != zoneMap_.end(); ++zone_itr) {
        if (!zone_itr->second->isFull()) {
            continue;
        }

        unsigned int n_ssts = 0;
        unsigned int n_partial_ssts = 0;
        map<uint64_t, Segment*> segment_map = zone_itr->second->getSegmentMap();

        // iterate through the segments in the zone
        for (auto seg_itr = segment_map.begin(); seg_itr != segment_map.end(); ++seg_itr) {
            FileInfo* finfo = seg_itr->second->getFileInfo();
            unsigned int n_zones = finfo->getNumZones();

            // check how many zones the file is split across
            if (n_zones == 0) {
                // shouldn't meet this criteria since it would mean the file does not exist
            } else if (n_zones == 1) {
                // file is only in one zone
                ++n_ssts;
            } else {
                // file is split across multiple zones
                ++n_partial_ssts;
            }
        }

        // the last index in the vector is for 5+ ssts so truncate n_ssts if it exceeds 5
        if (n_ssts > 5) {
            n_ssts = 5;
        }

        // increment count
        ++(histogram[n_ssts][n_partial_ssts]);
    }
}
void Level::getZoneCounts(int& nDesignate, int& nNonDesignate, int& nMismatch) {
    nDesignate = 0;
    nNonDesignate = 0;
    nMismatch = 0;

    switch (this->number_) {
    case 0:
        for (unordered_map<int, Zone*>::iterator it = zoneMap_.begin();
             it != zoneMap_.end(); ++it) {
            if (it->first != it->second->getNumber()) {
                ++nMismatch;
            }
            if (it->first > 65) {
                ++nDesignate;
            } else {
                ++nNonDesignate;
            }
        }
        break;
    case 7:
        for (unordered_map<int, Zone*>::iterator it = zoneMap_.begin();
             it != zoneMap_.end(); ++it) {
            if (it->first != it->second->getNumber()) {
                ++nMismatch;
            }
            if (it->first > 3 && it->first < 62) {
                ++nDesignate;
            } else {
                ++nNonDesignate;
            }
        }
        break;
    case 8:
        for (unordered_map<int, Zone*>::iterator it = zoneMap_.begin();
             it != zoneMap_.end(); ++it) {
            if (it->first != it->second->getNumber()) {
                ++nMismatch;
            }
            if (it->first == 64 || it->first == 65) {
                ++nDesignate;
            } else {
                ++nNonDesignate;
            }
        }
        break;
    default:
        break;
    }
}
Zone* Level::addZone(int zoneNum) {
    MutexLock l(&mu_);
    if (zoneNum >= disk_->getNumZones()) {
        return NULL;
    }
    Zone* zone = NULL;
    unordered_map<int, Zone*>::iterator it = zoneMap_.find(zoneNum);
    if (it == zoneMap_.end()) {
        zone = new Zone(zoneNum);
        zone->setLevel(this);
        zoneMap_[zoneNum] = zone;
    } else {
        zone = it->second;
    }
    return zone;
}

}  // namespace smr
//}  // namespace leveldb(


