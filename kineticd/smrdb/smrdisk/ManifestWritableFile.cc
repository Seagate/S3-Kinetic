/*
 * ManifestWritableFile.cpp
 *
 *  Created on: Nov 3, 2015
 *      Author: tri
 */

#include "ManifestWritableFile.h"

namespace smr {

/*
 * To disable background manifest compaction, comment out the body of this notify().
 * To avoid cost of virtual function call, we also comment out notify() calls from SmrWritableFile.
 */
void ManifestWritableFile::notify()
{
// DISABLE MANIFEST BACKGROUND COMPACTION: To disable background manifest compaction,
// comment out body of this notify() method
   vector<Listener*>::iterator it;
   for (it = listeners_.begin(); it != listeners_.end(); ++it) {
      (*it)->notifyNewManifestSegments();
   }

}
void ManifestWritableFile::rollback() {
    for (int i = 0; i < nNewSegmentsAfterLastSync_; ++i) {
        Segment* seg = finfo_->getLastSegment();
        if (seg == lastSegAfterLastSync_) {
            seg->reduceSize(seg->getSize() - lastSegAfterLastSyncSize_);
        } else {
            finfo_->removeSegment(seg);
            Level* level = seg->getZone()->getLevel();
            level->deallocateSegment(seg);
        }
    }
    nNewSegments_ = 0;
    nNewSegmentsAfterLastSync_ = 0;
    lastSegAfterLastSync_ = finfo_->getLastSegment();
    lastSegAfterLastSyncSize_ = finfo_->getSize();
}
} /* namespace smr */
