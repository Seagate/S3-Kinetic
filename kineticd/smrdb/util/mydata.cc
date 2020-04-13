#include "leveldb/mydata.h"
#include "smrdisk/SmrEnv.h"
#include "smrdisk/ValueFileCache.h"
#include <iomanip>

bool isSerialized(const LevelDBDataType& type)
{
  return type == LevelDBDataType::SERIALIZED_INTERNAL || type == LevelDBDataType::SERIALIZED_EXTERNAL;
}

char* ExternalValueInfo::serialize(char* buffer) const
{
  buffer = PutVarint64InBuff(buffer, file_number);
  buffer = PutVarint32InBuff(buffer, offset);
  buffer = PutVarint32InBuff(buffer, size);
  return buffer;
}

bool ExternalValueInfo::deserialize(const char* buffer)
{
  const char* limit = buffer + sizeof(ExternalValueInfo);
  buffer = GetVarint64Ptr(buffer, limit, &file_number);
  if(!buffer) return false;
  buffer = GetVarint32Ptr(buffer, limit, &offset);
  if(!buffer) return false;
  buffer = GetVarint32Ptr(buffer, limit, &size);
  if(!buffer) return false;
  return true;
}

uint32_t ExternalValueInfo::computeSerializedSize() const
{
  return VarintLength(file_number) + VarintLength(offset) + VarintLength(size);
}

bool operator==(const ExternalValueInfo& lhs, const ExternalValueInfo& rhs)
{
  return lhs.file_number == rhs.file_number && lhs.offset == rhs.offset && lhs.size == rhs.size;
}

bool operator!=(const ExternalValueInfo& lhs, const ExternalValueInfo& rhs)
{
  return !(lhs == rhs);
}

uint32_t LevelDBData::computeSerializedSize(const ExternalValueInfo* ext, bool skip_data, bool skip_header) const
{
  switch (type) {
    case LevelDBDataType::MEM_INTERNAL:
      if (ext) {
        uint32_t ext_size = ext->computeSerializedSize();
        return sizeof(type) + (skip_header ? VarintLength(0) : VarintLength(headerSize)) + VarintLength(ext_size)
               + (skip_header ? 0 : headerSize) + (skip_data ? 0 : ext_size);
      }
    case LevelDBDataType::MEM_EXTERNAL:
      return sizeof(type) + (skip_header ? VarintLength(0) : VarintLength(headerSize)) + VarintLength(dataSize)
             + (skip_header ? 0 : headerSize) + (skip_data ? 0 : dataSize);
    default:
      Status::InvalidArgument("Cannot compute size for serialized LevelDBData.");
      return 0;
  }
}

char* LevelDBData::serialize(char* buffer, const ExternalValueInfo* ext, bool skip_data, bool skip_header) const
{
  LevelDBDataType serialized_type;
  switch (type) {
    case LevelDBDataType::MEM_INTERNAL:
      if (!ext) {
        serialized_type = LevelDBDataType::SERIALIZED_INTERNAL;
        break;
      }
    case LevelDBDataType::MEM_EXTERNAL:
      serialized_type = LevelDBDataType::SERIALIZED_EXTERNAL;
      break;
    default:
      Status::InvalidArgument("Cannot serialize non mem type.");
      return buffer;
  }
  /* Set serialized type. use memcpy so it remains accessible in serialized state */
  memcpy(buffer, &serialized_type, sizeof(serialized_type));
  buffer += sizeof(serialized_type);

  /* Use variable length encoding for header and data size values */
  if(!skip_header) {
    buffer = PutVarint32InBuff(buffer, headerSize);
  }
  else {
    buffer = PutVarint32InBuff(buffer, 0);
  }
  if(ext) {
    buffer = PutVarint32InBuff(buffer, ext->computeSerializedSize());
  }
  else {
    buffer = PutVarint32InBuff(buffer, dataSize);
  }

  /* copy header buffer */
  if(!skip_header) {
    memcpy(buffer, header, headerSize);
    buffer += headerSize;
  }

  /* copy data buffer or serialize external info structure */
  if(!skip_data) {
    if (ext) {
      buffer = ext->serialize(buffer);
    } else if(dataSize) {
      memcpy(buffer, data, dataSize);
      buffer += dataSize;
    }
  }
  /*
  if (ext) {
      cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow , type = " << type << ", serialized_type = " << serialized_type << ", extInfo = " << *ext << endl;
  } else {
      //cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Watch Flow , type = " << type << ", serialized_type = " << serialized_type << ", extInfo = " << 0 << endl;
  }
*/
  return buffer;
}

bool LevelDBData::deserialize(const char* buffer)
{
  const char* limit = buffer + sizeof(LevelDBData);

  memcpy(&type, buffer, sizeof(type));
  buffer += sizeof(type);
  switch (type) {
    case LevelDBDataType::SERIALIZED_INTERNAL :
      type = LevelDBDataType::MEM_INTERNAL;
      break;
    case LevelDBDataType::SERIALIZED_EXTERNAL :
      type = LevelDBDataType::MEM_EXTERNAL;
      break;
    default:
      Status::InvalidArgument("Called with non serialized type.");
      return false;
  }

  buffer = GetVarint32Ptr(buffer, limit, &headerSize);
  if(!buffer) return false;
  buffer = GetVarint32Ptr(buffer, limit, &dataSize);
  if(!buffer) return false;

  header = (char*) buffer;
  buffer += headerSize;
  data = (char*) buffer;
  
  return true;
}

std::ostream& operator<<(std::ostream& os, const LevelDBDataType& type)
{
  switch(type) {
    case LevelDBDataType::MEM_EXTERNAL:
      os << "MEM_EXTERNAL";
      break;
    case LevelDBDataType::MEM_INTERNAL:
      os << "MEM_INTERNAL";
      break;
    case LevelDBDataType::SERIALIZED_EXTERNAL:
      os << "SERIALIZED_EXTERNAL";
      break;
    case LevelDBDataType::SERIALIZED_INTERNAL:
      os << "SERIALIZED_INTERNAL";
      break;
    case LevelDBDataType::INVALID:
      os << "INVALID";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const ExternalValueInfo& ext)
{
  os << "ExternalValueInfo: file_number=" << ext.file_number << " offset=" << ext.offset << " size=" << ext.size << endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const LevelDBData& data)
{
  os << "LevelDBData: type=" << data.type << " headerSize=" << data.headerSize << " dataSize=" << data.dataSize << endl;
  if(data.type == LevelDBDataType::MEM_EXTERNAL) {
    ExternalValueInfo external;
    memcpy(&external, data.data, data.dataSize);
    os << external;
  }
  return os;
}
