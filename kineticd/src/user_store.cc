#include "user_store.h"

#include <map>
#include "glog/logging.h"

#include "store_operation_status.h"
#include "pthreads_mutex_guard.h"

#include <unordered_set>

using namespace std; //NOLINT


namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::make_shared;
using std::pair;
using std::move;
using std::unique_ptr;

static const int64_t kDemoUserId = 1;
static const std::string kDemoUserKey = "asdfasdf";

UserStore::UserStore(unique_ptr<CautiousFileHandlerInterface> file_handler,
        Limits& limits)
        : file_handler_(move(file_handler)),
        serializer_(),
        limits_(limits),
        users_() {
    CHECK(!pthread_rwlock_init(&lock_, NULL));
}

UserStore::~UserStore() {
    CHECK(!pthread_rwlock_destroy(&lock_));
}

bool UserStore::Init() {
    CHECK(!pthread_rwlock_wrlock(&lock_));
    bool res = true;
    unique_ptr<vector<shared_ptr<User>>> users(nullptr);

    string data;
    FileReadResult readResult = file_handler_->Read(data);
    if (readResult == FileReadResult::IO_ERROR) {
        LOG(WARNING) << "Could not read user data";
        res = false;
        goto cleanup;
    } else if (readResult == FileReadResult::CANT_OPEN_FILE) {
        LOG(INFO) << "No user data file found";
        users_.clear();
        goto cleanup;
    }

    if (!serializer_.deserialize(data, users)) {
        LOG(ERROR) << "Could not deserialize user data";//NO_SPELL
        res = false;
        goto cleanup;
    }

    users_.clear();
    for (auto it = users->begin(); it != users->end(); it++) {
        auto u = *it;
        users_[u->id()] = u;
    }

    cleanup:

    CHECK(!pthread_rwlock_unlock(&lock_));
    return res;
}

UserStoreInterface::Status UserStore::Put(const std::list<User> &changedUsers, bool bUnused) {
    // Validate method argmument
    if (changedUsers.size() == 0) {
        return UserStore::Status::SUCCESS;
    }
    if (changedUsers.size() > limits_.max_identity_count()) {
        return UserStore::Status::EXCEED_LIMIT;
    }

    CHECK(!pthread_rwlock_wrlock(&lock_));
    Status status = Status::SUCCESS;

    bool bDuplicateId = false;
    vector<shared_ptr<User>> users;
    unordered_set<int64_t> userIds;

    // Add all changed users to a new user list
    for (auto changedIt = changedUsers.begin(); !bDuplicateId && changedIt != changedUsers.end(); ++changedIt) {
        auto u = make_shared<User>();
        *u = *changedIt;
        users.push_back(u);
        pair<unordered_set<int64_t>::iterator, bool> itBoolPair = userIds.insert(u->id());
        if (itBoolPair.second == false) {
            bDuplicateId = true;
        }
    }
    userIds.clear();
    if (bDuplicateId) {
        status = Status::DUPLICATE_ID;
    } else {
        if (WriteUsers(users)) {
            // Apply changes to this class memory user list
            users_.clear();
            for (auto changedIt = changedUsers.begin(); changedIt != changedUsers.end(); ++changedIt) {
                auto u = make_shared<User>();
                *u = *changedIt;
                users_[changedIt->id()] = u;
            }
        } else {
            status = Status::FAIL_TO_STORE;
        }
    }
    CHECK(!pthread_rwlock_unlock(&lock_));
    return status;
}

bool UserStore::Get(int64_t id, User *user) {
    CHECK(!pthread_rwlock_rdlock(&lock_));
    bool res = NoLockGet(id, user);
    CHECK(!pthread_rwlock_unlock(&lock_));
    return res;
}

bool UserStore::CreateDemoUser() {
    CHECK(!pthread_rwlock_wrlock(&lock_));

    // Make sure there isn't already a demo user
    if (NoLockDemoUserExists()) {
        LOG(WARNING) << "Attempted to create demo user but already exists";
        CHECK(!pthread_rwlock_unlock(&lock_));
        return false;
    }

    VLOG(1) << "Creating demo user";

    std::list<Domain> domains;
    domains.push_back(Domain(0, "", Domain::kAllRoles, false));
    User demo_user(kDemoUserId, kDemoUserKey, domains);
    demo_user.maxPriority(com::seagate::kinetic::proto::Command_Priority_HIGHEST);
    bool res = NoLockPut(demo_user);

    CHECK(!pthread_rwlock_unlock(&lock_));
    return res;
}

bool UserStore::DemoUserExists() {
    CHECK(!pthread_rwlock_rdlock(&lock_));
    bool res = NoLockDemoUserExists();
    CHECK(!pthread_rwlock_unlock(&lock_));
    return res;
}

bool UserStore::Clear() {
    CHECK(!pthread_rwlock_wrlock(&lock_));

    bool res = true;
    if (!file_handler_->Delete()) {
        LOG(WARNING) << "Could not delete user store file";
        res = false;
        goto cleanup;
    }

    users_.clear();

    CHECK(!pthread_rwlock_unlock(&lock_));

    cleanup:
    return res;
}

// must be called with write lock held
bool UserStore::WriteUsers(vector<shared_ptr<User>>& users) {
    // serialize the users
    string serialized;
    if (!serializer_.serialize(users, serialized)) {
        LOG(ERROR) << "Could not serialize users";
        return false;
    };

    if (!file_handler_->Write(serialized)) {
        LOG(ERROR) << "Could not write serialized users";
        return false;
    }

    return true;
}

// must be called with write lock held
bool UserStore::NoLockPut(const User& user) {
    if (users_.size() >= limits_.max_identity_count()) {
        return false;
    }

    int64_t id = user.id();

    // first, copy the existing users (except for the modified one)
    vector<shared_ptr<User>> user_vec;
    user_vec.reserve(users_.size());
    for (auto it = users_.begin(); it != users_.end(); it++) {
        if (it->second->id() == id) {
            continue;
        }

        user_vec.push_back(it->second);
    }

    // copy the provided user data
    auto u = make_shared<User>();
    *u = user;

    // append the new user
    user_vec.push_back(u);
    if (!WriteUsers(user_vec)) {
        // couldn't persist users, abort
        return false;
    }

    // we wrote the data, so now apply the change
    users_[u->id()] = u;

    return true;
}

// must be called with read lock held
bool UserStore::NoLockGet(int64_t id, User* user) {
    auto it = users_.find(id);

    if (it == users_.end()) {
        return false;
    }

    *user = *it->second;
    return true;
}

bool UserStore::NoLockDemoUserExists() {
    User demo_user;
    return NoLockGet(kDemoUserId, &demo_user);
}


} // namespace kinetic
} // namespace seagate
} // namespace com
