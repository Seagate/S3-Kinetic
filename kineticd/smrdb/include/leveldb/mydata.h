#ifndef STORAGE_LEVELDB_DB_MYDATA_H_
#define STORAGE_LEVELDB_DB_MYDATA_H_

#include <stdint.h>
#include <string.h>
#include <string>
#include <deque>
#include <mem/DynamicMemory.h>

using namespace std;

struct ExternalValueInfo {
  uint64_t file_number; // The data file number
  uint32_t offset;      // The offset within the file
  uint32_t size;        // The size of the value stored in the file

  // Constructor, initializing everything to zero
  ExternalValueInfo() : file_number(0), offset(0), size(0) {}
  // Return the serialized size.
  uint32_t computeSerializedSize() const;
  // Serialize ExternalValueInfo, returns pointer just past the serialized value
  char* serialize(char* buffer) const;
  // Deserialize ExternalValueInfo, return indicates success
  bool deserialize(const char* buffer);
};

enum class LevelDBDataType : uint8_t {
  MEM_INTERNAL,         // deserialized including value
  MEM_EXTERNAL,         // deserialized but value has not been read in
  SERIALIZED_INTERNAL,  // not deserialized, value stored internal
  SERIALIZED_EXTERNAL,  // not deserialized, value stored external
  INVALID
};

struct LevelDBData {
  LevelDBDataType type; // The serialization / deserialization state and location of value
  uint32_t headerSize;  // The size of the header buffer
  uint32_t dataSize;    // The size of the data buffer
  char* header;         // Contains version, tag, algorithm (use protobuf to deserialize)
  char* data;           // Contains real value or serialized ExternalValueInfo depending on type
  MEMORYType memType; // allocated memory type.

  // Constructor, initializing everything to zero
  LevelDBData() : type(LevelDBDataType::INVALID), headerSize(0), dataSize(0), header(NULL), data(NULL), memType(MEMORYType::INVALID) {}

  // If external value pointer is provided, it will be used for size computation instead of the data pointer
  // If skip_data flag is set, returned size disregards the data buffer.
  // If skip_header flag is set, returned size disregards the header buffer.
  uint32_t computeSerializedSize(
      const ExternalValueInfo* ext = NULL, bool skip_data = false, bool skip_header = false
  ) const;
  // If external value pointer is provided, external value will get serialized instead of the data buffer
  // If skip_data flag is set, data buffer will not be serialized.
  // If skip_header flag is set, header buffer will not be serialized.
  // Returns pointer just past the serialized structure.
  char* serialize(
      char* buffer, const ExternalValueInfo* ext = NULL, bool skip_data = false, bool skip_header = false
  ) const;
  // No additional memory is allocated, header and data pointers point inside provided buffer.
  bool deserialize(const char* buffer);
};

struct kvData
{
  // for levelDB value type or delete type
  char kType;
  uint32_t keySize;
  char* key;
  struct LevelDBData* value;

  explicit kvData()
  {
    key = NULL;
    value = NULL;
  }
};

struct BatchRecord
{
  uint64_t sequence; // fixed64
  uint32_t count;    // fixed32
  std::deque<struct kvData*> kvrecord;

  explicit BatchRecord()
  {}
};

// Convenience functions and operators
bool isSerialized(const LevelDBDataType& type);
bool operator==(const ExternalValueInfo& lhs, const ExternalValueInfo& rhs);
bool operator!=(const ExternalValueInfo& lhs, const ExternalValueInfo& rhs);
std::ostream& operator<<(std::ostream& os, const ExternalValueInfo& external);
std::ostream& operator<<(std::ostream& os, const LevelDBDataType& type);
std::ostream& operator<<(std::ostream& os, const LevelDBData& data);

#endif

