#ifndef KINETIC_USER_H_
#define KINETIC_USER_H_

#include <list>
#include <string>

#include "domain.h"

namespace com {
namespace seagate {
namespace kinetic {

class User {
    public:
    User();
    User(int64_t id, const std::string &key, const std::list<Domain> &domains);

    int64_t id() const;
    void id(int64_t aId) {
        id_ = aId;
    }
    const std::string& key() const;
    void key(const std::string& aKey) {
        key_ = aKey;
    }
    int maxPriority() {
        return maxPriority_;
    }
    void maxPriority(int priority) {
        maxPriority_ = priority;
    }
    const std::list<Domain> &domains() const;
    User& operator= (const User& source);

    private:
    int64_t id_;
    std::string key_;
    int maxPriority_;
    std::list<Domain> domains_;
};

} //namespace kinetic
} //namespace seagate
} // namespace com

#endif  // KINETIC_USER_H_
