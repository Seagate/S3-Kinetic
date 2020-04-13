#ifndef KINETIC_NET_PROCESSOR_H_
#define KINETIC_NET_PROCESSOR_H_

#include "glog/logging.h"

#include "popen_wrapper.h"
#include <map>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

struct NetworkPackets {
    unsigned int receive_packets;
    unsigned int receive_drop;
    unsigned int transmit_packets;
    unsigned int transmit_drop;
};

class NetProcessor : public LineProcessor {
    public:
    explicit NetProcessor(std::map<string, NetworkPackets>* interface_packet_information);
    virtual void ProcessLine(const string& line);
    virtual void ProcessPCloseResult(int pclose_result);
    virtual bool Success();

    private:
    std::map<string, NetworkPackets>* interface_packet_information_;
    int number_of_interfaces_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_NET_PROCESSOR_H_
