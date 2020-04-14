#include "key_value_store_interface.h"

using com::seagate::kinetic::KeyValueStoreInterface;

// This implementation is suitable for derived classes that do not need to
// perform any initialization that can fail.
bool KeyValueStoreInterface::Init(bool create_if_missing) {
    return true;
}

void KeyValueStoreInterface::Close() {
}

bool KeyValueStoreInterface::GetDBProperty(std::string property, std::string* value) {
    return true;
}

void KeyValueStoreInterface::SetLogHandlerInterface(LogHandlerInterface* log_handler) {
}

void KeyValueStoreInterface::SetListOwnerReference(
        SendPendingStatusInterface* send_pending_status_sender) {
}

KeyValueStoreInterface::~KeyValueStoreInterface() {}
