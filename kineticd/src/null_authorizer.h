#ifndef KINETIC_NULL_AUTHORIZER_H_
#define KINETIC_NULL_AUTHORIZER_H_

#include "authorizer.h"
#include "domain.h"

namespace com {
namespace seagate {
namespace kinetic {

/*
 * An authorizer that allows all users to do anything. This is really useful
 * only for testing and for the print_device_information utility.
 */

class NullAuthorizer : public AuthorizerInterface {
    public:
    NullAuthorizer() {}

    virtual bool AuthorizeKey(int64_t user_id, role_t permission,
            const std::string &key, RequestContext& request_context) {
        return true;
    }

    virtual bool AuthorizeGlobal(int64_t user_id, role_t permission,
            RequestContext& request_context) {
        return true;
    }

    virtual AuthorizationStatus AuthorizeKeyRange(int64_t user_id, role_t permission, std::string &start_key,
            std::string &end_key, bool &start_key_inclusive_flag, bool &end_key_inclusive_flag, RequestContext& request_context) {
        return AuthorizationStatus_SUCCESS;
    }
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_NULL_AUTHORIZER_H_
