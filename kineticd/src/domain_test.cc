#include "gtest/gtest.h"

#include "domain.h"

namespace com {
namespace seagate {
namespace kinetic {

TEST(DomainTest, IsApplicableReturnsTrueWhenAppropriate) {
    Domain domain(4, "abcd", Domain::kWrite, false);
    RequestContext request_context = {false};
    EXPECT_TRUE(domain.IsApplicable("xxxxabcd", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsTrueForEmptyString) {
    Domain domain(4, "", Domain::kWrite, false);
    RequestContext request_context = {false};
    EXPECT_TRUE(domain.IsApplicable("xxxx", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsFalseIfKeyIsTooShort) {
    Domain domain(4, "abcd", Domain::kWrite, false);
    RequestContext request_context = {false};
    EXPECT_FALSE(domain.IsApplicable("xxxxab", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsFalseIfKeyDoesNotMatch) {
    Domain domain(4, "abcd", Domain::kWrite, false);
    RequestContext request_context = {false};
    EXPECT_FALSE(domain.IsApplicable("xxxxabxx", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsFalseIfRoleIsNotAvailable) {
    Domain domain(4, "abcd", Domain::kWrite, false);
    RequestContext request_context = {false};
    EXPECT_FALSE(domain.IsApplicable("xxxxabcd", Domain::kRead, request_context));
}

TEST(DomainTest, IsApplicableReturnsFalseIfAnyRoleIsMissing) {
    Domain domain(4, "abcd", Domain::kWrite | Domain::kDelete, false);
    RequestContext request_context = {false};
    EXPECT_FALSE(domain.IsApplicable("xxxxabcd", Domain::kRead | Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsTrueIfTlsRequiredAndSSLRequest) {
    Domain domain(4, "", Domain::kWrite, true);
    RequestContext request_context = {true};
    EXPECT_TRUE(domain.IsApplicable("xxxx", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsFalseIfTlsRequiredAndNonSSLRequest) {
    Domain domain(4, "", Domain::kWrite, true);
    RequestContext request_context = {false};
    EXPECT_FALSE(domain.IsApplicable("xxxx", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsTrueIfTlsNotRequiredAndNonSSLRequest) {
    Domain domain(4, "", Domain::kWrite, false);
    RequestContext request_context = {false};
    EXPECT_TRUE(domain.IsApplicable("xxxx", Domain::kWrite, request_context));
}

TEST(DomainTest, IsApplicableReturnsTrueIfTlsNotRequiredAndSSLRequest) {
    Domain domain(4, "", Domain::kWrite, false);
    RequestContext request_context = {true};
    EXPECT_TRUE(domain.IsApplicable("xxxx", Domain::kWrite, request_context));
}

} // namespace kinetic
} // namespace seagate
} // namespace com
