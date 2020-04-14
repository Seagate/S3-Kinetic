#include "log_ring_buffer.h"
// #include "pthreads_mutex_guard.h"
#include <iostream>
#include <typeinfo>
#include <sstream>

#include "command_line_flags.h"

using namespace com::seagate::kinetic::proto; // NOLINT

namespace com {
namespace seagate {
namespace kinetic {

const int LogRingBuffer::NUMBER_OF_LOG_FILES = 5;
static int STALE_BUFFER_SIZE = 10;

LogRingBuffer* LogRingBuffer::log_ring_buffer_instance_ = NULL;

LogRingBuffer* LogRingBuffer::Instance() {
    if (!log_ring_buffer_instance_) { // only allow one instance of the class to be generated
        log_ring_buffer_instance_ = new LogRingBuffer(1024);
    }

    return log_ring_buffer_instance_;
}

LogRingBuffer::LogRingBuffer(uint32_t capacity): capacity_(capacity) {
    buf_.resize(capacity_);
    command_buf_.resize(capacity_);
    key_value_histo_buf_.resize(capacity_);
    key_size_histo_buf_.resize(capacity_);
    value_size_histo_buf_.resize(capacity_);
    stale_buf_.resize(STALE_BUFFER_SIZE);
    size_ = 0;
    start_ = 0;
    command_size_ = 0;
    command_start_ = 0;
    stale_size_ = 0;
    stale_start_ = 0;
    num_key_value_ = 0;
    num_key_size_ = 0;
    num_value_size_ = 0;
    num_stale_entry_ = 0;
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&mutex_, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

//    CHECK(!pthread_mutex_init(&(mutex_), NULL));
}

LogRingBuffer::~LogRingBuffer() {
    CHECK(!pthread_mutex_destroy(&mutex_));
    LogRingBuffer::log_ring_buffer_instance_ = NULL;
}

uint32_t LogRingBuffer::capacity() {
    return capacity_;
}

uint32_t LogRingBuffer::size() {
    return size_;
}

uint32_t LogRingBuffer::command_size() {
    return command_size_;
}

uint32_t LogRingBuffer::num_key_value() {
    return num_key_value_;
}

uint32_t LogRingBuffer::num_key_size() {
    return num_key_size_;
}

uint32_t LogRingBuffer::num_value_size() {
    return num_value_size_;
}

uint32_t LogRingBuffer::num_stale_entry() {
    return num_stale_entry_;
}

std::vector<LogRingCommandEntry> LogRingBuffer::command_buffer() {
    return command_buf_;
}

std::vector<LogRingKeyValueEntry> LogRingBuffer::key_value_histo_buffer() {
    return key_value_histo_buf_;
}

std::vector<LogRingKeySizeEntry> LogRingBuffer::key_size_histo_buffer() {
    return key_size_histo_buf_;
}

std::vector<LogRingValueSizeEntry> LogRingBuffer::value_size_histo_buffer() {
    return value_size_histo_buf_;
}

std::vector<LogRingStaleEntry> LogRingBuffer::stale_buffer() {
    return stale_buf_;
}

uint32_t LogRingBuffer::command_start() {
    return command_start_;
}

void LogRingBuffer::push(::google::LogSeverity severity, const char *full_filename,
    const char *base_filename, int line, const struct ::tm *tm_time, const char *message,
    size_t message_len, pid_t thread_id) {
    PthreadsMutexGuard guard(&mutex_);
    uint32_t index = start_ + size_;

    if (index >= capacity_) {
        index -= capacity_;
    }

    if (size_ == capacity_) {
        start_++;
        if (start_ == capacity_) {
            start_ = 0;
        }
    } else {
        size_++;
    }
    assert(index < buf_.size()); // NOLINT
    LogRingBufferEntry &entry = buf_.at(index);

    entry.severity = severity;

    // we're given char* by glog; re-use std::string to avoid allocation
    entry.full_filename.clear();
    entry.full_filename.append(full_filename);

    entry.base_filename.clear();
    entry.base_filename.append(base_filename);

    entry.line = line;

    entry.tm_time = *tm_time;

    entry.message.clear();
    entry.message.append(message, message_len);

    entry.thread_id = thread_id;
}

void LogRingBuffer::logCommand(std::time_t time_enqueued, std::time_t time_dequeued,
        int time_responded, Command_MessageType message_type, std::string success) {
    PthreadsMutexGuard guard(&mutex_);
    uint32_t index = command_start_ + command_size_;

    if (index >= capacity_) {
        index -= capacity_;
    }

    if (command_size_ == capacity_) {
        command_start_++;
        if (command_start_ == capacity_) {
            command_start_ = 0;
        }
    } else {
        command_size_++;
    }
    assert(index < command_buf_.size()); // NOLINT
    LogRingCommandEntry &entry = command_buf_.at(index);


    entry.time_enqueued = time_enqueued;
    entry.time_dequeued = time_dequeued;
    entry.time_responded = time_responded;

    std::string message;

    getCommandMessage(message, message_type);
    entry.message.clear();
    entry.message.append(message);

    entry.success.clear();
    entry.success.append(success);
}

void LogRingBuffer::logKeyValueHisto(int key_size, int value_size) {
    PthreadsMutexGuard guard(&mutex_);
    int new_entry = key_value_histo_map_[std::make_pair(key_size, value_size)] += 1;
    // if new_entry is equal to one then it's a unique entry, and
    // key_value_max to keep console ouput resonable.
    if (new_entry == 1) {
        num_key_value_++;
    }

    if (num_key_value_ == key_value_histo_buf_.size()) {
        if (key_value_histo_map_.begin() ==
            key_value_histo_map_.find(std::make_pair(key_size, value_size))) {
            key_value_histo_map_.erase(next(key_value_histo_map_.begin(), 1));
        } else {
            key_value_histo_map_.erase(key_value_histo_map_.begin());
        }
        num_key_value_--;
    }
    int index = 0;

    for (auto it : key_value_histo_map_) {
        assert(index >= 0 && index < (int)key_value_histo_buf_.size()); // NOLINT
        LogRingKeyValueEntry &entry = key_value_histo_buf_.at(index);
        entry.key_size = (it.first).first;
        entry.value_size = (it.first).second;
        entry.frequency = it.second;
        index++;
    }
}

void LogRingBuffer::logTransferLength(int key_size, int value_size) {
    PthreadsMutexGuard guard(&mutex_);
    int new_entry_key_size = key_size_map_[key_size] += 1;
    int new_entry_value_size = value_size_map_[value_size] += 1;

    if (new_entry_key_size == 1) {
        num_key_size_++;
    }

    if (new_entry_value_size == 1) {
        num_value_size_++;
    }

    if (num_key_size_ == key_size_histo_buf_.size()) {
        if (key_size_map_.begin() == key_size_map_.find(key_size)) {
            key_size_map_.erase(next(key_size_map_.begin(), 1));
        } else {
            key_size_map_.erase(key_size_map_.begin());
        }
        num_key_size_--;
    }

    if (num_value_size_ == value_size_histo_buf_.size()) {
        if (value_size_map_.begin() == value_size_map_.find(value_size)) {
            value_size_map_.erase(next(value_size_map_.begin(), 1));
        } else {
            value_size_map_.erase(value_size_map_.begin());
        }
        num_value_size_--;
    }

    int index = 0;
    for (auto it : key_size_map_) {
        assert(index >= 0 && index < (int)key_size_histo_buf_.size()); // NOLINT
        LogRingKeySizeEntry &key_entry = key_size_histo_buf_.at(index);
        key_entry.key_size = it.first;
        key_entry.frequency = it.second;
        index++;
    }

    index = 0;
    for (auto it : value_size_map_) {
        assert(index >= 0 && index < (int)value_size_histo_buf_.size()); // NOLINT
        LogRingValueSizeEntry &value_entry = value_size_histo_buf_.at(index);
        value_entry.key_size = it.first;
        value_entry.frequency = it.second;
        index++;
    }
}

void LogRingBuffer::makePersistent() {
    PthreadsMutexGuard guard(&mutex_);
    //Write contents of both buffers to util partition
    writeBufferToDisk(buf_, size_, start_, log_file_path_, old_log_file_path_);
    writeBufferToDisk(command_buf_, command_size_, command_start_,
        command_log_file_path_, command_old_log_file_path_);
}

void LogRingBuffer::makeHistoPersistent() {
    PthreadsMutexGuard guard(&mutex_);

    if (key_value_histo_map_.size() >= 1) {
        writeBufferToDisk(key_value_histo_buf_, num_key_value_, 0,
        key_value_log_file_path_, old_key_value_log_file_path_);
        num_key_value_ = 0;
        key_value_histo_map_.clear();
    }

    if (key_size_map_.size() >= 1) {
        writeBufferToDisk(key_size_histo_buf_, num_key_size_, 0,
        key_size_log_file_path_, old_key_size_log_file_path_);
        num_key_size_ = 0;
        key_size_map_.clear();
        key_size_histo_buf_.clear();
        key_size_histo_buf_.resize(capacity_);
    }

    if (value_size_map_.size() >= 1) {
        writeBufferToDisk(value_size_histo_buf_, num_value_size_, 0,
        value_size_log_file_path_, old_value_size_log_file_path_);
        num_value_size_ = 0;
        value_size_map_.clear();
        value_size_histo_buf_.clear();
        value_size_histo_buf_.resize(capacity_);
    }
}

void LogRingBuffer::makeStaleEntryDataPersistent() {
    //Check to see if there was any data recorded otherwise don't
    //write to disk.
    PthreadsMutexGuard guard(&mutex_);
    if (stale_entry_map_.size() >= 1) {
        writeBufferToDisk(stale_buf_, num_stale_entry_, 0,
            stale_data_log_file_path_, old_stale_data_log_file_path_);

        num_stale_entry_ = 0;
        stale_buf_.clear();
        stale_buf_.resize(STALE_BUFFER_SIZE);
        stale_entry_map_.clear();
    }
}

void LogRingBuffer::recordStaleDataToBuffer(int level) {
    PthreadsMutexGuard guard(&mutex_);
    assert(level >= 0 && level < STALE_BUFFER_SIZE); // NOLINT
    int index = 0;
    if (stale_entry_map_.find(level) == stale_entry_map_.end()) {
        stale_entry_map_[level] = 0;
    }
    int new_entry = (stale_entry_map_[level] += 1);
    // if new_entry is equal to one then it's a unique entry, and
    // key_value_max to keep console ouput resonable.
    if (new_entry == 1) {
        num_stale_entry_++;
    }
    assert(int(stale_entry_map_.size()) <= STALE_BUFFER_SIZE); // NOLINT
    for (auto it : stale_entry_map_) {
        assert(index >= 0 && index < (int)stale_buf_.size()); // NOLINT
        LogRingStaleEntry &entry = stale_buf_.at(index);
        entry.level = it.first;
        entry.frequency = it.second;
        index++;
    }
}

void LogRingBuffer::renameFile(std::string file_name, std::string old_file_name, int file_number) {
    if (file_number != NUMBER_OF_LOG_FILES) {
        std::string file_command;
        file_command += "test -f ";
        file_command += file_name;
        // File exists we need to nemame old file
        if (!system(file_command.c_str())) {
            std::string rename_command;
            std::string new_file_name = old_file_name;
            new_file_name += std::to_string(file_number);
            new_file_name += ".txt";

            renameFile(new_file_name, old_file_name, file_number + 1);
            rename_command += "mv ";
            rename_command += file_name;
            rename_command += " ";
            rename_command += old_file_name;
            rename_command += std::to_string(file_number);
            rename_command += ".txt";
            system(rename_command.c_str());
        }
    }
}


void LogRingBuffer::parseAndSetStatusCodes(std::string status_codes) {
    std::string delimiter = ",";
    size_t pos = 0;
    size_t start = 0;

    // Parse through status_codes string using ',' as a delimiter
    while ((pos = status_codes.find(delimiter)) != std::string::npos) {
        // Get status code
        int code = atoi(status_codes.substr(start, pos).c_str());
        // Cast status code and insert to set
        proto::Command_Status_StatusCode sCode = (proto::Command_Status_StatusCode)code;
        status_codes_.insert(sCode);
        status_codes = status_codes.substr(pos+1, status_codes.length());
    }
    // Need to insert last status code in the string
    proto::Command_Status_StatusCode sCode =
        (proto::Command_Status_StatusCode)atoi(status_codes.c_str());
    status_codes_.insert(sCode);
}

void LogRingBuffer::getCommandMessage(std::string &message_out, Command_MessageType message_type) {
    // std::string message;

    switch (message_type) {
        case Command_MessageType_GETKEYRANGE:
            message_out = "GETKEYRANGE";
            break;
         case Command_MessageType_GET:
            message_out = "GET";
            break;
        case Command_MessageType_GETVERSION:
            message_out = "GETVERSION";
            break;
        case Command_MessageType_GETNEXT:
            message_out = "GETNEXT";
            break;
        case Command_MessageType_GETPREVIOUS:
            message_out = "GETPREVIOUS";
            break;
        case Command_MessageType_PUT:
            message_out = "PUT";
            break;
        case Command_MessageType_DELETE:
            message_out = "DELETE";
            break;
        case Command_MessageType_FLUSHALLDATA:
            message_out = "FLUSHALLDATA";
            break;
        case Command_MessageType_SETUP:
            message_out = "SETUP";
            break;
        case Command_MessageType_SECURITY:
            message_out = "SECURITY";
            break;
        case Command_MessageType_GETLOG:
            message_out = "GETLOG";
            break;
       case Command_MessageType_PEER2PEERPUSH:
           message_out = "PEER2PEERPUSH";
           break;
        case Command_MessageType_NOOP:
            message_out = "NOOP";
            break;
        case Command_MessageType_MEDIASCAN:
            message_out = "MEDIASCAN";
            break;
        case Command_MessageType_PINOP:
            message_out = "PINOP";
            break;
        case Command_MessageType_START_BATCH:
            message_out = "START_BATCH";
            break;
        case Command_MessageType_END_BATCH:
            message_out = "END_BATCH";
            break;
        case Command_MessageType_ABORT_BATCH:
            message_out = "ABORT_BATCH";
            break;
        default:
            message_out = "UNIMPLEMENTED";
            break;
    }
}

void LogRingBuffer::setLogFilePaths(std::string log_file_path, std::string old_log_file_path,
    std::string command_log_file_path, std::string command_old_log_file_path,
    std::string key_value_log_file_path, std::string old_key_value_log_file_path,
    std::string key_size_log_file_path, std::string old_key_size_log_file_path,
    std::string value_size_log_file_path, std::string old_value_size_log_file_path,
    std::string stale_data_log_file_path, std::string old_stale_data_log_file_path) {

    log_file_path_ = log_file_path;
    old_log_file_path_ = old_log_file_path;
    command_log_file_path_ = command_log_file_path;
    command_old_log_file_path_ = command_old_log_file_path;
    key_value_log_file_path_ = key_value_log_file_path;
    old_key_value_log_file_path_ = old_key_value_log_file_path;
    key_size_log_file_path_ = key_size_log_file_path;
    old_key_size_log_file_path_ = old_key_size_log_file_path;
    value_size_log_file_path_ = value_size_log_file_path;
    old_value_size_log_file_path_ = old_value_size_log_file_path;
    stale_data_log_file_path_ = stale_data_log_file_path;
    old_stale_data_log_file_path_ = old_stale_data_log_file_path;
}

bool LogRingBuffer::checkTrigger(proto::Command_Status_StatusCode message_status) {
    if (status_codes_.find(message_status) != status_codes_.end()) {
        makePersistent();
        return true;
    }
    return false;
}

void LogRingBuffer::changeCapacity(uint32_t capacity) {
    capacity_ = capacity;
    buf_.resize(capacity_);
}

void LogRingBuffer::clearBuffer() {
    start_ = 0;
    size_ = 0;
}
void LogRingBuffer::copyCommandBuffer(std::vector<LogRingCommandEntry> &dest) {
    PthreadsMutexGuard guard(&mutex_);
    dest.reserve(command_size_);
    size_t index;
    for (uint32_t i = 0; i < command_size_; i++) {
        index = command_start_ + i;
        if (index >= capacity_) {
            index -= capacity_;
        }
        assert(index < this->command_buf_.size()); // NOLINT
        dest.push_back(this->command_buf_.at(index));
    }
}
void LogRingBuffer::copyKeyValueBuffer(std::vector<LogRingKeyValueEntry> &dest) {
    PthreadsMutexGuard guard(&mutex_);
    dest.reserve(this->num_key_value_);
    size_t index;
    for (uint32_t i = 0; i < this->num_key_value_; i++) {
        index = i;
        if (index >= capacity_) {
            index -= capacity_;
        }
        assert(index < this->key_value_histo_buf_.size()); //NOLINT
        dest.push_back(this->key_value_histo_buf_.at(index));
    }
}

void LogRingBuffer::copyKeySizeBuffer(std::vector<LogRingKeySizeEntry> &dest) {
    PthreadsMutexGuard guard(&mutex_);
    dest.reserve(this->num_key_size_);
    size_t index;
    for (uint32_t i = 0; i < num_key_size_; i++) {
        index = i;
        if (index >= capacity_) {
            index -= capacity_;
        }
        assert(index < this->key_size_histo_buf_.size()); // NOLINT
        dest.push_back(this->key_size_histo_buf_.at(index));
    }
}

void LogRingBuffer::copyValueSizeBuffer(std::vector<LogRingValueSizeEntry> &dest) {
    PthreadsMutexGuard guard(&mutex_);
    dest.reserve(this->num_value_size_);
    size_t index;
    for (uint32_t i = 0; i < num_value_size_; i++) {
        index = i;
        if (index >= capacity_) {
            index -= capacity_;
        }
        assert(index < this->value_size_histo_buf_.size()); // NOLINT
        dest.push_back(this->value_size_histo_buf_.at(index));
    }
}

void LogRingBuffer::copyStaleBuffer(std::vector<LogRingStaleEntry> &dest) {
    PthreadsMutexGuard guard(&mutex_);
    dest.reserve(this->stale_size_);
    size_t index;
    for (uint32_t i = 0; i < stale_size_; i++) {
        index = i;
        if (index >= capacity_) {
            index -= capacity_;
        }
        assert(index < this->stale_buf_.size()); // NOLINT
        dest.push_back(this->stale_buf_.at(index));
    }
}

void LogRingBuffer::toString(std::string& dest) {
    PthreadsMutexGuard guard(&mutex_);
    const int MAX_LOG_SIZE = 0.9*FLAGS_max_message_size_bytes;
    std::stringstream ss[2];
    int ssSize[2];
    ssSize[0] = ssSize[1] = 0;
    int nCurStream = 0;
    size_t index;
    // Iterate through buf_
    for (uint32_t i = 0; i < size_; i++) {
        index = start_ + i;
        if (index >= capacity_) {
            index -= capacity_;
        }
        std::string line;
        buf_.at(index).toString(line);
        ss[nCurStream] << line;
        ssSize[nCurStream] += line.size();
        // If adding a new line would make size of current string stream exceeding max log size,
        // switch to the other string stream
        if (ssSize[nCurStream] >= MAX_LOG_SIZE) {
            nCurStream = (nCurStream + 1) % 2;
            ss[nCurStream].str("");
            ssSize[nCurStream] = 0;
        }
    }
    // Trying to get MAX_LOG_SIZE chars for persisting.
    if (ssSize[nCurStream] == MAX_LOG_SIZE) {
        dest = ss[nCurStream].str();
    } else if (ssSize[nCurStream] > MAX_LOG_SIZE) {
        int cutPos = ssSize[nCurStream] - MAX_LOG_SIZE;
        dest = ss[nCurStream].str().substr(cutPos);
    } else {
        int nPrevStream = (nCurStream + 1) % 2;
        int prefixLogSize = MAX_LOG_SIZE - ssSize[nCurStream];
        if (ssSize[nPrevStream] <= prefixLogSize) {
            dest = ss[nPrevStream].str();
            dest += ss[nCurStream].str();
        } else {
            int cutPos = ssSize[nPrevStream] - prefixLogSize;
            dest = ss[nPrevStream].str().substr(cutPos);
            dest += ss[nCurStream].str();
        }
    }
}


} // namespace kinetic
} // namespace seagate
} // namespace com
