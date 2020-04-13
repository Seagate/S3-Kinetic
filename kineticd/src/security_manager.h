#ifndef KINETIC_SECURITY_MANAGER_H_
#define KINETIC_SECURITY_MANAGER_H_

#include <string>
#include "log_handler_interface.h"

namespace com {
namespace seagate {
namespace kinetic {

enum class PinIndex {
    ERASEPIN = 0,
    LOCKPIN = 1
};

enum class LockRequest {
    LOCKQUERY = 1,
    ENABLE,
    DISABLE,
    LOCK,
    UNLOCK,
    LOPC,
    NO_LOPC
};

enum class PinStatus {
    PIN_SUCCESS,
    AUTH_FAILURE,
    INTERNAL_ERROR
};

enum class BitStatus {
    LOCKED,
    UNLOCKED,
    INVALID,
    INTERNAL_ERROR
};

class SecurityInterface {
    public:
    virtual PinStatus Lock(std::string pin) = 0;
    virtual PinStatus Enable(std::string pin) = 0;
    virtual PinStatus Disable(std::string pin) = 0;
    virtual PinStatus Unlock(std::string pin) = 0;
    virtual BitStatus GetLockStatus() = 0;
    virtual PinStatus SetPin(std::string new_pin, std::string old_pin, PinIndex pin_index) = 0;
};

class SecurityManager : public SecurityInterface {
    public:
    static const int MAX_RETRIES;
    static const int SLEEP_TIME;
    SecurityManager();
    ~SecurityManager();
    void SetLogHandlerInterface(LogHandlerInterface* log_handler);
    PinStatus Erase(std::string pin);
    PinStatus Lock(std::string pin);
    PinStatus Enable(std::string pin);
    PinStatus Disable(std::string pin);
    PinStatus Unlock(std::string pin);
    BitStatus GetLockStatus();
    PinStatus SetPin(std::string new_pin, std::string old_pin, PinIndex pin_index);
    PinStatus EraseTCG(std::string pin);
    PinStatus SetPinTCG(std::string new_pin, std::string old_pin, PinIndex pin_index);
    PinStatus RetryManLockingCommand(std::string pin, LockRequest lock_request);

    private:
    PinStatus ManLockingCommand(std::string pin, LockRequest lock_request);
    BitStatus LockQueryCommand();
    BitStatus CheckStatusBits(int lock_status);
    LogHandlerInterface* log_handler_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SECURITY_MANAGER_H_
