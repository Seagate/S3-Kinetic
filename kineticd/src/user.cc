#include "user.h"
#include "domain.h"
#include "kinetic.pb.h"

using namespace com::seagate::kinetic::proto;//NOLINT

namespace com {
namespace seagate {
namespace kinetic {

User::User() {
    maxPriority_ = -1;
}

User::User(int64_t id, const std::string &key, const std::list<Domain> &domains)
    : id_(id), key_(key), domains_(domains) {
    maxPriority_ = -1;
}

int64_t User::id() const {
    return this->id_;
}

const std::string& User::key() const {
    return this->key_;
}

const std::list<Domain> &User::domains() const {
    return domains_;
}

User& User::operator=(const User& source) {
    id_ = source.id_;
    key_ = source.key_;
    maxPriority_ = source.maxPriority_;
    domains_ = source.domains_;
    return *this;
}

} //namespace kinetic
} //namespace seagate
} // namespace com
