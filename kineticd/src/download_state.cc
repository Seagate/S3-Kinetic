#include "download_state.h"

#include <sys/wait.h>

#include "down_state.h"
#include "connection_queue.h"
#include "popen_wrapper.h"
#include "server.h"

namespace com {
namespace seagate {
namespace kinetic {

KineticState* DownloadState::GetNextState(StateEvent event, bool success) {
    KineticState* nextState = NULL;
    switch (event) {
        case StateEvent::DOWNLOADED:
            server_->CloseDB();
            if (ExecuteFirmware()) {
               nextState = new DownState(server_);
            } else {
               nextState = prevState_;
               prevState_ = NULL;
               server_->OpenDB();
            }
            break;
        default:
            break;
    }
    return nextState;
}

bool DownloadState::ExecuteFirmware() {
    bool exeStatus = false;
    pid_t cpid = fork();
    if (cpid == -1) {
        PLOG(ERROR) << "Could not fork to run update";
        return false;
    } else if (cpid == 0) {
        int exitStatus = EXIT_SUCCESS;
        std::queue<std::string>* download_queue = static_cast<std::queue<std::string>*>(data_);
        int status = 0;
        std::string command_result;
        std::string command_base("/bin/sh ");
        RawStringProcessor processor(&command_result, &status);

        while (!download_queue->empty()) {
            std::string filename = download_queue->front();
            std::string command = command_base + filename;
            download_queue->pop();
            VLOG(1) << "Executing installer file: " << filename;

            if (!execute_command(command, processor)) {
                LOG(ERROR) << "Installer output:\n" << command_result;
                LOG(ERROR) << "Failed to run update command: " << command;
                exitStatus = EXIT_FAILURE;
                break;
            }
        }

        if (status == 0) {
            VLOG(1) << "Successfully executed firmware update";
            if (!execute_command("reboot", processor)) {
                LOG(ERROR) << "Failed to execute reboot";
                exitStatus = EXIT_FAILURE;
            }
       }

       exit(exitStatus);
    } else {
        VLOG(1) << "Forked child to run firmware update with pid " << cpid;//NO_SPELL
        pid_t w = -1;
        int status = -1;
        do {
            w = waitpid(cpid, &status, WUNTRACED | WCONTINUED);
            if (w == -1) {
                PLOG(ERROR) << "Failed to wait child process " << cpid;
                return false;
            }
            exeStatus = (status == 0);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return exeStatus;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
