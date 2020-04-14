/*
 * ManifestWritableFile.h
 *
 *  Created on: Nov 3, 2015
 *      Author: tri
 */

#ifndef MANIFESTWRITABLEFILE_H_
#define MANIFESTWRITABLEFILE_H_

#include "SmrWritableFile.h"
#include "common/Listener.h"
#include "common/Listenable.h"

using namespace com::seagate::common;

namespace smr {

class ManifestWritableFile: public SmrWritableFile {
public:
    ManifestWritableFile(const std::string& fname, int fd, Disk* const& disk,
            size_t page_size, FileInfo* finfo) : SmrWritableFile(fname, fd, disk, page_size, finfo) {
        lastSegAfterLastSync_ = finfo_->getLastSegment();
        if (lastSegAfterLastSync_) {
            lastSegAfterLastSyncSize_ = lastSegAfterLastSync_->getSize();
        } else {
            lastSegAfterLastSyncSize_ = 0;
        }
    }
    virtual ~ManifestWritableFile() {
    }
    void rollback();

protected:
    virtual void notify();
};

} /* namespace smr */

#endif /* MANIFESTWRITABLEFILE_H_ */
