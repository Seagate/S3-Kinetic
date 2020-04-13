#ifndef KINETIC_HMAC_PROVIDER_H_
#define KINETIC_HMAC_PROVIDER_H_

#include "kinetic/common.h"

#include "kinetic.pb.h"

namespace com {
namespace seagate {
namespace kinetic {

class HmacProvider {
    public:
    HmacProvider();
    std::string ComputeHmac(const proto::Message& message,
        const std::string& key) const;
    bool ValidateHmac(const proto::Message& message,
        const std::string& key) const;

    private:
    DISALLOW_COPY_AND_ASSIGN(HmacProvider);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_HMAC_PROVIDER_H_
