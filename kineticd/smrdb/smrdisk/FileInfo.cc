/*
 * FileInfo.cc
 *
 *  Created on: Apr 29, 2015
 *      Author: tri
 */



#include "FileInfo.h"
#include <iostream>

#include "Zone.h"
#include "Level.h"
#include "Segment.h"
#include "Disk.h"
//#include "ValueFileCache.h"

using namespace std;

//namespace leveldb {
namespace smr {

ostream& operator<<(ostream& out, FileInfo& src) {
    out << "{# " << src.number_ << ", t " << src.type_ << ", adr " << src.getAddr() << ", sz " << src.getSize();
    out << ", #s " << src.segments_.size() << ", valFInfo: (" << src.value_info_ << ")";
    vector<Segment*>::iterator it;
    for (it = src.segments_.begin(); it != src.segments_.end(); ++it) {
        out << ", " << **it ;
    }
    out << "}";
    return out;
}

string FileInfo::getFileName() const {
    int n = 0;
    string fname;
    fname.resize(100);
    switch (type_) {
    case kTableFile:
        n = snprintf((char *)fname.c_str(),100, "%06llu.%s", (unsigned long long)number_, "sst");
        break;
    case kUSTableFile:
        n = snprintf((char *)fname.c_str(), 100, "%06llu.%s", (unsigned long long)number_, "ust");
        break;
    case kLogFile:
        n = snprintf((char *)fname.c_str(), 100, "%06llu.%s", (unsigned long long)number_, "log");
        break;
    case kDescriptorFile:
        n = snprintf((char *)fname.c_str(), 100, "MANIFEST-%06llu", (unsigned long long)number_);
        break;
    case kTempFile:
        n = snprintf((char *)fname.c_str(), 100, "%06llu.%s", (unsigned long long)number_, "dbtmp");
        break;
    case kSSTTempFile:
        n = snprintf((char *)fname.c_str(), 100, "%06llu.%s", (unsigned long long)number_, "ssttmp");
        break;
      case kValueFile:
        n = snprintf((char *)fname.c_str(), 100, "%06llu.%s", (unsigned long long)number_, "dat");
        break;
    default:
        break;
    }
    fname.resize(n);
    return fname;
}
Segment* FileInfo::replaceLastSegment(Segment* newSeg) {
    if (segments_.size() == 0) {
        return NULL;
    }
    Segment* lastSegment = segments_[segments_.size() - 1];
    segments_.erase(segments_.end() - 1);
    addSegment(newSeg);
    return lastSegment;
}

bool FileInfo::deserialize(Slice& src, Disk* disk, bool withSegment) {
    uint32_t nSegments = 0;
    segments_.clear();
    if (!GetVarint64(&src, &number_)) {
        return false;
    }
    if (!GetVarint32(&src, reinterpret_cast<uint32_t *>(&type_))) {
        return false;
    }
    uint32_t level = 0;
    if (!GetVarint32(&src, reinterpret_cast<uint32_t *>(&level))) {
        return false;
    }
    if (type_ == FileType::kValueFile) {
      this->value_info_.deserialize(src);
    }
    level_ = disk->getLevel(level);
    if (level_ == NULL) {
       return true;
    }
    if (withSegment) {
        if (!GetVarint32(&src, &nSegments)) {
            return false;
        }
        for (int i = 0; i < nSegments; ++i ){
            Segment* segment = new Segment();
            if (!segment->deserialize(src, level_)) {
                delete segment;
                return false;
            }
            addSegment(segment);
        }
    }
    return true;
}
void FileInfo::deserializeDeletedFile(Slice& src) {
    GetVarint64(&src, &number_);
    GetVarint32(&src, reinterpret_cast<uint32_t *>(&type_));
}


char* FileInfo::serialize(char* dest, bool withSegment) {
    char* ptr;
    ptr = dest;
    ptr = PutVarint64InBuff(ptr, number_);
    ptr = PutVarint32InBuff(ptr, type_);
    if (level_) {
        ptr = PutVarint32InBuff(ptr, level_->getNumber());
    } else {
        ptr = PutVarint32InBuff(ptr, -1);
    }
    if(type_ == FileType::kValueFile) {
        ptr = value_info_.serialize(ptr);
    }
    if (withSegment) {
        uint32_t n = segments_.size();
        ptr = PutVarint32InBuff(ptr, n);
        for (int i = 0; i < n; i++ ){
            ptr = segments_[i]->serialize(ptr);
        }
    }
    return ptr;
}

void FileInfo::serialize(string& dest, bool withSegment) {
    PutVarint64(&dest, number_);
    PutVarint32(&dest, type_);
    if (level_) {
        PutVarint32(&dest, level_->getNumber());
    } else {
        PutVarint32(&dest, -1);
    }
    if (type_ == FileType::kValueFile) {
        value_info_.serialize(dest);
    }
    if (withSegment) {
        uint32_t n = segments_.size();
        PutVarint32(&dest, n);
        for (int i = 0; i < n; i++ ){
            segments_[i]->serialize(dest);
        }
    }
}

char* FileInfo::serializeDeletedFile(char* src) {
    src = PutVarint64InBuff(src, number_);
    src = PutVarint32InBuff(src, type_);
    return src;
}

unsigned int FileInfo::getNumZones() {
    if (segments_.size() <= 0) {
        return 0;
    } else if (segments_.size() == 1) {
        return 1;
    } else if (segments_.size() == 2) {
        if (segments_[0]->getZone()->getNumber() == segments_[1]->getZone()->getNumber()) {
            return 1;
        } else {
            return 2;
        }
    } else {
        int zone;
        vector<int> zones;
        for (auto it = segments_.begin(); it != segments_.end(); ++it) {
            zone = (*it)->getZone()->getNumber();

            auto zone_itr = zones.begin();
            for (; zone_itr != zones.end(); ++zone_itr) {
                if (*zone_itr == zone) {
                    break;
                }
            }

            if (zone_itr == zones.end()) {
                zones.push_back(zone);
            }
        }

        return zones.size();
    }
}
Segment* FileInfo::removeSegment(Segment* seg) {
    Segment* removeSeg = NULL;
    vector<Segment*>::iterator it = segments_.begin();
    for (; it != segments_.end(); ++it) {
        if ((*it)->getAddr() == seg->getAddr()) {
            removeSeg = *it;
            segments_.erase(it);
            break;
        }
    }
    return removeSeg;
}
Status FileInfo::replaceSegment(Segment* oldSeg, vector<Segment*>& newSegs) {
            Status status;
            vector<Segment*>::iterator it = segments_.begin();
            while (it != segments_.end() && *it != oldSeg) {
                ++it;
            }
            if (it == segments_.end()) {
                stringstream ss;
                ss << "Source segment doesn't belong to the file:" << endl
                   << "Segment: " << *oldSeg << endl
                   << "File info: " << *this;
               status = Status::InvalidArgument("replaceSegment", ss.str().c_str());
               ss.clear();
            } else {
               // Insert after the replaced segment
               segments_.insert(++it, newSegs.begin(), newSegs.end());
               for (it = segments_.begin(); it != segments_.end(); ++it) {
                   if (*it == oldSeg) {
                       break;
                   }
               }
               segments_.erase(it);
            }
            return status;
}
bool FileInfo::isDefragtable(string& dbname) {
    if (this->value_info_.getDeleted() == 0) { // || CacheManager::cache(dbname)->isWritable(number_)) {
        return false;
    }
    Zone* lastSegZone = getLastSegment()->getZone();
    return !lastSegZone->isWritable();
}
void FileInfo::setAsNotWritable() {
    Zone* lastSegZone = getLastSegment()->getZone();
    lastSegZone->setAsNotWritable();
}
    

// define static mutex
std::mutex FileValueInfo::mutex_;

ostream& operator<<(ostream& out, const FileValueInfo& src) {
    out << "#total " << src.num_total << ", #del " << src.num_deleted;
    return out;
}
uint16_t FileValueInfo::getTotal() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return num_total;
}

uint16_t FileValueInfo::getDeleted() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return num_deleted;
}

void FileValueInfo::incrTotal(uint16_t size)
{
  std::lock_guard<std::mutex> lock(mutex_);
  num_total += size;
}

void FileValueInfo::incrDeleted(uint16_t size)
{
  std::lock_guard<std::mutex> lock(mutex_);
  num_deleted += size;
}
}
