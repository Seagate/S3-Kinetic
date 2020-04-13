#ifndef KINETIC_SMRDB_TEST_HELPERS_H_
#define KINETIC_SMRDB_TEST_HELPERS_H_

#include <string>
#include "leveldb/slice.h"
#include "leveldb/env.h"
#include "smrdisk/DriveEnv.h"
#include "util/random.h"

using leveldb::Slice;
using leveldb::EnvWrapper;
using leveldb::Random;
using smr::DriveEnv;

namespace com {
namespace seagate {
namespace kinetic {

char* PackValueMem(std::string value);

char* PackValueMem(std::string value, int* size);

// std::string PackValueString(std::string value);

char* PackValue(char* value);

// std::string RandomString(Random* rnd, int len, std::string* dst);

// int RandomSeed();

// std::string RandomKey(Random* rnd, int len);

std::string UnpackValue(const char* packed_value);

std::string ExtractValue(const char *packed_value, const std::string key_value_store_name);

void deallocate_getvalue_buffer(char* buff);

// // A wrapper that allows injection of errors.
// class ErrorEnv : public EnvWrapper {
//  public:
//     bool writable_file_error_;
//     int num_writable_file_errors_;

//     ErrorEnv() : EnvWrapper(DriveEnv::getInstance()), writable_file_error_(false), num_writable_file_errors_(0) {}

//     virtual Status NewWritableFile(const std::string& fname, WritableFile** result) {
//         if (writable_file_error_) {
//             ++num_writable_file_errors_;
//             *result = NULL;
//             return Status::IOError(fname, "fake error");
//         }
//         return target()->NewWritableFile(fname, result);
//     }
// };

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SMRDB_TEST_HELPERS_H_
