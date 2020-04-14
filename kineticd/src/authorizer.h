#ifndef KINETIC_AUTHORIZER_H_
#define KINETIC_AUTHORIZER_H_

#include <string>

#include "kinetic/common.h"

#include "authorizer_interface.h"
#include "domain.h"
#include "user_store_interface.h"
#include "profiler.h"
#include "limits.h"

namespace com {
namespace seagate {
namespace kinetic {

class Authorizer : public AuthorizerInterface {
    public:
    explicit Authorizer(UserStoreInterface &user_store, Profiler &profiler, Limits& limits);
    ~Authorizer();
    bool AuthorizeKey(int64_t user_id,
            role_t permission,
            const std::string &key,
            RequestContext& request_contex);
    bool AuthorizeGlobal(int64_t user_id, role_t permission, RequestContext& request_contex);
    AuthorizationStatus AuthorizeKeyRange(int64_t user_id,
                        role_t permission,
                        std::string &start_key,
                        std::string &end_key,
                        bool &start_key_inclusive_flag,
                        bool &end_key_inclusive_flag,
                        RequestContext& request_context);

    private:
    // Threadsafe; UserStoreInterface implementations must be threadsafe
    UserStoreInterface &user_store_;

    // Threadsafe
    Profiler &profiler_;
    Limits& limits_;
    AuthorizationStatus CheckIfEndKeyBeyondOtherKSR(User user, role_t permission, RequestContext& request_context,
        std::string &current_ksr_end_key, int scope_num,
    std::string &end_key, bool &end_key_inclusive_flag);
    DISALLOW_COPY_AND_ASSIGN(Authorizer);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_AUTHORIZER_H_
