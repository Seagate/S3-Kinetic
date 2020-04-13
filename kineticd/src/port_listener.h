#ifndef KINETIC_PORT_LISTENER_H_
#define KINETIC_PORT_LISTENER_H_

#include <string>

#include "kinetic/common.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class PortListener {
    public:
    explicit PortListener(int receive_buffer_size);
    void MakeSocketNonBlocking(int sfd);
    bool Listen(const string & listen_address, uint port, int *fd);
    bool Close(int fd);
    virtual ~PortListener() {
    }

    private:
    const int receive_buffer_size_;
    DISALLOW_COPY_AND_ASSIGN(PortListener);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PORT_LISTENER_H_
