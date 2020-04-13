#include "file_system_store.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <math.h>
#include "openssl/sha.h"

#include "glog/logging.h"

#include "kinetic/reader_writer.h"

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using ::kinetic::ReaderWriter;

size_t FileSystemStore::MAX_SUB_DIR = pow(2, 12);
size_t FileSystemStore::SUB_DIR_NAME_LEN = 3;

FileSystemStore::FileSystemStore(const std::string &directory): directory_(directory) {}

void FileSystemStore::UnlinkFile(const string& srcName) {
    if (unlink(srcName.c_str()) != 0) {
        PLOG(WARNING) << "Failed to unlink temporary file";
    }
}

bool FileSystemStore::Init(bool create_if_missing) {
    VLOG(1) << "Creating directory <" << directory_ << ">";

    struct stat dir_stat;
    if (!stat(directory_.c_str(), &dir_stat)) {
        VLOG(1) << "Directory already exists";
        return true;
    } else {
    // Create directory
        if (!create_if_missing) {
            LOG(ERROR) << "Directory not present and not created";
           return false;
        }

        string dir = directory_;
        if (dir[dir.size() - 1] != '/') {
            dir += '/';
        }

        size_t start = 0;
        size_t end = 0;
        while ((end = dir.find_first_of('/', start)) != string::npos) {
            string path_component = dir.substr(0, end++);
            start = end;

            if (path_component.empty()) {
                continue;
            }

            if (mkdir(path_component.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) && errno != EEXIST) {
                PLOG(ERROR) << "Unable to create path component <" << path_component << ">";
                return false;
            }
        }
    }

    // Create subdirectories
    size_t nDir = 0;
    std::string zero("0");
    do {
        std::stringstream stream;
        stream << std::hex << nDir;
        std::string subdir(stream.str());
        while (subdir.size() < SUB_DIR_NAME_LEN) {
            subdir = zero + subdir;
        }
        std::string dir = directory_ + "/" + subdir;
        if (mkdir(dir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) && errno != EEXIST) {
            PLOG(ERROR) << "Unable to create path component <" << dir << ">";
            return false;
        }
    } while (++nDir < MAX_SUB_DIR);
    return true;
}

bool FileSystemStore::Get(const std::string &key, NullableOutgoingValue *value) {
    std::string name;
    FileName(key, &name);
    int fd = open(name.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }
    value->set_value(new OutgoingFileValue(fd));
    return true;
}

////////////////////////////////////////////////////////
//MediaScan Centric Function for accessing File Store
//Based Values Multiple Times
//Author: James DeVore
//-----------------------
//Initializes ms_file_value_ member variable to OutgoingFileValue.
//Allows access to the File Descriptor in order to close the FD every iteration.
//The Standard Get() function (above); was intended for a single Value Access,
//and rely's on the Outgoing Value destructor call to close the files.
//Since a Scan will not trigger these destructors every iteration, it would have
//left multiple files Open.
bool FileSystemStore::MScanGet(const std::string &key, NullableOutgoingValue *value) {
    std::string name;
    FileName(key, &name);
    int fd = open(name.c_str(), O_RDONLY);
#ifdef KDEBUG
    DLOG(INFO) << "FileSystemStore MScanGet() key: " << key;//NO_SPELL
    DLOG(INFO) << "FileSystemStore MScanGet() File Name: " << name.c_str();//NO_SPELL
#endif
    if (fd == -1) {
#ifdef KDEBUG
        DLOG(INFO) << "FileSystemStore MScanGet() Failed to open File";//NO_SPELL
#endif
        return false;
    }
    ms_file_value_ = new OutgoingFileValue(fd);
    value->set_value(ms_file_value_);
    return true;
}

// This helper method is like fsync() for the directory containing the given file
// path. It's needed because rename() is atomic but not durable. To make sure a
// creation or deletion gets persisted we need to call fsync() on the containing
// directory's fd.
bool FileSystemStore::SyncContainingDir(const std::string& file) {
    char* buffer = strdup(file.c_str());
    char* containing_dir = dirname(buffer);
    int directory_fd = open(containing_dir, O_RDONLY);

    free(buffer);

    if (directory_fd == -1) {
        PLOG(ERROR) << "Unable to open containing_dir <" << file << ">";//NO_SPELL
        return false;
    }

    bool success = true;

    if (fsync(directory_fd)) {
        PLOG(ERROR) << "Unable to fsync containing_dir <" << file << ">";//NO_SPELL
        success = false;
    }

    if (close(directory_fd)) {
        PLOG(ERROR) << "Unable to close containing_dir <" << file << ">";//NO_SPELL
        success = false;
    }

    return success;
}

bool FileSystemStore::Put(const string& key, IncomingValueInterface* value,
                          bool guarantee_durable) {
    std::string final_name;
    string subDir;
    FileName(key, &final_name, &subDir);

    // We write first to a temporary file so that if it should fail, the old
    // value remains intact.
    std::string tmp_name;
    int tmp_fd;
    if (!MakeTemporaryFile(&tmp_name, &tmp_fd, &subDir, guarantee_durable)) {
        return false;
    }
    bool success = value->TransferToFile(tmp_fd);

    if (guarantee_durable) {
        if (fsync(tmp_fd) != 0) {
            PLOG(ERROR) << "Unable to fsync temporary file";//NO_SPELL
            success = false;
        }
    }

    if (close(tmp_fd) != 0) {
        PLOG(ERROR) << "Failed to close temporary file";
        success = false;
    }

    if (success) {
        if (rename(tmp_name.c_str(), final_name.c_str()) != 0) {
            PLOG(ERROR) << "Failed to rename temporary file";
            success = false;
        }

        if (guarantee_durable) {
            if (!SyncContainingDir(final_name)) {
                LOG(ERROR) << "Unable to sync dir of <" << final_name << ">";//NO_SPELL
                success = false;
            }
        }
    }

    if (!success) {
        if (unlink(tmp_name.c_str()) != 0) {
            PLOG(ERROR) << "Failed to unlink temporary file";
        }
    }

    return success;
}
bool FileSystemStore::TemporarilyPut(const string& key, IncomingValueInterface* value,
        bool guarantee_durable, string& tmp_name) {
    std::string final_name;
    string subDir;
    FileName(key, &final_name, &subDir);

    // We write first to a temporary file so that if it should fail, the old
    // value remains intact.
    int tmp_fd;
    tmp_name.clear();
    if (!MakeTemporaryFile(&tmp_name, &tmp_fd, &subDir, guarantee_durable)) {
        return false;
    }
    bool success = value->TransferToFile(tmp_fd);

    if (close(tmp_fd) != 0) {
        PLOG(ERROR) << "Failed to close temporary file";
        success = false;
    }
    return success;
}

bool FileSystemStore::RenameFile(const string& srcName, const string& key) {
    string destName;
    string subDir;
    FileName(key, &destName, &subDir);
    if (rename(srcName.c_str(), destName.c_str()) != 0) {
        PLOG(ERROR) << "Failed to rename " << srcName << " to " << destName;
        return false;
    }
    return true;
}

bool FileSystemStore::Delete(const std::string &key) {
    std::string name;
    FileName(key, &name);
    int status = unlink(name.c_str());
    return status == 0;
}

void FileSystemStore::FileName(const std::string &key, std::string *result, std::string* subDir) {
    *result = directory_;
    result->append("/");
    unsigned char hash[SHA_DIGEST_LENGTH];
    Sha1(key, hash);
    char *hex_hash = Hex(hash, SHA_DIGEST_LENGTH);
    std::string tmpStr;
    if (!subDir) {
        subDir = &tmpStr;
    }
    subDir->assign(hex_hash, SUB_DIR_NAME_LEN);
    *result += *subDir + "/";
    result->append(hex_hash, 2 * SHA_DIGEST_LENGTH);
    delete[] hex_hash;
}

bool FileSystemStore::MakeTemporaryFile(std::string *name, int *fd, const std::string* subDir,
                                        bool guarantee_durable) {
    std::string tmp_format(directory_);
    if (subDir && !subDir->empty()) {
        tmp_format.append("/");
        tmp_format.append(*subDir);
    }
    tmp_format.append("/XXXXXX");

    char *tmp_file_name = new char[tmp_format.size() + 1];
    memcpy(tmp_file_name, tmp_format.c_str(), tmp_format.size() + 1);
    int tmp_fd = -1;
    if (guarantee_durable) {
        tmp_fd = mkostemp(tmp_file_name, O_RDWR | O_SYNC);
    } else {
        tmp_fd = mkstemp(tmp_file_name);
    }
    if (tmp_fd == -1) {
        PLOG(ERROR) << "Failed to create a temporary file";
    } else {
        *fd = tmp_fd;
        *name = tmp_file_name;
    }

    delete[] tmp_file_name;
    return tmp_fd != -1;
}

void FileSystemStore::Sha1(const std::string &key, unsigned char *hash) {
    CHECK_EQ(hash, SHA1(reinterpret_cast<const unsigned char *>(key.data()), key.size(), hash));
}

char *FileSystemStore::Hex(const unsigned char *in, size_t n) {
    const char *lookup_table = "0123456789abcdef";
    char *result = new char[n * 2];
    for (size_t i = 0; i < n; ++i) {
        result[2 * i] = lookup_table[(in[i] >> 4) & 0xf];
        result[2 * i + 1] = lookup_table[in[i] & 0xf];
    }
    return result;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
