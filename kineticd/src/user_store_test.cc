#include <stdlib.h>
#include "gtest/gtest.h"

#include "domain.h"
#include "user_store.h"
#include "user.h"
#include "user_store_interface.h"
#include "cautious_file_handler.h"
#include "limits.h"

using com::seagate::kinetic::Domain;
using com::seagate::kinetic::role_t;
using com::seagate::kinetic::UserStore;
using com::seagate::kinetic::UserStoreInterface;
using com::seagate::kinetic::User;
using com::seagate::kinetic::BlackholeCautiousFileHandler;
using com::seagate::kinetic::CautiousFileHandlerInterface;
using com::seagate::kinetic::CautiousFileHandler;
using com::seagate::kinetic::Limits;

using std::unique_ptr;
using std::move;

class UserStoreTest : public testing::Test {
    protected:
    UserStoreTest() :
        limits_(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000),
        user_store_(std::move(unique_ptr<CautiousFileHandlerInterface>(
                new BlackholeCautiousFileHandler())), limits_) {}

    virtual void SetUp() {
        ASSERT_NE(-1, system("mkdir -p test_user_store"));
    }

    virtual void TearDown() {
        ASSERT_NE(-1, system("rm -rf test_user_store test_user_store2"));
    }


    Limits limits_;
    UserStore user_store_;
};

TEST_F(UserStoreTest, GetReturnsUser) {
    Domain domain(4, "xxxx", Domain::kRead | Domain::kDelete, false);
    std::list<Domain> expected_domains;
    expected_domains.push_back(domain);
    User expected_user(42, "my key", expected_domains);
    std::list<User> users;
    users.push_back(expected_user);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, this->user_store_.Put(users));

    User actual_user;
    ASSERT_TRUE(this->user_store_.Get(42, &actual_user));
    EXPECT_EQ(42, actual_user.id());
    EXPECT_EQ("my key", actual_user.key());
    std::list<Domain> actual_domains = actual_user.domains();
    ASSERT_EQ(1u, actual_domains.size());
    EXPECT_EQ(4u, actual_domains.front().offset());
    EXPECT_EQ("xxxx", actual_domains.front().value());
    EXPECT_EQ(Domain::kRead | Domain::kDelete, actual_domains.front().roles());
}

TEST_F(UserStoreTest, GetReturnsFalseIfNotPresent) {
    User actual_user;
    EXPECT_FALSE(this->user_store_.Get(0, &actual_user));
}

TEST_F(UserStoreTest, RepeatedPutOverwritesExistingUser) {
    User original_user(42, "original key", std::list<Domain>());
    User updated_user(42, "new key", std::list<Domain>());
    User final_user;
    std::list<User> users;
    users.push_back(original_user);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, this->user_store_.Put(users));
    users.clear();
    users.push_back(updated_user);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, this->user_store_.Put(users));
    ASSERT_TRUE(this->user_store_.Get(42, &final_user));
    ASSERT_EQ("new key", final_user.key());
}

TEST_F(UserStoreTest, CreateDemoUserCreatesADemoUser) {
    ASSERT_TRUE(this->user_store_.CreateDemoUser());

    User demo_user;
    ASSERT_TRUE(this->user_store_.Get(1, &demo_user));
    ASSERT_EQ("asdfasdf", demo_user.key());
}

TEST_F(UserStoreTest, CreateDemoUserFailsIfUserAlreadyExists) {
    ASSERT_TRUE(this->user_store_.CreateDemoUser());
    ASSERT_FALSE(this->user_store_.CreateDemoUser());
}

TEST_F(UserStoreTest, DemoUserCanDoAnything) {
    ASSERT_TRUE(this->user_store_.CreateDemoUser());

    User user;
    ASSERT_TRUE(this->user_store_.Get(1, &user));
    ASSERT_EQ(1U, user.domains().size());

    Domain domain = user.domains().front();
    EXPECT_EQ(0U, domain.offset());
    EXPECT_EQ("", domain.value());
    role_t all_roles = 0xFFFFFFFF;
    EXPECT_EQ(all_roles, domain.roles());
}

TEST_F(UserStoreTest, DemoUserExistsReturnsFalseIfEmpty) {
    EXPECT_FALSE(this->user_store_.DemoUserExists());
}

TEST_F(UserStoreTest, DemoUserExistsReturnsTrueIfExists) {
    ASSERT_TRUE(this->user_store_.CreateDemoUser());

    EXPECT_TRUE(this->user_store_.DemoUserExists());
}

TEST_F(UserStoreTest, DemoUserExistsReturnsFalseIfOtherUsersExist) {
    User user_1(42, "original key", std::list<Domain>());
    User user_2(43, "original key", std::list<Domain>());
    User user_3(44, "original key", std::list<Domain>());

    std::list<User> users;
    users.push_back(user_1);
    users.push_back(user_2);
    users.push_back(user_3);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));

    EXPECT_FALSE(this->user_store_.DemoUserExists());
}

TEST_F(UserStoreTest, ReadSavedDataWithNewStore) {
    Limits limits(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000);
    unique_ptr<CautiousFileHandlerInterface> handler(
            new CautiousFileHandler("test_user_store", "users.json"));
    UserStore us(move(handler), limits);
    ASSERT_EQ(true, us.Init());

    User user_1(42, "original key", std::list<Domain>());
    User user_2(43, "original key", std::list<Domain>());
    User user_3(44, "original key", std::list<Domain>());

    std::list<User> users;
    users.push_back(user_1);
    users.push_back(user_2);
    users.push_back(user_3);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, us.Put(users));

    User u;

    ASSERT_TRUE(us.Get(42, &u));
    ASSERT_EQ(42, u.id());

    // now, read from another store with the same handler
    unique_ptr<CautiousFileHandlerInterface> handler2(
            new CautiousFileHandler("test_user_store", "users.json"));
    UserStore us2(move(handler2), limits);
    ASSERT_EQ(true, us2.Init());

    ASSERT_TRUE(us2.Get(43, &u));
    ASSERT_EQ(43, u.id());
}

TEST_F(UserStoreTest, InitClearsWhenFileCantBeFound) {
    User user_1(42, "original key", std::list<Domain>());
    std::list<User> users;
    users.push_back(user_1);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));

    User u;

    ASSERT_TRUE(user_store_.Get(42, &u));

    ASSERT_NE(-1, system("rm -rf test_user_store"));

    ASSERT_TRUE(user_store_.Init());

    ASSERT_FALSE(user_store_.Get(42, &u));
}

TEST_F(UserStoreTest, InitClearsWhenFileIsFound) {
    User user_1(42, "original key", std::list<Domain>());
    User user_2(43, "original key", std::list<Domain>());

    Limits limits(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000);
    unique_ptr<CautiousFileHandlerInterface> handler(
        new CautiousFileHandler("test_user_store", "users.json"));
    UserStore us(move(handler), limits);
    std::list<User> users;
    users.push_back(user_1);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, us.Put(users));
    User u;

    ASSERT_TRUE(us.Get(42, &u));

    ASSERT_NE(-1, system("cp -r test_user_store test_user_store2"));

    users.clear();
    users.push_back(user_2);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, us.Put(users));

    ASSERT_TRUE(us.Get(43, &u));

    ASSERT_NE(-1, system("rm -rf test_user_store"));
    ASSERT_NE(-1, system("mv test_user_store2 test_user_store"));

    ASSERT_TRUE(us.Init());

    ASSERT_TRUE(us.Get(42, &u));
    ASSERT_FALSE(us.Get(43, &u));
}

TEST_F(UserStoreTest, ClearRemovesAllUsers) {
    Limits limits(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000);
    unique_ptr<CautiousFileHandlerInterface> handler(
            new CautiousFileHandler("test_user_store", "users.json"));
    UserStore us(move(handler), limits);
    ASSERT_EQ(true, us.Init());

    User user_1(42, "original key", std::list<Domain>());
    std::list<User> users;
    users.push_back(user_1);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, us.Put(users));

    User u;

    ASSERT_TRUE(us.Get(42, &u));

    us.Clear();

    ASSERT_FALSE(us.Get(42, &u));
}

TEST_F(UserStoreTest, ClearLeavesStoreInUsableState) {
    Limits limits(100, 100, 100, 1024, 1024, 10, 10, 10, 10, 100, 5, 64*1024*1024, 24000);
    unique_ptr<CautiousFileHandlerInterface> handler(
            new CautiousFileHandler("test_user_store", "users.json"));
    UserStore us(move(handler), limits);
    ASSERT_EQ(true, us.Init());

    User user_1(42, "original key", std::list<Domain>());
    std::list<User> users;
    users.push_back(user_1);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, us.Put(users));

    us.Clear();

    User u;
    ASSERT_FALSE(us.Get(42, &u));

    users.clear();
    users.push_back(user_1);
    ASSERT_EQ(UserStoreInterface::Status::SUCCESS, us.Put(users));
    ASSERT_TRUE(us.Get(42, &u));

    // now, read from another store with the same handler
    unique_ptr<CautiousFileHandlerInterface> handler2(
            new CautiousFileHandler("test_user_store", "users.json"));
    UserStore us2(move(handler2), limits);
    ASSERT_EQ(true, us2.Init());

    ASSERT_TRUE(us2.Get(42, &u));
    ASSERT_EQ(42, u.id());
}
