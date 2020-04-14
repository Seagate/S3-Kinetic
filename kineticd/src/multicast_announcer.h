#ifndef KINETIC_MULTICAST_ANNOUNCER_H_
#define KINETIC_MULTICAST_ANNOUNCER_H_

#include <string>
#include <vector>
#include <memory>
#include <netinet/in.h>

#include <gtest/gtest_prod.h>

#include "announcer_interface.h"
#include "getlog_multicastor.h"

namespace com {
namespace seagate {
namespace kinetic {

enum OptionsFileState {
    INITIAL, LOADED, NOT_LOADED
};

class MulticastAnnouncer : public AnnouncerInterface {
    public:
        MulticastAnnouncer(
                std::string name,
                std::string vendor_options_file,
                std::string ipv6_vendor_options_file,
                std::shared_ptr<GetLogMulticastor> multicastor,
                NetworkInterfaces& network_interfaces);
        virtual bool Configure();
        virtual void Announce();

    private:
        bool ReadFirstLineOfFile(const std::string &path, std::string &line);
        bool ReadAnnouncerInfo(bool ipv4);
        bool LoadVendorOptions(std::string &vendor_options_file, std::string &vendor_options,
                               OptionsFileState &state);
        void HexToBytes(const std::string &hex, std::string &bytes);
        bool ParseVendorOptions(const std::string &hex_encoded_vendor_options);

        std::string name_;
        std::string ipv4_;
        std::string vendor_options_file_;
        std::string ipv6_vendor_options_file_;
        std::shared_ptr<GetLogMulticastor> multicastor_;
        NetworkInterfaces& network_interfaces_;
        bool bAnnounce_;
        bool bAnnounce6_;
        in_addr_t ip_;
        in6_addr ip6_;
        uint16_t port_;
        uint16_t port6_;
        OptionsFileState ipv4_options_state_;
        OptionsFileState ipv6_options_state_;


        // for tests...
        FRIEND_TEST(MulticastAnnouncerTest, LoadVendorOptionsReturnsFalseIfNoFiles);
        FRIEND_TEST(MulticastAnnouncerTest, HexToBytesWorks);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsHandlesDisabledMulticast);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsHandlesIPv6MulticastAddress);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsUsesDefaultIpv6MulticastConfiguration);//NOLINT
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsHandlesMulticastAddress);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsAllowsUnusedPaddingAfter);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsHandlesUnicastAddress);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsDetectsImpossiblyLongLength);
        FRIEND_TEST(MulticastAnnouncerTest, ParseVendorOptionsIgnoresUnexpectedOption);
};

} // namespace kinetic
} // namespace seagate
} // namespace com
#endif  // KINETIC_MULTICAST_ANNOUNCER_H_
