#include <string>
#include "lldp_listener.h"
#include "popen_wrapper.h"

using ::lldp_kin::LldpListener;

void reboot() {
    // Kill kinetic and reboot
    com::seagate::kinetic::execute_command("killkv");
    com::seagate::kinetic::execute_command("reboot");
}

int main(int argc, char* argv[]) {
    std::string interface;

    if (argc <= 1) {
        return -1;
    } else {
        interface = argv[1];
    }

    LldpListener listener(interface);
    if (!listener.ListenForConfig()) {
        return -1;
    } else {
        reboot();
    }
}
