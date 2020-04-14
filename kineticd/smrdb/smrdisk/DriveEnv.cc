#include "DriveEnv.h"

#include "util/env_posix.h"
#include "SmrEnv.h"

using namespace leveldb;
using namespace leveldb::posix;

namespace smr {

ostream& operator<<(ostream& out, DriveEnv& env) {
    out << *((SmrEnv*)env.smrEnv_) << endl;
    return out;
}
DriveEnv* DriveEnv::instance_ = NULL;

DriveEnv* DriveEnv::getInstance() {
    if (NULL == instance_) {
        instance_ = new DriveEnv();
    }
    return instance_;
}
DriveEnv::DriveEnv() {
    smrEnv_ = new SmrEnv();
    posixEnv_ = new PosixEnv();
}

uint64_t DriveEnv::gettid() {
    return PosixEnv::gettid();
}

void* DriveEnv::BGThreadWrapper(void* arg) {
    return SmrEnv::BGThreadWrapper(arg);
}
}  // namespace smr
//}  // namespace leveldb
