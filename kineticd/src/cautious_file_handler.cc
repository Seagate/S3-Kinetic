#include "cautious_file_handler.h"

#include <cstdio>
#include <cstdint>
#include <sstream>
#include <fstream>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "glog/logging.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::stringstream;
using std::ifstream;
using std::ofstream;

CautiousFileHandler::CautiousFileHandler(const string& dir_path, const string& file_name)
    : file_path_(dir_path + "/" + file_name),
    tmp_file_path_(file_path_ + ".tmp"),
    dir_path_(dir_path) {}

bool CautiousFileHandler::Write(string& data) {
    if (!WriteToTempFile(data)) {
        return false;
    }

    if (rename(tmp_file_path_.c_str(), file_path_.c_str())) {
        PLOG(ERROR) << "Unable to rename " << tmp_file_path_ << " to " << file_path_;
        return false;
    }

    int directory_fd = open(dir_path_.c_str(), O_RDONLY);

    if (directory_fd == -1) {
        PLOG(ERROR) << "Unable to open dir <" << dir_path_ << ">";//NO_SPELL
        return false;
    }

    bool success = true;

    if (fsync(directory_fd)) {
        PLOG(ERROR) << "Unable to fsync dir <" << dir_path_ << ">";//NO_SPELL
        success = false;
    }

    if (close(directory_fd)) {
        PLOG(ERROR) << "Unable to close dir <" << dir_path_ << ">";//NO_SPELL
        success = false;
    }

    return success;
}

FileReadResult CautiousFileHandler::Read(string& data) {
    ifstream ifs(file_path_);

    if (!ifs.is_open()) {
        LOG(WARNING) << "Could not open " << file_path_;
        return FileReadResult::CANT_OPEN_FILE;
    }

    stringstream ss;

    if (!(ss << ifs.rdbuf())) {
        ifs.close();
        return FileReadResult::IO_ERROR;
    }
    ifs.close();

    data = ss.str();
    return FileReadResult::OK;
}

bool CautiousFileHandler::WriteToTempFile(string& data) {
    if (mkdir(dir_path_.c_str(), 0777) != 0) {
        if (errno != EEXIST) {
            PLOG(ERROR) << "Failed to create directory " << dir_path_;
            return false;
        }
    }

    FILE* file = fopen(tmp_file_path_.c_str(), "w");
    if (!file) {
        PLOG(ERROR) << "Unable to open " << tmp_file_path_;
        return false;
    }

    bool success = true;

    if (fwrite(data.c_str(), data.size(), 1, file) != 1) {
        PLOG(ERROR) << "Unable to write to " << tmp_file_path_;
        success = false;
        goto cleanup;
    }

    if (fflush(file)) {
        PLOG(ERROR) << "Unable to fflush " << tmp_file_path_;//NO_SPELL
        success = false;
        goto cleanup;
    }

    if (fsync(fileno(file))) {
        PLOG(ERROR) << "Unable to fsync " << tmp_file_path_;//NO_SPELL
        success = false;
        goto cleanup;
    }

    cleanup:
    if (fclose(file)) {
        PLOG(ERROR) << "Unable to close " << tmp_file_path_;
        success = false;
    }

    if (!success) {
        if (unlink(tmp_file_path_.c_str()) != 0) {
            PLOG(ERROR) << "Failed to unlink temporary file";
        }
    }

    return success;
}

bool CautiousFileHandler::Delete() {
    if (unlink(file_path_.c_str()) == -1 && errno != ENOENT) {
        PLOG(WARNING) << "Could not delete " << file_path_;
        return false;
    }

    return true;
}

bool BlackholeCautiousFileHandler::Write(string& data) {
    return true;
}

FileReadResult BlackholeCautiousFileHandler::Read(string& data) {
    return FileReadResult::CANT_OPEN_FILE;
}

BlackholeCautiousFileHandler::BlackholeCautiousFileHandler() {
}


bool BlackholeCautiousFileHandler::Delete() {
    return true;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
