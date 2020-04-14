#include "domain.h"
#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

Domain::Domain() {}

Domain::Domain(uint64_t offset, const std::string &value, role_t roles, bool tls_required)
    : offset_(offset), value_(value), roles_(roles), tls_required_(tls_required) {}

Domain::Domain(const Domain &domain)
    : offset_(domain.offset()), value_(domain.value()), roles_(domain.roles()),
      tls_required_(domain.tls_required()) {}

uint64_t Domain::offset() const {
    return offset_;
}

void Domain::set_offset(uint64_t offset) {
    offset_ = offset;
}

const std::string& Domain::value() const {
    return value_;
}

void Domain::set_value(const std::string &value) {
    value_ = value;
}

role_t Domain::roles() const {
    return roles_;
}

void Domain::set_roles(role_t roles) {
    roles_ = roles;
}

bool Domain::tls_required() const {
    return tls_required_;
}
void Domain::set_tls_required(bool tls_required) {
    tls_required_ = tls_required;
}

bool Domain::IsApplicable(const std::string &key, role_t role, RequestContext& request_context) {
    //Check if the key satisfies offset & value of the user's ACL
    if (offset_ + value_.size() > key.size()) {
        return false;
    }

    if (key.substr(offset_, value_.size()) != value_) {
        return false;
    }

    return ValidPermissionsAndRequestContext(role, request_context);
}

bool Domain::ValidPermissionsAndRequestContext(role_t role, RequestContext& request_context) {
    //Check if the user has the permission
    if ((roles_ & role) != role) {
        return false;
    }

    return (!tls_required_ || request_context.is_ssl);
}

} // namespace kinetic
} // namespace seagate
} // namespace com
