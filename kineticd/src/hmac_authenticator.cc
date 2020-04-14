#include "glog/logging.h"

#include "hmac_authenticator.h"

namespace com {
namespace seagate {
namespace kinetic {

HmacAuthenticator::HmacAuthenticator(
    UserStoreInterface &user_store,
    const HmacProvider &hmac_provider)
        : user_store_(user_store), hmac_provider_(hmac_provider) {}

AuthenticationStatus HmacAuthenticator::Authenticate(const proto::Message &message) {
    if (!message.has_hmacauth() ||
            !message.hmacauth().has_identity()) {
        return kUnknownUser;
    }

    User user;
    if (!user_store_.Get(message.hmacauth().identity(), &user)) {
        return kUnknownUser;
    }

    if (!hmac_provider_.ValidateHmac(message, user.key())) {
        return kInvalidMac;
    }

    return kSuccess;
}

void HmacAuthenticator::AssignMac(const User& user, proto::Message *message) {
    message->mutable_hmacauth()->set_identity(user.id());
    message->mutable_hmacauth()->set_hmac(hmac_provider_.ComputeHmac(*message, user.key()));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
