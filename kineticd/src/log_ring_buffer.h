#ifndef KINETIC_LOG_RING_BUFFER_H_
#define KINETIC_LOG_RING_BUFFER_H_

#include "glog/logging.h"
#include "kinetic/common.h"
#include <pthread.h>

#include <fstream>
#include <set>
#include "kinetic.pb.h"

#include <chrono>
#include "pthreads_mutex_guard.h"

namespace com {
namespace seagate {
namespace kinetic {

using namespace std::chrono; //NOLINT
using proto::Command_MessageType;

struct LogRingBufferBaseEntry {
    ::std::string message;
    virtual ~LogRingBufferBaseEntry() {}
    virtual void toString(std::string &line) {}
};

struct LogRingBufferEntry : LogRingBufferBaseEntry {
    ::google::LogSeverity severity;
    ::std::string full_filename;
    ::std::string base_filename;
    int line;
    struct ::tm tm_time;
    // ::std::string message;
    pid_t thread_id;

    void toString(std::string &struct_line) {
        const int kDateBufLength = 100;
        char date_buf[kDateBufLength];
        int formatted_date_len;

        // itterate through buffer
        // Date
        formatted_date_len = strftime(date_buf, kDateBufLength,
            "%Y-%m-%dT%H:%M:%SZ", &(tm_time));
        if (formatted_date_len == 0) {
            struct_line += "-";
        } else {
            struct_line += date_buf;
        }
        struct_line += " ";
        // severity
        CHECK_LT(severity, ::google::NUM_SEVERITIES);
        struct_line += ::google::LogSeverityNames[severity];
        struct_line += " ";
        // Thread id
        struct_line += std::to_string(thread_id);
        struct_line += " ";
        // File name
        struct_line += base_filename;
        struct_line += ":";
        // Line number
        struct_line += std::to_string(line);
        struct_line += " ";
        // Message
        struct_line += message;
        struct_line += "\n";
    }
};

struct LogRingCommandEntry : LogRingBufferBaseEntry {
    std::time_t time_enqueued;
    std::time_t time_dequeued;
    int time_responded;
    std::string success;
    // ::std::string message;
    void toString(std::string &line) {
        // Time queued
        line += "Time queued: ";
        line += std::to_string(time_enqueued);
        line += " ";
        // Time dequeued
        line += "Time dequeued: ";
        line += std::to_string(time_dequeued);
        line += " ";
        // Time process
        line += "Time to process: ";
        line += std::to_string(time_responded);
        line += " ";
        // Command type
        line += "Command Type: ";
        line += message;
        // Success
        line += " ";
        line += "Successful: ";
        line += success;
        line += "\n";
    }
};

struct LogRingKeyValueEntry : LogRingBufferBaseEntry {
    int key_size;
    int value_size;
    int frequency;
    void toString(std::string &line) {
        // Key size
        line += "(Key size: ";
        line += std::to_string(key_size);
        line += ", Value size: ";
        line += std::to_string(value_size);
        line += ")";
        line += "==";
        line += std::to_string(frequency);
        line += "\n";
    }
};

struct LogRingStaleEntry : LogRingBufferBaseEntry {
    int level;
    int frequency;
    void toString(std::string &line) {
        line += "Level: ";
        line += std::to_string(level);
        line += " Frequency: ";
        line += std::to_string(frequency);
        line += "\n";
    }
};

struct LogRingKeySizeEntry : LogRingBufferBaseEntry {
    int key_size;
    int frequency;
    void toString(std::string &line) {
        line += "Key Size: ";
        line += std::to_string(key_size);
        line += " Frequency: ";
        line += std::to_string(frequency);
        line += "\n";
    }
};

struct LogRingValueSizeEntry : LogRingBufferBaseEntry {
    int key_size;
    int frequency;
    void toString(std::string &line) {
        line += "Value Size: ";
        line += std::to_string(key_size);
        line += " Frequency: ";
        line += std::to_string(frequency);
        line += "\n";
    }
};

/**
* Threadsafe queue of log messages
*/
class LogRingBuffer {
    public:
    static const int NUMBER_OF_LOG_FILES;
    static LogRingBuffer* Instance();
    ~LogRingBuffer();

    void push(::google::LogSeverity severity,
        const char *full_filename,
        const char *base_filename,
        int line,
        const struct ::tm *tm_time,
        const char *message,
        size_t message_len,
        pid_t thread_id);

    void parseAndSetStatusCodes(std::string status_codes);

    void setLogFilePaths(std::string log_file_path, std::string old_log_file_path,
        std::string command_log_file_path, std::string command_old_log_file_path,
        std::string key_value_log_file_path, std::string old_key_value_log_file_path,
        std::string key_size_log_file_path, std::string old_key_size_log_file_path,
        std::string value_size_log_file_path, std::string old_value_size_log_file_path,
        std::string stale_data_log_file_path, std::string old_stale_data_log_file_path);

    void getCommandMessage(std::string &message_out, Command_MessageType message_type);
    std::string getLogFilePath() {
        return log_file_path_;
    }
    // how many slots exist total
    uint32_t capacity();
    // how many slots are used right now
    uint32_t size();

    uint32_t command_size();

    std::vector<LogRingCommandEntry> command_buffer();

    std::vector<LogRingStaleEntry> stale_buffer();

    std::vector<LogRingKeyValueEntry> key_value_histo_buffer();

    std::vector<LogRingKeySizeEntry> key_size_histo_buffer();

    std::vector<LogRingValueSizeEntry> value_size_histo_buffer();

    uint32_t num_key_value();

    uint32_t num_key_size();

    uint32_t num_value_size();

    uint32_t num_stale_entry();

    uint32_t command_start();
    void copyCommandBuffer(std::vector<LogRingCommandEntry> &dest);
    void copyKeyValueBuffer(std::vector<LogRingKeyValueEntry> &dest);
    void copyKeySizeBuffer(std::vector<LogRingKeySizeEntry> &dest);
    void copyValueSizeBuffer(std::vector<LogRingValueSizeEntry> &dest);
    void copyStaleBuffer(std::vector<LogRingStaleEntry> &dest);

    // write current contents of specified buffer into dest
    template<typename T>
    void copyBuffer(std::vector<T> &dest) { //, std::vector<T> buffer, uint32_t size, uint32_t start) {
        PthreadsMutexGuard guard(&mutex_);

        dest.reserve(size_);

        size_t index;
        for (uint32_t i = 0; i < size_; i++) {
            index = start_ + i;
            if (index >= capacity_) {
                index -= capacity_;
            }
            assert(index < buf_.size()); // NOLINT
            dest.push_back(buf_.at(index));
        }
    };

    void toString(std::string& dest);

    // write contents of a given buffer to disk
    template<typename T>
    void writeBufferToDisk(std::vector<T> buf, uint32_t size, uint32_t start,
        std::string log_file, std::string old_log_file) {
        // write what is ever in buffer to file in util partition
        // check if file exists and rename other log files appropriately
        renameFile(log_file, old_log_file, 0);
        // set log file ready to be written to
        std::ofstream logFile(log_file, std::ofstream::binary);
        std::string buffer;
        size_t index;

        buffer.reserve(size_ * 40);

        // itterate through buffer
        for (uint32_t i = 0; i < size; i++) {
            index = start + i;
            if (index >= capacity_) {
                index -= capacity_;
            }
            std::string line;
            assert(index < buf.size()); // NOLINT
            buf.at(index).toString(line);
            buffer += line;
        }
        // write to logfile
        logFile << buffer;
        // close file
        logFile.close();
    }

    // write what's in buffer to disk
    void makePersistent();
    // write what's in histogram buffer to disk
    void makeHistoPersistent();
    // write what's in stale_entry buffer to disk
    void makeStaleEntryDataPersistent();
    // write whats in stale_map to disk
    void recordStaleDataToBuffer(int level);
    // method to rename log files
    void renameFile(std::string file_name, std::string old_file_name, int file_number);
    // checks is status code is in set and calls makePersistent
    bool checkTrigger(proto::Command_Status_StatusCode);
    // change capcaity
    void changeCapacity(uint32_t capcaity);
    // clear buffer
    void clearBuffer();

    void logCommand(std::time_t time_enqueued, std::time_t time_dequeued,
        int time_responded, Command_MessageType message_type, std::string success);

    void logKeyValueHisto(int key_size, int value_size);

    void logTransferLength(int key_size, int value_size);

    private:
    explicit LogRingBuffer(uint32_t capacity);  // Private so that it cannot be called
    pthread_mutex_t mutex_;
    std::vector<LogRingBufferEntry> buf_;
    std::vector<LogRingCommandEntry> command_buf_;
    std::vector<LogRingKeyValueEntry> key_value_histo_buf_;
    std::vector<LogRingKeySizeEntry> key_size_histo_buf_;
    std::vector<LogRingValueSizeEntry> value_size_histo_buf_;
    std::vector<LogRingStaleEntry> stale_buf_;

    std::map<std::pair<int, int>, int> key_value_histo_map_;
    std::map<int, int> key_size_map_;
    std::map<int, int> value_size_map_;
    std::map<int, int> stale_entry_map_;
    // length of buf_ array
    uint32_t capacity_;
    // starting index of the written-to block of cells in buf_
    uint32_t start_;

    uint32_t command_start_;

    uint32_t stale_size_;
    // how many slots contain written-to values
    uint32_t size_;

    uint32_t command_size_;

    uint32_t stale_start_;

    std::set<proto::Command_Status_StatusCode> status_codes_;

    std::string log_file_path_;
    std::string old_log_file_path_;
    std::string command_log_file_path_;
    std::string command_old_log_file_path_;
    std::string stale_data_log_file_path_;
    std::string old_stale_data_log_file_path_;
    std::string key_value_log_file_path_;
    std::string old_key_value_log_file_path_;
    std::string key_size_log_file_path_;
    std::string old_key_size_log_file_path_;
    std::string value_size_log_file_path_;
    std::string old_value_size_log_file_path_;
    uint32_t num_key_value_;
    uint32_t num_key_size_;
    uint32_t num_value_size_;
    uint32_t num_stale_entry_;
    static LogRingBuffer* log_ring_buffer_instance_;

    DISALLOW_COPY_AND_ASSIGN(LogRingBuffer);
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_LOG_RING_BUFFER_H_
