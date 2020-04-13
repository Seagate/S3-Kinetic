#ifndef KINETIC_USER_STORE_INTERFACE_H_
#define KINETIC_USER_STORE_INTERFACE_H_

#include <string>

#include "user.h"
#include "kinetic.pb.h"

using namespace com::seagate::kinetic::proto;//NOLINT


namespace com {
namespace seagate {
namespace kinetic {

/**
* Implementations must be threadsafe
*/
class UserStoreInterface {
    public:
    enum class Status {
        SUCCESS,
        EXCEED_LIMIT,
        FAIL_TO_STORE,
        DUPLICATE_ID
    };

    virtual ~UserStoreInterface() {}
    virtual bool Init() = 0;
    virtual Status Put(const std::list<User> &changedUsers, bool bUnused = true) = 0;
    virtual bool Get(int64_t id, User *user) = 0;
    virtual bool CreateDemoUser() = 0;
    virtual bool DemoUserExists() = 0;
    virtual bool Clear() = 0;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_USER_STORE_INTERFACE_H_
