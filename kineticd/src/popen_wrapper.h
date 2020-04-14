#ifndef KINETIC_POPEN_WRAPPER_H_
#define KINETIC_POPEN_WRAPPER_H_

#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;

class LineProcessor {
    public:
    virtual void ProcessLine(const string& line) = 0;
    virtual void ProcessPCloseResult(int pclose_result) = 0;
    virtual bool Success() = 0;
};

class BlackholeLineProcessor : public LineProcessor {
    public:
    virtual void ProcessLine(const string& line) {}
    virtual void ProcessPCloseResult(int pclose_result) {}
    virtual bool Success() { return true; }
};

// Generic processor that logs all returned data into output and the response code
// into pclose_result
class RawStringProcessor : public LineProcessor {
    public:
    explicit RawStringProcessor(string* output, int* pclose_result);
    virtual void ProcessLine(const string& line);
    virtual void ProcessPCloseResult(int pclose_result);
    virtual bool Success();

    private:
    string* output_;
    int* pclose_result_;
};

bool execute_command(const string command_line);
bool execute_command(const string command_line, LineProcessor& line_processor);
bool execute_command(const string command_line, LineProcessor& line_processor, bool display);

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_POPEN_WRAPPER_H_
