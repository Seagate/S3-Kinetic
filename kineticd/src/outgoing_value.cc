#include "outgoing_value.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "glog/logging.h"

#include "kinetic/reader_writer.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::ReaderWriter;
//////////////////////////////////////////
///"OutgoingFileValue"
///--------------------------------------
/// belongs to a polymorphic hierarchy
/// whose parent class is "OutgoingValueInterface"
/// "OutgoingValueInterface" is also the parent of:
///  -outgoing_string_value
///  -NullableOutgoingValue
OutgoingFileValue::OutgoingFileValue(int fd)
    : fd_(fd) {}

OutgoingFileValue::~OutgoingFileValue() {
    if (close(fd_) != 0) {
        PLOG(ERROR) << " Failed to close file descriptor " << fd_;
    }
}

size_t OutgoingFileValue::size() const {
    struct stat stat_buf;
    // fstat is guaranteed to succeed if we pass a valid file descriptor
    CHECK_EQ(0, fstat(fd_, &stat_buf));
    return stat_buf.st_size;
}

bool OutgoingFileValue::ToString(std::string *result, int* err) const {
    ReaderWriter reader_writer(fd_);
    size_t value_size = size();
    char *buf = new char[value_size];
    bool success = reader_writer.Read(buf, value_size, err);
    *result = std::string(buf, value_size);
    delete[] buf;
    // Reset the file descriptor so that subsequent calls start from the
    // beginning. This call cannot fail as long as we provide a valid file
    // descriptor.
    CHECK_EQ(0, lseek(fd_, 0, SEEK_SET));
    return success;
}
//////////////////////////////////////////
///Media Scan Function: Access File Descriptor
///Author: James DeVore
///---------------------
///A Media Scan op does not immediately
/// return after one atomic op like a GetPrev/Next or Get.
///We need access to the FD to close it every
/// iteration. Otherwise, multiple files would be
/// left open as the Scan looks at every key's value.
///If file's were left open,the /mnt/store partition
/// will not be able to unmount in Lock() events.
int OutgoingFileValue::GetFD() {
    return fd_;
}

//////////////////////////////////////////
///"NullableOutgoingValue"
///------------------------------------------
/// belongs to a polymorphic hierarchy
/// whose parent class is "OutgoingValueInterface"
/// "OutgoingValueInterface" is also the parent of:
///  -outgoing_string_value
///  -outgoing_file_value
///  @value_ is of type "OutgoingValueInterface*"
NullableOutgoingValue::NullableOutgoingValue() : value_(NULL) {}

NullableOutgoingValue::~NullableOutgoingValue() {
    if (value_ != NULL) {
        delete value_;
    }
}

size_t NullableOutgoingValue::size() const {
    if (value_ == NULL) {
        return 0;
    }
    return value_->size();
}

bool NullableOutgoingValue::TransferToSocket(int fd, int* err) const {
    if (value_ == NULL) {
        return true;
    }
    return value_->TransferToSocket(fd, err);
}

bool NullableOutgoingValue::ToString(std::string *result, int* err) const {
    if (value_ == NULL) {
        *result = "";
        return true;
    }
    return value_->ToString(result, err);
}

void NullableOutgoingValue::set_value(OutgoingValueInterface *value) {
    // To simplify things, we require that the wrapped value can only be set
    // once and must not be set to NULL.
    CHECK(value_ == NULL);
    CHECK_NOTNULL(value);
    value_ = value;
}

//////////////////////////////////////////
///Media Scan Function: Clear "value_" pointer
///for re-use.
///---------------------
///Reason: The rigid NullObject Pattern employed
/// for this class does not allow for re-assignment
/// of value_ unless it is NULL.
/// Author: James DeVore
///---------------------
///A Media Scan op does not immediately
/// return after one atomic op like a GetPrev/Next or Get.
/// We need to re-use the NullableOutgoingValue every iteration.
///---------------------
///A "NullableOutgoingValue" belongs to a polymorphic hierarchy
/// The Delete call to @value_ will be dynamically bound to the
/// correct class for destruction.

void NullableOutgoingValue::clear_value() {
    delete value_;
    value_ = NULL;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
