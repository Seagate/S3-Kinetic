#ifndef KINETIC_USER_SERIALIZER_H_
#define KINETIC_USER_SERIALIZER_H_

#include <vector>
#include <memory>

#include "user.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::vector;
using std::shared_ptr;
using std::unique_ptr;


class UserSerializer {
    public:
    bool serialize(vector<shared_ptr<User>>& users, string& data);
    bool deserialize(string& data, unique_ptr<vector<shared_ptr<User>>>& users);
};

} //namespace kinetic
} //namespace seagate
} //namespace com

#endif  // KINETIC_USER_SERIALIZER_H_
