#include "authorizer.h"
#include <list>
#include "domain.h"
#include "glog/logging.h"
#include <string>

namespace com {
namespace seagate {
namespace kinetic {

Authorizer::Authorizer(UserStoreInterface &user_store, Profiler &profiler, Limits& limits)
    : user_store_(user_store),
    profiler_(profiler),
    limits_(limits) {}

Authorizer::~Authorizer() {}

bool Authorizer::AuthorizeKey(int64_t user_id, role_t permission,
        const std::string &key,
        RequestContext& request_context) {
    Event e;
    profiler_.BeginAutoScoped(kKeyAuthorization, &e);

    User user;
    if (!user_store_.Get(user_id, &user)) {
        VLOG(2) << "User " << user_id << " not found";
        return false;
    }

    std::list<Domain> domains = user.domains();
    for (auto it = domains.begin(); it != domains.end(); ++it) {
        if (it->IsApplicable(key, permission, request_context)) {
            return true;
        }
    }
    VLOG(2) << "User " << user_id << " found but not authorized to perform permission " << permission;
    return false;
}

bool Authorizer::AuthorizeGlobal(int64_t user_id,
        role_t permission,
        RequestContext& request_context) {
    Event e;
    profiler_.BeginAutoScoped(kGlobalAuthorization, &e);
    User user;
    if (!user_store_.Get(user_id, &user)) {
        return false;
    }

    std::list<Domain> domains = user.domains();
    for (auto it = domains.begin(); it != domains.end(); ++it) {
        if ((it->roles() & permission)) {
            if (!it->tls_required() || request_context.is_ssl) {
                return true;
            }
        }
    }

    return false;
}


bool IncrementStrBy1(std::string &str_to_increment) {
    int j = 0;
    for (j = str_to_increment.length()-1; j >= 0; j--) {
        if ((unsigned char)str_to_increment[j] != 0xFF) {
            str_to_increment[j]++;
            return true;
        } else {
            str_to_increment[j] = 0x00;
        }
    }
    return false;
}

void RepeatCharInStr(std::string &s, int num, char val) {
    std::string str(num, val);
    s.assign(str);
}

bool KsrAdjacentCheck(std::string prev, std::string next) {
    int len = prev.length() - 1;
    int i = 0;
    if (!IncrementStrBy1(prev)) {
        return false;
    }
    while (prev[len-i] == 0x00) {
       i++;
    }
    if (prev.compare(0, (prev.length() - i), next) == 0) {
        VLOG(1) << "Key scope regions are adjacent";
        return true;
    } else {
        VLOG(1) << "Key scope regions are not adjacent";
        return false;
    }
}

AuthorizationStatus Authorizer::CheckIfEndKeyBeyondOtherKSR(User user, role_t permission, RequestContext& request_context,
    std::string &current_ksr_end_key, int scope_num, std::string &end_key, bool &end_key_inclusive_flag) {
    int i = 0;
    std::string ksr_start_key;
    std::string ksr_end_key;
    int s_offset;
    std::string s_value;
    unsigned int off_val_len;

    std::list<Domain> domains = user.domains();
    for (auto it = domains.begin(); it != domains.end(); ++it, i++) {
        if (it->ValidPermissionsAndRequestContext(permission, request_context)) {
            s_offset = it->offset();
            s_value = it->value();
            off_val_len = s_offset + s_value.length();
            ksr_start_key = "";

            if ((i == scope_num) && (s_offset == 0)) {
                continue;
            }
            if (it->IsApplicable(current_ksr_end_key, permission, request_context) && (i != scope_num)) {
                ksr_start_key = current_ksr_end_key.substr(0, off_val_len);
                RepeatCharInStr(ksr_end_key, limits_.max_key_size(), 0xFF);
                ksr_end_key.replace(0, ksr_start_key.length(), ksr_start_key);
                if (ksr_end_key.compare(current_ksr_end_key) > 0) {
                    if (ksr_end_key.compare(end_key) >= 0) {
                        VLOG(1) << "CheckIfEndKeyBeyondOtherKSR:: end_key is within key scope region - overlap";
                        return AuthorizationStatus_SUCCESS;
                    } else {
                        return CheckIfEndKeyBeyondOtherKSR(user, permission, request_context, ksr_end_key, i, end_key, end_key_inclusive_flag);
                    }
                } else {
                    if (s_offset == 0) {
                        continue;
                    }
                }
            }
            if (s_offset == 0) {
                ksr_start_key = s_value;
            } else if (s_offset > 0) {
                ksr_start_key = current_ksr_end_key.substr(0, s_offset);
                if ((current_ksr_end_key.compare(s_offset, s_value.length(), s_value) > 0) || (i == scope_num)) {
                    if (!IncrementStrBy1(ksr_start_key)) {
                        continue;
                    }
                }
                ksr_start_key = ksr_start_key + s_value;
            }
            if (!ksr_start_key.empty()) {
                RepeatCharInStr(ksr_end_key, limits_.max_key_size(), 0xFF);
                ksr_end_key.replace(0, ksr_start_key.length(), ksr_start_key);
                if (ksr_end_key.compare(current_ksr_end_key) > 0) {
                    if (KsrAdjacentCheck(current_ksr_end_key, ksr_start_key)) {
                        if (ksr_end_key.compare(end_key) >= 0) {
                            VLOG(1) << "CheckIfEndKeyBeyondOtherKSR:: end_key is within key scope region";
                            return AuthorizationStatus_SUCCESS;
                        } else {
                            return CheckIfEndKeyBeyondOtherKSR(user, permission, request_context, ksr_end_key, i, end_key, end_key_inclusive_flag);
                        }
                    } else if (ksr_start_key.compare(end_key) <= 0) {
                        VLOG(1) << "CheckIfEndKeyBeyondOtherKSR:: end_key is greater than new KSR start_key  ";
                        if ((end_key.compare(ksr_start_key) == 0) && (end_key_inclusive_flag == false)) {
                            continue;
                        }
                        return AuthorizationStatus_INVALID_REQUEST;
                    }
                }
            }
        }
    }
    end_key = current_ksr_end_key;
    end_key_inclusive_flag = true;
    return AuthorizationStatus_SUCCESS;
}

AuthorizationStatus Authorizer::AuthorizeKeyRange(int64_t user_id, role_t permission, std::string &start_key, std::string &end_key,
    bool &start_key_inclusive_flag, bool &end_key_inclusive_flag, RequestContext& request_context) {
    User user;
    std::string ksr_end_key(limits_.max_key_size(), 0xFF);
    std::string ksr_start_key;
    int i = 0;
    std::string nearest_ksr_start_key(limits_.max_key_size(), 0xFF);
    bool status_flag = false;
    int scope_num = 0;
    std::string str;
    int s_offset;
    std::string s_value;
    unsigned int off_val_len;

    if (!user_store_.Get(user_id, &user)) {
        VLOG(1) << "User " << user_id << " not found";
        return AuthorizationStatus_NOT_AUTHORIZED;
    }
    if (start_key.empty()) {
        RepeatCharInStr(start_key, 1, 0x00);
    }
    if (end_key.empty()) {
        RepeatCharInStr(end_key, limits_.max_key_size(), 0xFF);
    }
    if (start_key.compare(end_key) > 0) {
        VLOG(1) << "AuthorizeKeyRange:: start_key is greater than end_key";
        return AuthorizationStatus_INVALID_REQUEST;
    }
    std::list<Domain> domains = user.domains();
    for (auto it = domains.begin(); it != domains.end(); ++it, i++) {
        if (it->IsApplicable(start_key, permission, request_context)) {
            s_offset = it->offset();
            s_value = it->value();
            off_val_len = s_offset + s_value.length();

            VLOG(1) << "AuthorizeKeyRange:: start_key in scope: " << i;
            if (off_val_len > 0) {
                if ((start_key.substr(0, off_val_len)).compare(end_key.substr(0, off_val_len)) == 0) {
                    VLOG(1) << "AuthorizeKeyRange:: end_key within key scope region";
                    return AuthorizationStatus_SUCCESS;
                }
                ksr_start_key = start_key.substr(0, off_val_len);
                ksr_end_key.replace(0, ksr_start_key.length(), ksr_start_key);
                if ((start_key.compare(ksr_end_key) == 0) && (start_key_inclusive_flag == false)) {
                    continue;
                }
                return CheckIfEndKeyBeyondOtherKSR(user, permission, request_context, ksr_end_key, i, end_key, end_key_inclusive_flag);
            }
            return AuthorizationStatus_SUCCESS;
        }
    }
    VLOG(1) << "AuthorizeKeyRange:: start_key not in scope";
    i = 0;
    start_key_inclusive_flag = true;
    for (auto it = domains.begin(); it != domains.end(); ++it, i++) {
        if (it->ValidPermissionsAndRequestContext(permission, request_context)) {
            s_offset = it->offset();
            s_value = it->value();
            off_val_len = s_offset + s_value.length();
            ksr_start_key = "";
            if (s_offset == 0) {
                ksr_start_key = s_value;
            } else {
                if (start_key.length() < off_val_len) {
                    RepeatCharInStr(ksr_start_key, off_val_len, 0x00);
                    ksr_start_key.replace(s_offset, s_value.length(), s_value);
                    ksr_start_key.replace(0, (start_key.substr(0, s_offset)).length(), start_key.substr(0, s_offset));
                    if (ksr_start_key.compare(start_key) < 0) {
                        str = ksr_start_key.substr(0, s_offset);
                        if (!IncrementStrBy1(str)) {
                            continue;
                        }
                        ksr_start_key.replace(0, str.length(), str);
                    }
                } else {
                    ksr_start_key = start_key.substr(0, s_offset);
                    if (start_key.compare(s_offset, s_value.length(), s_value) >= 0) {
                        if (!IncrementStrBy1(ksr_start_key)) {
                            continue;
                        }
                    }
                    ksr_start_key = ksr_start_key + s_value;
                }
            }
            if ((ksr_start_key.compare(nearest_ksr_start_key) < 0) && (ksr_start_key.compare(start_key) > 0) &&
                (ksr_start_key.compare(end_key) <= 0) && (!ksr_start_key.empty())) {
                scope_num = i;
                nearest_ksr_start_key = ksr_start_key;
                status_flag = true;
            }
        }
    }
    if ((!status_flag) || ((end_key.compare(nearest_ksr_start_key) == 0) && (end_key_inclusive_flag == false))) {
        VLOG(1) << "AuthorizeKeyRange:: No key scope region found for this request";
        return AuthorizationStatus_NOT_AUTHORIZED;
    }
    ksr_end_key.replace(0, nearest_ksr_start_key.length(), nearest_ksr_start_key);
    start_key = nearest_ksr_start_key;
    if (ksr_end_key.compare(end_key) >= 0) {
        VLOG(1) << "AuthorizeKeyRange:: end_key within key scope region";
        return AuthorizationStatus_SUCCESS;
    } else {
        return CheckIfEndKeyBeyondOtherKSR(user, permission, request_context, ksr_end_key, scope_num, end_key, end_key_inclusive_flag);
    }
}

} // namespace kinetic
} // namespace seagate
} // namespace com
