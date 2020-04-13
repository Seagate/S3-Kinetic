#ifndef SMRDB_VALUEFILESECTION_H
#define SMRDB_VALUEFILESECTION_H

#include <stdint.h>
#include <string.h>
#include "smrdisk/Util.h"

namespace smr {

// Section descriptor is appended at the end of 4KB boundary to an sst table
struct SectionDescriptor
{
  const uint64_t magic;       // magic number
  uint32_t sst_offset;        // file offset to section sst block
  uint32_t sst_size;          // size of section sst block
  uint32_t prev_end_offset;   // file offset to end of previous section

  /* constructor, sets section magic */
  SectionDescriptor() : magic(0x421d413b4533423bull), sst_offset(0), sst_size(0), prev_end_offset(0) {}

  /* append section descriptor page-aligned to end of section sst buffer
   * do not use varint encoding, section descriptor needs to have a fixed size */
  char* append(char* sst_buffer) const
  {
    char* buffer = sst_buffer
                   + ROUNDUP(sst_size + sizeof(SectionDescriptor), 4096)
                   - sizeof(SectionDescriptor);

    memcpy(buffer, &magic, sizeof(magic));
    buffer += sizeof(magic);
    memcpy(buffer, &sst_offset, sizeof(sst_offset));
    buffer += sizeof(sst_offset);
    memcpy(buffer, &sst_size, sizeof(sst_size));
    buffer += sizeof(sst_size);
    memcpy(buffer, &prev_end_offset, sizeof(prev_end_offset));
    buffer += sizeof(prev_end_offset);
    return buffer;
  }

  /* attempt to deserialize section descriptor from buffer */
  bool deserialize(const char* buffer)
  {
    if (memcmp(buffer, &magic, sizeof(magic)) != 0) {
      return false;
    }
    buffer += sizeof(magic);

    memcpy(&sst_offset, buffer, sizeof(sst_offset));
    buffer += sizeof(sst_offset);
    memcpy(&sst_size, buffer, sizeof(sst_size));
    buffer += sizeof(sst_size);
    memcpy(&prev_end_offset, buffer, sizeof(prev_end_offset));
    return true;
  }
};

}

#endif //SMRDB_VALUEFILESECTION_H
