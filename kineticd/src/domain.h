#ifndef KINETIC_DOMAIN_H_
#define KINETIC_DOMAIN_H_

#include <string>

#include "kinetic/common.h"
#include "request_context.h"

namespace com {
namespace seagate {
namespace kinetic {

typedef uint32_t role_t;

class Domain {
    public:
    // User roles: these can be ORed together to form a bit mask indicating a
    // user's capabilities for a given domain.
    static const role_t kRead = 1;
    static const role_t kWrite = 1 << 1;
    static const role_t kDelete = 1 << 2;
    static const role_t kRange = 1 << 3;
    static const role_t kSetup = 1 << 4;
    static const role_t kP2Pop = 1 << 5;
    static const role_t kGetLog = 1 << 6;
    static const role_t kSecurity = 1 << 7;
    static const role_t kPower = 1 << 8;

    static const role_t kAllRoles = 0xFFFFFFFF;

    Domain();
    Domain(const Domain &domain);
    Domain(uint64_t offset, const std::string &value, role_t roles, bool tls_required);
    uint64_t offset() const;
    void set_offset(uint64_t offset);
    const std::string& value() const;
    void set_value(const std::string &value);
    role_t roles() const;
    void set_roles(uint32_t roles);
    bool tls_required() const;
    void set_tls_required(bool tls_required);
    bool IsApplicable(const std::string &key, uint32_t role, RequestContext& request_context);
    bool ValidPermissionsAndRequestContext(role_t role, RequestContext& request_context);

    private:
    uint64_t offset_;
    std::string value_;
    uint32_t roles_;
    bool tls_required_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_DOMAIN_H_
