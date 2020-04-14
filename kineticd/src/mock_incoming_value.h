#ifndef KINETIC_MOCK_INCOMING_VALUE_H_
#define KINETIC_MOCK_INCOMING_VALUE_H_

#include <string>

#include "gmock/gmock.h"

namespace com {
namespace seagate {
namespace kinetic {

class MockIncomingValue : public IncomingValueInterface {
    public:
    MockIncomingValue() {}
    MOCK_METHOD0(size, size_t());
    MOCK_METHOD1(TransferToFile, bool(int fd));
    MOCK_METHOD1(ToString, bool(std::string *result));
    MOCK_METHOD0(Consume, void());
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_MOCK_INCOMING_VALUE_H_
