#ifndef KINETIC_MOCK_AUTHORIZER_H_
#define KINETIC_MOCK_AUTHORIZER_H_

#include "gmock/gmock.h"

#include "authorizer_interface.h"
#include "domain.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockAuthorizer : public AuthorizerInterface {
    public:
    MockAuthorizer() {}
    MOCK_METHOD4(AuthorizeKey, bool(int64_t user_id,
                                    role_t permission,
                                    const std::string &key,
                                    RequestContext& request_contex));
    MOCK_METHOD3(AuthorizeGlobal, bool(int64_t user_id,
                                       role_t permission,
                                        RequestContext& request_contex));
    MOCK_METHOD7(AuthorizeKeyRange, AuthorizationStatus(int64_t user_id,
                                       role_t permission,
                                       std::string &start_key,
                                       std::string &end_key,
                                       bool &start_key_inclusive_flag,
                                       bool &end_key_inclusive_flag,
                                        RequestContext& request_contex));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_AUTHORIZER_H_
