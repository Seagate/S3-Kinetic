#ifndef KINETIC_PINOP_HANDLER_H_
#define KINETIC_PINOP_HANDLER_H_

#include <string.h>
#include "skinny_waist.h"
#include "kinetic.pb.h"
#include "security_manager.h"

namespace com {
namespace seagate {
namespace kinetic {

class Server;
class ConnectionHandler;
class Connection;

class PinOpHandler {
    public:
    explicit PinOpHandler(SkinnyWaistInterface& skinny_waist,
                          const string mountpoint,
                          const string partition,
                          bool remount_x86 = false);

    void ProcessRequest(const proto::Command &command,
                                      IncomingValueInterface* request_value,
                                      proto::Command *command_response,
                                      RequestContext& request_context,
                                      const proto::Message_PINauth& pin_auth,
                                      Connection* connection);
//                                      pthread_rwlock_t *ise_rw_lock);
    void SetServer(Server* server) {
        server_ = server;
    }

    void SetConnectionHandler(ConnectionHandler* connHandler) {
        connHandler_ = connHandler;
    }

    private:
    Server* server_;
    // Threadsafe; SkinnyWaistInterface implementations must be threadsafe
    SkinnyWaistInterface& skinny_waist_;
    const string mount_point_;
    const string partition_;
    bool remount_x86_;
    ConnectionHandler* connHandler_;


    bool EmptyPin(proto::Command *command_response, const proto::Message_PINauth& pin_auth);
    bool ValidPin(SecurityInterface& sed_manager,
      proto::Command *command_response,
      const proto::Message_PINauth& pin_auth,
      PinIndex pin_index);
    void DestroyAllBatchSets(Connection* connection);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_PINOP_HANDLER_H_
