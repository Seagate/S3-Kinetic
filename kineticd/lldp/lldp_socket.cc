#include <sys/socket.h>
#include <net/ethernet.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <assert.h> // NOLINT
#include "includes/lldp_socket.h"

namespace lldp_kin {

LldpSocket::LldpSocket(const std::vector<std::string>& iface_names) : sock_fd_(-1), current_mac_(6, 0) {
    buff_ = malloc(kMAX_FRAME_SIZE);

    sock_fd_ = socket(AF_PACKET, SOCK_RAW, ETH_P_LLDP);

    if (sock_fd_ < 0) {
        perror("Bad file descriptor");
    }

    struct packet_mreq mr;
    memset(&mr, 0, sizeof(mr));
    mr.mr_type = PACKET_MR_MULTICAST;
    mr.mr_alen = ETH_ALEN;
    mr.mr_address[0] = kMULTICAST_MAC0;
    mr.mr_address[1] = kMULTICAST_MAC1;
    mr.mr_address[2] = kMULTICAST_MAC2;
    mr.mr_address[3] = kMULTICAST_MAC3;
    mr.mr_address[4] = kMULTICAST_MAC4;
    mr.mr_address[5] = kMULTICAST_MAC5;

    for (auto it : iface_names) {
        struct ifreq if_idx;
        memset(&if_idx, 0, sizeof(struct ifreq));
        strncpy((char*)if_idx.ifr_name, it.c_str(), it.length());

        if (ioctl(sock_fd_, SIOCGIFINDEX, &if_idx) < 0) {
            perror("SIOCGIFINDEX");
        }

        mr.mr_ifindex = if_idx.ifr_ifindex;
        setsockopt(sock_fd_, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));

        ifaces_.insert(std::make_pair(it, if_idx));
    }

    dest_address_.sll_halen = ETH_ALEN;
    dest_address_.sll_addr[0] = kMULTICAST_MAC0;
    dest_address_.sll_addr[1] = kMULTICAST_MAC1;
    dest_address_.sll_addr[2] = kMULTICAST_MAC2;
    dest_address_.sll_addr[3] = kMULTICAST_MAC3;
    dest_address_.sll_addr[4] = kMULTICAST_MAC4;
    dest_address_.sll_addr[5] = kMULTICAST_MAC5;
}

LldpSocket::~LldpSocket() {
    close(sock_fd_);
    free(buff_);
}

std::vector<uint8_t> LldpSocket::SetTarget(const std::string& target) {
    assert(ifaces_.find(target) != ifaces_.end()); // NOLINT
    struct ifreq if_idx = ifaces_.at(target);

    // Get target MAC address
    struct ifreq if_mac;
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, target.c_str(), target.length());
    if (ioctl(sock_fd_, SIOCGIFHWADDR, &if_mac) < 0) {
        perror("SIOCGIFHWADDR");
    }

    current_mac_[0] = ((uint8_t*) &if_mac.ifr_hwaddr.sa_data)[0];
    current_mac_[1] = ((uint8_t*) &if_mac.ifr_hwaddr.sa_data)[1];
    current_mac_[2] = ((uint8_t*) &if_mac.ifr_hwaddr.sa_data)[2];
    current_mac_[3] = ((uint8_t*) &if_mac.ifr_hwaddr.sa_data)[3];
    current_mac_[4] = ((uint8_t*) &if_mac.ifr_hwaddr.sa_data)[4];
    current_mac_[5] = ((uint8_t*) &if_mac.ifr_hwaddr.sa_data)[5];

    // Bind socket to target
    struct sockaddr_ll sll;
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_idx.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_LLDP);
    bind(sock_fd_, (struct sockaddr*) &sll, sizeof(sll));

    dest_address_.sll_ifindex = if_idx.ifr_ifindex;

    return current_mac_;
}

void LldpSocket::BuildEthernetHeader() {
    // Initialize packet
    struct ether_header* header = (struct ether_header *) buff_;
    memset(buff_, 0, kMAX_FRAME_SIZE);

    // Fill in source MAC address
    header->ether_shost[0] = current_mac_[0];
    header->ether_shost[1] = current_mac_[1];
    header->ether_shost[2] = current_mac_[2];
    header->ether_shost[3] = current_mac_[3];
    header->ether_shost[4] = current_mac_[4];
    header->ether_shost[5] = current_mac_[5];

    // Fill in dest MAC address
    header->ether_dhost[0] = kMULTICAST_MAC0;
    header->ether_dhost[1] = kMULTICAST_MAC1;
    header->ether_dhost[2] = kMULTICAST_MAC2;
    header->ether_dhost[3] = kMULTICAST_MAC3;
    header->ether_dhost[4] = kMULTICAST_MAC4;
    header->ether_dhost[5] = kMULTICAST_MAC5;

    // Set Ethertype to be LLDP
    header->ether_type = htons(ETH_P_LLDP);

    // Set packet size
    packet_size_ = sizeof(struct ether_header);
}

uint8_t* LldpSocket::GetPayloadBuffer() {
    BuildEthernetHeader();
    return (uint8_t*)buff_ + packet_size_;
}

void LldpSocket::SetPayloadSize(uint16_t size) {
    packet_size_ += size;
}

ssize_t LldpSocket::SendBuffer() {
    ssize_t sent;
    ssize_t to_send = packet_size_;

    while (to_send > 0) {
        sent = sendto(sock_fd_, buff_, packet_size_, 0, (struct sockaddr*) &dest_address_, sizeof(struct sockaddr_ll));
        if (sent < 0) {
            return -1;
        } else {
            to_send -= sent;
        }
    }

    return packet_size_;
}

uint8_t* LldpSocket::Receive(ssize_t& bytes) {
    bytes = read(sock_fd_, buff_, kMAX_FRAME_SIZE);
    if (bytes < 0) {
        perror("Read error");
    }
    return (uint8_t*)buff_ + sizeof(struct ether_header);
}

} // namespace lldp_kin
