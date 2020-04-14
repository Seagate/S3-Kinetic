#ifndef KINETIC_P2P_PUSH_CALLBACK_H_
#define KINETIC_P2P_PUSH_CALLBACK_H_

#include "kinetic/kinetic.h"
#include "kinetic/common.h"
#include "kinetic.pb.h"

#include <sstream>
#include <vector>

namespace com {
namespace seagate {
namespace kinetic {

using std::stringstream;
using std::unique_ptr;
using std::vector;

using ::kinetic::KineticStatus;
using proto::Command_Status;
using proto::Command_Status_StatusCode;
using proto::Command_Status_StatusCode_IsValid;
using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;
using proto::Command_P2POperation;
using proto::Command_P2POperation_Operation;

class P2PPushCallback : public ::kinetic::P2PPushCallbackInterface {
    public:
    P2PPushCallback(int* outstanding_pushes,
            proto::Command_Status* status,
            int* successful_operation_count,
            size_t* heuristic_cache_size,
            size_t heuristic_operation_size,
            Command_P2POperation* p2pop);

    virtual void Success(unique_ptr<vector<KineticStatus>> operation_statuses,
            const com::seagate::kinetic::client::proto::Command& response);

    virtual void Failure(KineticStatus error,
            com::seagate::kinetic::client::proto::Command const * const response);

    private:
    void ConvertClientP2PStatus(
        com::seagate::kinetic::client::proto::Command_Status client_status,
        Command_Status* result_status);

    void ConvertClientP2POperationResponse(
        com::seagate::kinetic::client::proto::Command_P2POperation client_p2pop,
        Command_P2POperation* result_p2pop);

    int* outstanding_pushes_;
    proto::Command_Status* status_;
    int* successful_operation_count_;
    size_t* heuristic_cache_size_;
    size_t heuristic_operation_size_;
    Command_P2POperation* p2pop_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_P2P_PUSH_CALLBACK_H_
