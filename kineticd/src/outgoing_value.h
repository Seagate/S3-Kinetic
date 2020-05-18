#ifndef KINETIC_OUTGOING_VALUE_H_
#define KINETIC_OUTGOING_VALUE_H_

#include <string>

#include "gmock/gmock.h"

#include "kinetic/common.h"
#include "kinetic/outgoing_value.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::OutgoingValueInterface;

/*
 * OutgoingFileValue represents a file whose contents are intended to be
 * written to a socket. The TransferToSocket member function uses the sendfile
 * system call to do this efficiently. When it is not possible to write the raw
 * bytes directly to a socket (e.g. when TLS is in use), the ToString member
 * function can be used as an alternative, though it will not benefit from the
 * performance of sendfile.
 */
class OutgoingFileValue : public OutgoingValueInterface {
    public:
    explicit OutgoingFileValue(int fd);
    ~OutgoingFileValue();
    size_t size() const;
    bool TransferToSocket(int fd, int* err) const;
    bool ToString(std::string *result, int* err) const;
    int GetFD();
    private:
    // Call sendfile with 128-KiB blocks
    static const size_t kBlockSize = 128 * 1024;
    const int fd_;
    DISALLOW_COPY_AND_ASSIGN(OutgoingFileValue);
};


/*
 * NullableOutgoingValue is a small wrapper around an OutgoingValueInterface
 * pointer. It implements the null object pattern in the sense that if its
 * set_value member function is not called, the object acts as if its contents
 * are an empty string. The set_value function can be called to pass ownership
 * of a heap-allocated OutgoingValueInterface to the object. Thereafter, it
 * transparently proxies method calls to the wrapped object and deletes it when
 * its destructor is called. These things make it convenient to pass a
 * NullableOutgoingValue pointer as a result parameter to functions that may or
 * may not return an outgoing value.
 */
class NullableOutgoingValue : public OutgoingValueInterface {
    public:
    NullableOutgoingValue();
    ~NullableOutgoingValue();
    size_t size() const;
    bool TransferToSocket(int fd, int* err) const;
    bool ToString(std::string *result, int* err) const;
    // set_value should not be called more than once.
    void set_value(OutgoingValueInterface *value);
    void clear_value();
    private:
    OutgoingValueInterface *value_;
    DISALLOW_COPY_AND_ASSIGN(NullableOutgoingValue);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_OUTGOING_VALUE_H_
