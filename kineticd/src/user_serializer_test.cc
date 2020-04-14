#include <math.h>
#include <sstream>
#include "gtest/gtest.h"

#include "user_serializer.h"

using com::seagate::kinetic::Domain;
using com::seagate::kinetic::role_t;
using com::seagate::kinetic::User;
using com::seagate::kinetic::UserSerializer;

using std::unique_ptr;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::list;
using std::stringstream;

class UserSerializerTest : public testing::Test {
    protected:
    UserSerializerTest() :
            user_serializer_(),
            data_(),
            vector_(),
            deser_vector_(new vector<shared_ptr<User>>()) { }

    UserSerializer user_serializer_;
    string data_;
    vector<shared_ptr<User>> vector_;
    unique_ptr<vector<shared_ptr<User>>> deser_vector_;
};

TEST_F(UserSerializerTest, SerializeNoUsers) {
    ASSERT_TRUE(user_serializer_.serialize(vector_, data_));
    ASSERT_EQ("[]\n", data_);
}

TEST_F(UserSerializerTest, Serialize64BitUserId) {
    list<Domain> domains;

    User user(powl(2, 50), "key", domains);
    user.maxPriority(3);
    vector_.push_back(make_shared<User>(user));

    ASSERT_TRUE(user_serializer_.serialize(vector_, data_));
    ASSERT_EQ("[{\"id\":1125899906842624,\"key\":\"key\",\"maxPriority\":3,\"scopes\":[]}]\n", data_);
}

TEST_F(UserSerializerTest, SerializeDomains) {
    list<Domain> domains;
    Domain d(23, "v", Domain::kRead | Domain::kWrite, false);
    Domain d2(33, "u", Domain::kDelete | Domain::kRange, true);
    domains.push_back(d);
    domains.push_back(d2);

    vector_.push_back(make_shared<User>(3, "k", domains));

    ASSERT_TRUE(user_serializer_.serialize(vector_, data_));

    stringstream ss;
    ss << "[{\"id\":3,\"key\":\"k\",\"maxPriority\":-1,\"scopes\":" \
        << "[{\"offset\":23,\"permissions\":3,\"tls_required\":false,\"value\":\"v\"},"
        << "{\"offset\":33,\"permissions\":12,\"tls_required\":true,\"value\":\"u\"}]}]\n";
    ASSERT_EQ(ss.str(), data_);
}

TEST_F(UserSerializerTest, SerializeMultipleUsers) {
    list<Domain> domains;
    Domain d(23, "v", Domain::kRead | Domain::kWrite, false);
    domains.push_back(d);

    list<Domain> no_domains;

    vector_.push_back(make_shared<User>(3, "k", domains));
    vector_.push_back(make_shared<User>(11, "j", no_domains));

    ASSERT_TRUE(user_serializer_.serialize(vector_, data_));
    stringstream ss;
    ss << "[{\"id\":3,\"key\":\"k\",\"maxPriority\":-1,\"scopes\":" \
        << "[{\"offset\":23,\"permissions\":3,\"tls_required\":false,\"value\":\"v\"}]}" \
        << ",{\"id\":11,\"key\":\"j\",\"maxPriority\":-1,\"scopes\":[]}]\n";
    ASSERT_EQ(ss.str(), data_);
}

TEST_F(UserSerializerTest, DeserializeBadJsonReturnsFalse) {
    string data = "foo";
    ASSERT_FALSE(user_serializer_.deserialize(data, deser_vector_));
}

TEST_F(UserSerializerTest, DeserializeEmptyJsonReturnsFalse) {
    string data = "";
    ASSERT_FALSE(user_serializer_.deserialize(data, deser_vector_));
}

TEST_F(UserSerializerTest, DeserializeEmptyArray) {
    string data = "[]";
    ASSERT_TRUE(user_serializer_.deserialize(data, deser_vector_));
    ASSERT_EQ(0UL, deser_vector_->size());
}

TEST_F(UserSerializerTest, DeserializeSingleUser) {
    stringstream ss;
    ss << "[{\"id\":3,\"key\":\"k\",\"maxPriority\":5,\"scopes\":" \
        << "[{\"offset\":23,\"permissions\":3,\"tls_required\":false,\"value\":\"v\"},"
        << "{\"offset\":33,\"permissions\":12,\"tls_required\":true,\"value\":\"u\"}]}]\n";
    string data = ss.str();
    ASSERT_TRUE(user_serializer_.deserialize(data, deser_vector_));
    ASSERT_EQ(1UL, deser_vector_->size());

    auto u = deser_vector_->at(0);

    ASSERT_EQ(3, u->id());
    ASSERT_EQ("k", u->key());

    list<Domain> domains = u->domains();
    ASSERT_EQ(2UL, domains.size());

    Domain d = domains.front();
    domains.pop_front();
    ASSERT_EQ(23U, d.offset());
    ASSERT_EQ(3U, d.roles());
    ASSERT_EQ("v", d.value());
    ASSERT_FALSE(d.tls_required());

    d = domains.front();
    domains.pop_front();
    ASSERT_EQ(33U, d.offset());
    ASSERT_EQ(12U, d.roles());
    ASSERT_EQ("u", d.value());
    ASSERT_TRUE(d.tls_required());
}

TEST_F(UserSerializerTest, DeserializeMultipleUsers) {
    stringstream ss;
    ss << "[{\"id\":3,\"key\":\"k\",\"maxPriority\":5,\"scopes\":[]}," \
        << "{\"id\":4,\"key\":\"i\",\"maxPriority\":5,\"scopes\":[]}]\n";
    string data = ss.str();
    ASSERT_TRUE(user_serializer_.deserialize(data, deser_vector_));
    ASSERT_EQ(2UL, deser_vector_->size());
    auto u = deser_vector_->at(0);

    ASSERT_EQ(3, u->id());
    ASSERT_EQ("k", u->key());
    ASSERT_EQ(0UL, u->domains().size());

    u = deser_vector_->at(1);

    ASSERT_EQ(4, u->id());
    ASSERT_EQ("i", u->key());
    ASSERT_EQ(0UL, u->domains().size());
}

TEST_F(UserSerializerTest, DeserializeInvalidUser) {
    string data = "[{\"id\":1,\"key\":\"k\",\"maxPriority\":5,\"scopesXYZFOOBAR\":[]}]";
    ASSERT_FALSE(user_serializer_.deserialize(data, deser_vector_));
}

TEST_F(UserSerializerTest, DeserializeInvalidScope) {
    string data = "[{\"id\":1,\"key\":\"k\",\"maxPriority\":5,\"scopes\":[{}]}]";
    ASSERT_FALSE(user_serializer_.deserialize(data, deser_vector_));
}
