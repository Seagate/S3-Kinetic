#ifndef KINETIC_HMAC_AUTHENTICATOR_H_
#define KINETIC_HMAC_AUTHENTICATOR_H_

#include "authenticator_interface.h"
#include "hmac_provider.h"
#include "user_store_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class HmacAuthenticator : public AuthenticatorInterface {
    public:
    HmacAuthenticator(UserStoreInterface &user_store, const HmacProvider &hmac_provider);
    AuthenticationStatus Authenticate(const proto::Message &message);
    void AssignMac(const User& user, proto::Message *message);

    private:
    // Threadsafe; UserStoreInterface implementations must be threadsafe
    UserStoreInterface &user_store_;

    // Threadsafe; HmacProvider is immutable
    const HmacProvider &hmac_provider_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_HMAC_AUTHENTICATOR_H_
