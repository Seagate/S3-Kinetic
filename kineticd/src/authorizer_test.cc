#include "gtest/gtest.h"

#include "authorizer.h"
#include "domain.h"
#include "user_store.h"
#include "std_map_key_value_store.h"
#include "request_context.h"
#include "limits.h"

namespace com {
namespace seagate {
namespace kinetic {

class AuthorizerTest : public ::testing::Test {
    protected:
    AuthorizerTest() :
        limits_(4000, 128, 2048, 1024*1024, 1024*1024,
                100, 30, 20, 200, 100, 5, 64*1024*1024, 24000),
        user_store_(std::move(unique_ptr<CautiousFileHandlerInterface>(
                new BlackholeCautiousFileHandler())), limits_),
        profiler_(),
        authorizer_(user_store_, profiler_, limits_) {}

    void CreateUserWithSingleDomain(
            int64_t id,
            int64_t offset,
            const std::string &value,
            role_t roles,
            bool tls_required) {
        Domain domain(offset, value, roles, tls_required);
        std::list<Domain> domains;
        domains.push_back(domain);
        User user(id, "secret", domains);
        std::list<User> users;
        users.push_back(user);
        ASSERT_EQ(UserStoreInterface::Status::SUCCESS, user_store_.Put(users));
    }

    Limits limits_;
    UserStore user_store_;
    Profiler profiler_;
    Authorizer authorizer_;
};

TEST_F(AuthorizerTest, AuthorizeKeyFailsIfUserDoesNotExist) {
    RequestContext request_context;
    EXPECT_FALSE(authorizer_.AuthorizeKey(100, Domain::kWrite, "xxxxabcd", request_context));
}

TEST_F(AuthorizerTest, AuthorizeKeyFailsIfThereAreNoApplicableDomains) {
    CreateUserWithSingleDomain(100, 4, "abcd", Domain::kRead, false);
    RequestContext request_context;
    EXPECT_FALSE(authorizer_.AuthorizeKey(100, Domain::kWrite, "xxxxabcd", request_context));
}

TEST_F(AuthorizerTest, AuthorizeKeySucceedsIfThereIsAnApplicableDomain) {
    CreateUserWithSingleDomain(100, 4, "abcd", Domain::kWrite, false);
    RequestContext request_context;
    EXPECT_TRUE(authorizer_.AuthorizeKey(100, Domain::kWrite, "xxxxabcd", request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalFailsIfUserDoesNotExist) {
    RequestContext request_context;
    EXPECT_FALSE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalFailsIfThereAreNoApplicableDomains) {
    CreateUserWithSingleDomain(100, 0, "", Domain::kWrite, false);
    RequestContext request_context;
    EXPECT_FALSE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalSucceedsIfThereIsAnApplicableDomain) {
    CreateUserWithSingleDomain(100, 0, "", Domain::kSetup, false);
    RequestContext request_context;
    EXPECT_TRUE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalSucceedsIfTlsRequiredTrueAndRequestIsSSL) {
    CreateUserWithSingleDomain(100, 0, "", Domain::kSetup, true);
    RequestContext request_context = {true};
    EXPECT_TRUE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalFailsIfTlsRequiredTrueAndRequestNotSSL) {
    CreateUserWithSingleDomain(100, 0, "", Domain::kSetup, true);
    RequestContext request_context = {false};
    EXPECT_FALSE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalSucceedsIfTlsRequiredFalseAndRequestIsSSL) {
    CreateUserWithSingleDomain(100, 0, "", Domain::kSetup, false);
    RequestContext request_context = {true};
    EXPECT_TRUE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

TEST_F(AuthorizerTest, AuthorizeGlobalSucceedsIfTlsRequiredFalseAndRequestIsNotSSL) {
    CreateUserWithSingleDomain(100, 0, "", Domain::kSetup, false);
    RequestContext request_context = {false};
    EXPECT_TRUE(authorizer_.AuthorizeGlobal(100, Domain::kSetup, request_context));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
