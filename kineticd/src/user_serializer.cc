#include "user_serializer.h"

#define JSON_IS_AMALGAMATION
#include "json/json.h"

#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::to_string;
using std::list;
using std::make_shared;
using std::move;

bool UserSerializer::serialize(vector<shared_ptr<User>>& users,
        string& data) {
    Json::Value root;
    root.resize(users.size());

    uint32_t count = 0;
    for (auto i = users.begin(); i != users.end(); i++) {
        shared_ptr<User> u = *i;

        Json::Value v;
        v["id"] = Json::Value((Json::UInt64) u->id());
        v["key"] = Json::Value(u->key());

        Json::Value scopes;
        scopes.resize(u->domains().size());

        uint32_t scope_count = 0;
        for (auto scope = u->domains().begin(); scope != u->domains().end(); scope++) {
            // using names that map to what's current in kinetic.proto: domain -> scope, etc.
            Json::Value scope_json;
            scope_json["value"] = Json::Value(scope->value());
            scope_json["offset"] = Json::Value((Json::UInt64) scope->offset());
            scope_json["permissions"] = Json::Value(scope->roles());
            scope_json["tls_required"] = Json::Value(scope->tls_required());

            scopes[scope_count] = scope_json;
            scope_count++;
        }

        v["scopes"] = scopes;
        v["maxPriority"] = Json::Value(u->maxPriority());

        root[count] = v;
        count++;
    }

    Json::FastWriter writer;
    data = writer.write(root);

    return true;
}

bool UserSerializer::deserialize(string& data, unique_ptr<vector<shared_ptr<User>>>& users) {
    Json::Value root;
    Json::Reader reader;

    bool ok = reader.parse(data, root);
    if (!ok) {
        LOG(ERROR) << "Could not parse user json";//NO_SPELL
        return false;
    }

    unique_ptr<vector<shared_ptr<User>>> vec(new vector<shared_ptr<User>>());
    vec->reserve(root.size());

    for (Json::ArrayIndex i = 0; i < root.size(); i++) {
        Json::Value u = root[i];

        if (!(u.isMember("id") && u.isMember("key") && u.isMember("scopes") && u.isMember("maxPriority"))) {
            LOG(ERROR) << "User had invalid json";//NO_SPELL
            return false;
        }

        int64_t id = u["id"].asInt64();
        string key = u["key"].asString();
        Json::Value scopes_json = u["scopes"];

        list<Domain> domains;

        for (Json::ArrayIndex j = 0; j < scopes_json.size(); j++) {
            Json::Value s = scopes_json[j];

            if (!(s.isMember("value")
                    && s.isMember("offset")
                    && s.isMember("permissions")
                    && s.isMember("tls_required"))) {
                LOG(ERROR) << "Scope had invalid json";//NO_SPELL
                return false;
            }

            uint64_t offset = s["offset"].asUInt64();
            string value = s["value"].asString();
            uint32_t permissions = s["permissions"].asUInt();
            bool tls_required = s["tls_required"].asBool();

            Domain d(offset, value, permissions, tls_required);
            domains.push_back(d);
        }

        auto user = make_shared<User>(id, key, domains);
        user.get()->maxPriority(u["maxPriority"].asInt());
        vec->push_back(user);
    }

    users.reset(move(vec.release()));

    return true;
}

} //namespace kinetic
} //namespace seagate
} //namespace com
