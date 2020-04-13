#ifndef KINETIC_POWER_MANAGER_H_
#define KINETIC_POWER_MANAGER_H_

#include "skinny_waist.h"
#include "kinetic.pb.h"

namespace com {
namespace seagate {
namespace kinetic {

class Server;

class PowerManager {
 public:
    explicit PowerManager(SkinnyWaistInterface& skinny_waist, std::string device_path);
    void ProcessRequest(const proto::Command &command, proto::Command *command_response);
    proto::Command_PowerLevel GetPowerLevel() {
        return curr_power_level_;
    }
    void SetServer(Server* server) {
        server_ = server;
    }

 private:
    void SetOperational(proto::Command *command_response);
    void SetHibernate(proto::Command *command_response);
    void SetShutdown(proto::Command *command_response);
    int OpenDevice();
    bool CloseDatabase();
    void OpenDatabase();
    bool ATAGoIdle2(int fd);

    proto::Command_PowerLevel curr_power_level_;
    SkinnyWaistInterface& skinny_waist_;
    std::string device_path_;
    Server* server_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_POWER_MANAGER_H_
