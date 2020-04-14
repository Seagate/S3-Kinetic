#ifndef KINETIC_VALUE_FACTORY_H_
#define KINETIC_VALUE_FACTORY_H_

#include "kinetic/incoming_value.h"
#include "kernel_mem_mgr.h"
#include "openssl/ssl.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::IncomingValueFactoryInterface;
using ::kinetic::IncomingValueInterface;

class ValueFactory : public IncomingValueFactoryInterface {
    public:
    ValueFactory();
    virtual ~ValueFactory() {
        if (buff_) {
            free(buff_);
        }
    }
    IncomingValueInterface *NewValue(int fd, size_t n);
    IncomingValueInterface *SslNewValue(SSL *ssl, size_t n);

    private:
    IncomingValueInterface *SslNewSmallValue(SSL *ssl, size_t n);
    IncomingValueInterface *SslNewLargeValue(SSL *ssl, size_t n);
    void consume(SSL *ssl, size_t n);
    void consume(int fd, size_t n);

    private:
    char* buff_;

    DISALLOW_COPY_AND_ASSIGN(ValueFactory);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_VALUE_FACTORY_H_
