#ifndef KINETIC_USER_STORE_H_
#define KINETIC_USER_STORE_H_

#include <map>
#include <unordered_map>
#include "pthread.h"

#include "user.h"
#include "user_store_interface.h"
#include "user_serializer.h"
#include "limits.h"

#include "cautious_file_handler.h"

/*
 * A UserStoreInterface implementation backed by json serialization to disk
 */

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::unordered_map;

class UserStore : public UserStoreInterface {
    public:
    explicit UserStore(unique_ptr<CautiousFileHandlerInterface> file_handler,
            Limits& limits);
    UserStoreInterface::Status Put(const std::list<User> &changedUsers, bool bUnused = true);
    bool Get(int64_t id, User *user);
    bool CreateDemoUser();
    bool DemoUserExists();
    bool Init();
    bool Clear();
    ~UserStore();

    private:
    bool WriteUsers(vector<shared_ptr<User>>& users);
    bool NoLockPut(const User& user);
    bool NoLockGet(int64_t id, User *user);
    bool NoLockDemoUserExists();

    unique_ptr<CautiousFileHandlerInterface> file_handler_;
    UserSerializer serializer_;
    Limits& limits_;
    unordered_map<int64_t, shared_ptr<User>> users_;
    pthread_rwlock_t lock_;
    DISALLOW_COPY_AND_ASSIGN(UserStore);
};

} //namespace kinetic
} //namespace seagate
} //namespace com

#endif  // KINETIC_USER_STORE_H_
