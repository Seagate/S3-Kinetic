#ifndef KINETIC_AUTHORIZER_INTERFACE_H_
#define KINETIC_AUTHORIZER_INTERFACE_H_

#include <string>

#include "domain.h"
#include "request_context.h"

namespace com {
namespace seagate {
namespace kinetic {

enum AuthorizationStatus {
    AuthorizationStatus_SUCCESS,
    AuthorizationStatus_NOT_AUTHORIZED,
    AuthorizationStatus_INVALID_REQUEST
};

/**
* Implementations must be threadsafe
*/
class AuthorizerInterface {
    public:
    virtual ~AuthorizerInterface() {}

    // Indicates whether the user has the given permission for a particular key
    virtual bool AuthorizeKey(int64_t user_id, role_t permission, const std::string &key,
            RequestContext& request_context) = 0;

    // Indicates whether the user has the given global permission (namely setup or security)
    virtual bool AuthorizeGlobal(int64_t user_id, role_t permission,
            RequestContext& request_context) = 0;

    virtual AuthorizationStatus AuthorizeKeyRange(int64_t user_id, role_t permission, std::string &start_key,
            std::string &end_key, bool &start_key_inclusive_flag, bool &end_key_inclusive_flag, RequestContext& request_context) = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_AUTHORIZER_INTERFACE_H_
