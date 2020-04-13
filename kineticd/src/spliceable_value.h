#ifndef KINETIC_SPLICEABLE_VALUE_H_
#define KINETIC_SPLICEABLE_VALUE_H_

#include "kinetic/incoming_value.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingValueInterface;

/*
 * SpliceableValue represents a value that can potentially be transferred
 * directly from a socket to a file using the Linux splice() system call. Users
 * of the class should avoid calling the ToString method, because it
 * necessitates a copy to user space that negates the performance benefits of
 * splice().
 */
class SpliceableValue : public IncomingValueInterface {
    public:
    SpliceableValue(int fd, size_t size);
    size_t size();
    bool TransferToFile(int fd);
    bool ToString(std::string *result);
    void Consume();

    private:
    static const size_t kBlockSize = 65536;
    int fd_;
    size_t size_;
    bool defunct_;
    DISALLOW_COPY_AND_ASSIGN(SpliceableValue);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SPLICEABLE_VALUE_H_
