#ifndef KINETIC_OPENSSL_INITIALIZATION_H_
#define KINETIC_OPENSSL_INITIALIZATION_H_

#include "openssl/ssl.h"

#include <string>

namespace com {
namespace seagate {
namespace kinetic {

SSL_CTX *initialize_openssl(const std::string &private_key, const std::string &certificate);

void free_openssl(SSL_CTX *ssl_context);

}  // namespace kinetic
}  // namespace seagate
}  // namespace com

#endif  // KINETIC_OPENSSL_INITIALIZATION_H_
