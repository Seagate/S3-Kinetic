#ifndef KINETIC_CAUTIOUS_FILE_HANDLER_H_
#define KINETIC_CAUTIOUS_FILE_HANDLER_H_

#include <string>
#include <memory>

#include "kinetic/common.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::unique_ptr;

enum class FileReadResult {
    OK,
    CANT_OPEN_FILE,
    IO_ERROR
};

/**
* Handles reading and writing to a file where the file contents are
* first written to a tmp file and then copied over.
*
* Not threadsafe.
*/
class CautiousFileHandlerInterface {
    public:
    virtual ~CautiousFileHandlerInterface() {}

    // Write the data to a temp file and then move it to the final file name.
    virtual bool Write(string& data) = 0;
    // Read the data from the file into the provided string.
    virtual FileReadResult Read(string& data) = 0;
    // delete the file
    virtual bool Delete() = 0;
};

class CautiousFileHandler : public CautiousFileHandlerInterface {
    public:
    explicit CautiousFileHandler(const string& dir_path, const string& file_name);

    // Write the data to a temp file and then move it to the final file name.
    virtual bool Write(string& data);

    // Read the data from the file into the provided string.
    virtual FileReadResult Read(string& data);

    virtual bool Delete();

    private:
    bool WriteToTempFile(string& data);

    const std::string file_path_;
    const std::string tmp_file_path_;
    const std::string dir_path_;

    DISALLOW_COPY_AND_ASSIGN(CautiousFileHandler);
};

// no op impl
class BlackholeCautiousFileHandler : public CautiousFileHandlerInterface {
    public:
    explicit BlackholeCautiousFileHandler();
    virtual bool Write(string& data);
    virtual FileReadResult Read(string& data);
    virtual bool Delete();

    private:
    DISALLOW_COPY_AND_ASSIGN(BlackholeCautiousFileHandler);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_CAUTIOUS_FILE_HANDLER_H_
