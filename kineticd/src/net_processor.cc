#include <stdlib.h>

#include "net_processor.h"
#include <inttypes.h>
// #include "popen_wrapper.h"
// #include <utility>
// #include <functional>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

NetProcessor::NetProcessor(std::map<string, NetworkPackets>* interface_packet_information)
    : interface_packet_information_(interface_packet_information) {
        number_of_interfaces_ = 0;
    }

void NetProcessor::ProcessLine(const string& line) {
    char interface[50];
    unsigned int receive_packets, receive_drop, transmit_packets, transmit_drop;//NOLINT
    if (sscanf(line.c_str(), "%s %*d %u %*d %u %*d %*d %*d %*d %*d %u %*d %u", interface,
        &receive_packets, &receive_drop, &transmit_packets, &transmit_drop) == 5) {
        VLOG(2) << "Network Stats: " << interface
                << " receive_packets=" << receive_packets//NO_SPELL
                << " receive_drop=" << receive_drop//NO_SPELL
                << " transmit_packets=" << transmit_packets//NO_SPELL
                << " transmit_drop=" << transmit_drop;//NO_SPELL

        number_of_interfaces_++;
        struct NetworkPackets network_packets_;
        network_packets_.receive_packets = receive_packets;
        network_packets_.receive_drop = receive_drop;
        network_packets_.transmit_packets = transmit_packets;
        network_packets_.transmit_drop = transmit_drop;
        string key = interface;
        (*interface_packet_information_)[key] = network_packets_;
    }
}

void NetProcessor::ProcessPCloseResult(int pclose_result) {}

bool NetProcessor::Success() {
    return true;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
