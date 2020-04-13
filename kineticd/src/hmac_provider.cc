#include <arpa/inet.h>
#include <list>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "glog/logging.h"

#include "hmac_provider.h"

namespace com {
namespace seagate {
namespace kinetic {

HmacProvider::HmacProvider() {}

std::string HmacProvider::ComputeHmac(const proto::Message& message,
        const std::string& key) const {
    std::string input(message.commandbytes());

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_CTX ctx;
    HMAC_CTX_init(&ctx);
    HMAC_Init_ex(&ctx, key.c_str(), key.length(), EVP_sha1(), NULL);

    if (input.length() != 0) {
        uint32_t message_length_bigendian = htonl(input.length());
        HMAC_Update(&ctx, reinterpret_cast<unsigned char *>(&message_length_bigendian),
            sizeof(uint32_t));
        HMAC_Update(&ctx, reinterpret_cast<const unsigned char *>(input.c_str()),
            input.length());
    }
    unsigned char result[SHA_DIGEST_LENGTH];
    unsigned int result_length = SHA_DIGEST_LENGTH;
    HMAC_Final(&ctx, result, &result_length);
    HMAC_CTX_cleanup(&ctx);
#else
    HMAC_CTX *h = HMAC_CTX_new();
    HMAC_Init_ex(h, key.c_str(), key.length(), EVP_sha1(), NULL);

    if (input.length() != 0) {
        uint32_t message_length_bigendian = htonl(input.length());
        HMAC_Update(h, reinterpret_cast<unsigned char *>(&message_length_bigendian),
            sizeof(uint32_t));
        HMAC_Update(h, reinterpret_cast<const unsigned char *>(input.c_str()),
            input.length());
    }
    unsigned char result[SHA_DIGEST_LENGTH];
    unsigned int result_length = SHA_DIGEST_LENGTH;
    HMAC_Final(h, result, &result_length);
    HMAC_CTX_free(h);
#endif
    return std::string(reinterpret_cast<char *>(result), result_length);
}

bool HmacProvider::ValidateHmac(const proto::Message& message,
        const std::string& key) const {
    std::string correct_hmac(ComputeHmac(message, key));

    if (!message.has_hmacauth() ||
            (!message.hmacauth().has_hmac())) {
        return false;
    }

    const std::string &provided_hmac = message.hmacauth().hmac();

    if (provided_hmac.length() != correct_hmac.length()) {
        return false;
    }

    int result = 0;
    for (size_t i = 0; i < correct_hmac.length(); i++) {
        result |= provided_hmac[i] ^ correct_hmac[i];
    }

    return result == 0;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
