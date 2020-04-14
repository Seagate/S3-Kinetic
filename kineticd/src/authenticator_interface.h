#ifndef KINETIC_AUTHENTICATOR_INTERFACE_H_
#define KINETIC_AUTHENTICATOR_INTERFACE_H_

#include "kinetic/incoming_value.h"

#include "kinetic.pb.h"
#include "user.h"

namespace com {
namespace seagate {
namespace kinetic {

enum AuthenticationStatus {
    kSuccess,
    kUnknownUser,
    kInvalidMac
};

/**
* Handles checking the HMAC in a Message and making sure the user has the needed permissions.
* It also handles setting the correct HMAC for the response message. Implementations must be
* threadsafe.
*/
class AuthenticatorInterface {
    public:
    virtual ~AuthenticatorInterface() {}
    virtual AuthenticationStatus Authenticate(const proto::Message &message) = 0;
    virtual void AssignMac(const User& user, proto::Message *message) = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_AUTHENTICATOR_INTERFACE_H_
