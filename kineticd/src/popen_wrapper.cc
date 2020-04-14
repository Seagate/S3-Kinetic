#include <stdlib.h>

#include "popen_wrapper.h"
#include <sstream>
#include "leveldb/status.h"

using namespace leveldb; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {


RawStringProcessor::RawStringProcessor(string* output, int* pclose_result)
    : output_(output), pclose_result_(pclose_result) {}

void RawStringProcessor::ProcessLine(const string& line) {
    *output_ += line;
}

void RawStringProcessor::ProcessPCloseResult(int pclose_result) {
    *pclose_result_ = pclose_result;
}

bool RawStringProcessor::Success() {
    return (*pclose_result_ == 0);
}

bool execute_command(const string command_line) {
    BlackholeLineProcessor blackhole_line_processor;
    return execute_command(command_line, blackhole_line_processor);
}

bool execute_command(const string command_line, LineProcessor& line_processor) {
    return execute_command(command_line, line_processor, true);
}

bool execute_command(const string command_line, LineProcessor& line_processor, bool display) {
    if (display) {
        VLOG(2) << "Executing <" << command_line << ">";
    }

    int nCount = 0;
    FILE* command_stream = popen(command_line.c_str(), "r");
    while (!command_stream && nCount < 3) {
        stringstream ss;
        ss << " EXECUTE CMD FAILED " << command_line << " strerror " << strerror(errno);
        Status::InvalidArgument(ss.str());
        sleep(1);
        ++nCount;
        command_stream = popen(command_line.c_str(), "r");
    }

    if (!command_stream) {
        if (display) {
            PLOG(ERROR) << "Unable to execute <" << command_line << ">";
        }
        return false;
    }

    char* line_buffer = NULL;
    size_t line_buffer_size;
    int bytes_read;
    while ((bytes_read = getline(&line_buffer, &line_buffer_size, command_stream)) != -1) {
        if (bytes_read > 0) {
            string line(line_buffer, bytes_read);
            if (display) {
                VLOG(2) << command_line << ": " << line;
            }
            line_processor.ProcessLine(line);
        }
    }

    if (line_buffer) {
        free(line_buffer);
    }

    int return_code = pclose(command_stream);
    if (WIFEXITED(return_code)) {
        return_code = WEXITSTATUS(return_code);
    } else {
        return_code = -1;
    }

    line_processor.ProcessPCloseResult(return_code);

    if (return_code) {
        if (display) {
            LOG(WARNING) << "Command <" << command_line << "> got non-zero return code " << return_code;
        }
    }

    return line_processor.Success();
}

} // namespace kinetic
} // namespace seagate
} // namespace com
