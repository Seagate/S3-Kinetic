#ifndef KINETIC_MOCK_USER_STORE_H_
#define KINETIC_MOCK_USER_STORE_H_

#include "gmock/gmock.h"

#include "user_store_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockUserStore : public UserStoreInterface {
    public:
    MockUserStore() {}
    MOCK_METHOD2(Put, UserStoreInterface::Status(const std::list<User> &changedUsers, bool bUnused));
    MOCK_METHOD2(Get, bool(int64_t id, User *user));
    MOCK_METHOD0(CreateDemoUser, bool());
    MOCK_METHOD0(DemoUserExists, bool());
    MOCK_METHOD0(Init, bool());
    MOCK_METHOD0(Clear, bool());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_USER_STORE_H_
