/*
 * ManifestSequentialFile.h
 *
 *  Created on: Nov 3, 2015
 *      Author: tri
 */

#ifndef MANIFESTSEQUENTIALFILE_H_
#define MANIFESTSEQUENTIALFILE_H_

#include "SmrSequentialFile.h"

namespace smr {

class ManifestSequentialFile: public SmrSequentialFile {
public:
    ManifestSequentialFile(const std::string& fname, int fd,
            Disk* const& disk, size_t page_size, FileInfo* finfo) :
                SmrSequentialFile(fname, fd, disk, page_size, finfo) {
        currentIt_ = finfo_->getSegments()->begin();
        currentSegmentOff_ = 0;
        contiguousSize_ = 0;
    }
    virtual ~ManifestSequentialFile() {
    }
    virtual Status Read(size_t n, Slice* result, char* scratch);
    virtual Status Skip(uint64_t n);

protected:
    bool MapNewRegion();

    uint64_t GetMaxContiguousSize();
    int ExtractData(Segment* segment, size_t n, char* retBuf) {
        assert(n + currentSegmentOff_ <= segment->getSize());
        memcpy(retBuf, dst_, n);
        currentSegmentOff_ += n;
        return n;
    }
private:
    int SkipInCurrent(int n) {
        Segment* segment = *currentIt_;
        if (n >= segment->getSize() - this->currentSegmentOff_) {
            n = segment->getSize() - this->currentSegmentOff_;
            this->contiguousSize_ -= segment->getSizeIn4KAlignedBytes();
            ++currentIt_;
            dst_ += segment->getSizeIn4KAlignedBytes() - currentSegmentOff_;
            currentSegmentOff_ = 0;
        } else {
            dst_ += n;
            currentSegmentOff_ += n;
        }
        return n;
    }

private:
    vector<Segment*>::iterator currentIt_;
    int currentSegmentOff_;
    uint64_t contiguousSize_;
};

} /* namespace smr */

#endif /* MANIFESTSEQUENTIALFILE_H_ */
