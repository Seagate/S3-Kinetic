/*
 * SmrEnv.h
 *
 *  Created on: Apr 19, 2015
 *      Author: tri
 */

#ifndef SMRENV_H_
#define SMRENV_H_

#include <sstream>
#include "leveldb/env.h"
#include "util/env_posix.h"
#include "Disk.h"
#include "DriveEnv.h"
#include "SmrSequentialFile.h"
#include "SmrWritableFile.h"
#include "SmrDirectWritableFile.h"
#include "SmrRandomAccessFile.h"
#include "ManifestWritableFile.h"
#include "ManifestSequentialFile.h"
#include "ValueDeleter.h"

#include "stack_trace.h"
using namespace leveldb::posix;

//namespace leveldb {
namespace smr {

class ReplaySegment;

class SmrEnv : public Env {
        friend class DriveEnv;

    public:
        friend ostream& operator<<(ostream& out, SmrEnv& env);

        SmrEnv();
        virtual ~SmrEnv() {
            if (disk_ != NULL){
                const int fd = open(disk_name_.c_str(), O_RDWR);
                disk_->sync(fd);
                close(fd);
                delete disk_;
            }
        }

	virtual bool ISE() {
		return disk_->ISE();
	}

	virtual Status Sync() {
	    return disk_->sync();
	}
    virtual void clearDisk() {
        disk_name_.clear();
    	if (disk_ != NULL) {
            delete disk_;
            disk_ = NULL;
    	}
    }

    virtual bool checkDiskInfoValid() {
        if (disk_ != NULL) {
            return disk_->checkDiskInfoValid();
        }
        return false;
    }

    virtual void FillZoneMap() {
        disk_->FillZoneMap();
    }
    virtual void fileDeleted(uint64_t fnumber, FileType ftype) {
        this->disk_->fileDeleted(fnumber, ftype);
    }
    virtual void segmentCompleted(ReplaySegment* seg) {
        disk_->segmentCompleted(seg);
    }
    virtual void segmentUpdated(ReplaySegment* seg) {
        disk_->segmentUpdated(seg);
    }
    virtual bool IsCorrupted() {
        return disk_->IsCorrupted();
    }
    virtual bool isSuperblockSyncable() {
        return disk_->getNumberGoodSuperblocks() > 1;
    }
    virtual int numberOfGoodSuperblocks() {
        return disk_->getNumberGoodSuperblocks();
    }
    virtual Status status() const {
        return disk_->status();
   }

        virtual Status NewExternalValueDeleter(Logger* logger, const string& dbName, ExternalValueDeleter** result) {
            *result = new ValueDeleter(logger, dbName, disk_);
            return Status::OK();
        }

        virtual Status NewSequentialFile(const std::string& fname,
                SequentialFile** result) {
            *result = NULL;
            Status s;
            uint64_t number;
            FileType type;

            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }

                if ( type == kTableFile || type == kLogFile || type == kDescriptorFile ||
		     type == kValueFile || type == kUSTableFile) {
                    const int fd = open(disk_name_.c_str(), O_RDONLY);
                    if (fd < 0) {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
                        s = IOError(context.str(), errno);
                        return s;
                    }
                    FileInfo* finfo = disk_->getFileInfo (number, type);
                    if (!finfo) {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": File not found: " << fname;
                        s = IOError(context.str(), errno);
                        return s;
                    }
                    if (type == kDescriptorFile) {
                        *result = new ManifestSequentialFile(fname, fd, disk_, page_size_, finfo);
                    } else {
                        *result = new SmrSequentialFile(fname, fd, disk_, page_size_, finfo);
                    }
                    return s;
                }
                else if (type == kCurrentFile) {
                    *result = new SmrSequentialFile(fname, 0, disk_, page_size_, NULL);
                    return s;
                }
                else {
                    s = DriveEnv::getInstance()->NewSequentialFile(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos),
                            result);
                    return s;
                }
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file: " << fname;
                return Status::NotSupported(context.str());
            }
        }

        virtual Status NewRandomAccessFile(const std::string& fname,
                RandomAccessFile** result) {
            *result = NULL;
            Status s;
            uint64_t number;
            FileType type;

            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }

                if ( type == kTableFile || type == kLogFile || type == kDescriptorFile ||
		     type == kValueFile || type == kUSTableFile) {
                    const int fd = open(disk_name_.c_str(), O_RDONLY);
                    if (fd < 0) {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to open "  << disk_name_;
                        s = IOError(context.str(), errno);
                        return s;
                    }
                    if (type == kTableFile || type == kValueFile) {
                        posix_fadvise64(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

                    }
                    FileInfo* finfo = disk_->getFileInfo (number, type);
                    if (!finfo) { //fs.IsNotFound()) {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to get file info" << fname;
                        s = IOError(context.str(), errno);
                        return s;
                    }

                    *result = new SmrRandomAccessFile(fname, fd, disk_, page_size_, finfo);
                    return s;
                }
                else {
                    s = DriveEnv::getInstance()->NewRandomAccessFile(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos),
                            result);
                    return s;
                }
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file: " << fname;
                return Status::NotSupported(context.str());
            }
        }

        virtual Status NewWritableFile(const std::string& fname,
                WritableFile** result) {
            *result = NULL;
            Status s;
            uint64_t number;
            FileType type;
            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }

                if ( type == kTableFile || type == kLogFile || type == kDescriptorFile || type == kValueFile ||
                        type == kUSTableFile) {
                  int fd = -1;

                  if (type == kLogFile || type == kValueFile || type == kTableFile) {
                    fd = open(disk_name_.c_str(), O_RDWR | O_DIRECT); //(Thai use for posix_memalign only)
                  } else {
                    fd = open(disk_name_.c_str(), O_RDWR);
                  }
                  if (fd < 0) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to open " << fname;
                    s = IOError(context.str(), errno);
                    return s;
                  }
                  FileInfo* finfo = disk_->allocateFile(number, type);
                  if (finfo == NULL) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to allocateFile";
                    s = Status::NoSpaceAvailable(context.str());
                    return s;
                  }
                  if (type == kValueFile) {
                    *result = new SmrDirectWritableFile(fname, fd, disk_, page_size_, finfo);
                  } else if (type == kDescriptorFile) {
                    *result = new ManifestWritableFile(fname, fd, disk_, page_size_, finfo);
                  } else {
                    *result = new SmrWritableFile(fname, fd, disk_, page_size_, finfo);
                  }
                  return s;
                }
                else {
                    s = DriveEnv::getInstance()->NewWritableFile(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos),
                            result);
                    return s;
                }
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file: " << fname;
                return s;
            }
        }

        virtual bool FileExists(const std::string& fname) {
            Status s;
            uint64_t number;
            FileType type;

            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return false;
                }
                if( type == kTableFile || type == kLogFile || type == kDescriptorFile ||
		    type == kValueFile || type == kUSTableFile) {
                    FileInfo* finfo = disk_->getFileInfo(number, type);
                    if (!finfo) {
                        return false;
                    } else {
                        return true;
                    }
                }
                else if (type == kCurrentFile) {
                    return (disk_->getCURRENTsize() != 0);
                }
                else{
                    return DriveEnv::getInstance()->FileExists(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos));
                }
            } else {
                return false;
            }
        }

        // List all files
        virtual Status GetChildren(const std::string& dir,
                std::vector<std::string>* result) {
            Status s;
            result->clear();
            if(disk_name_.empty())
                return Status::OK();
            if (disk_name_ != dir) {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << dir;
                return IOError(context.str(), -1);
            }
            // Iterate through the map and include all files and concat result with the local file results and return
            disk_->getFiles(result);
            std::vector<std::string> local_result;
            s = DriveEnv::getInstance()->GetChildren(Disk::LEVEL_INFO_DIR + dir.substr(4, std::string::npos), &local_result);
            if (!s.ok())
                return s;
            result->insert(result->end(), local_result.begin(), local_result.end());
            return Status::OK();
        }

        virtual Status CreateDir(const std::string& name, bool create_if_missing = false) {
            Status result;
            // TODO: Lock and change stuff
            if (!disk_name_.empty()) {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": disk_name_ empty";
                result = IOError(context.str(), -1);
                return result;
            }
            DriveEnv::getInstance()->CreateDir(Disk::LEVEL_INFO_DIR);
            const int fd = open(name.c_str(), O_RDWR | O_DIRECT);
            if (fd < 0) {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to open " << name;
                return IOError(context.str(), errno);
            }

            disk_ = new Disk(fd, page_size_, name, create_if_missing);

            if (!disk_->loadDB()) {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to create Disk " << name;
                cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to create Disk " << name << endl;
                return IOError(context.str(), errno);
            }

            DriveEnv::getInstance()->CreateDir(Disk::LEVEL_INFO_DIR + name.substr(4, std::string::npos));
            disk_name_ = name;
            return result;
        }

        virtual int GetZonesUsed() {
            return disk_->getZonesUsedInZoneMap();
        }

        virtual Status DeleteDir(const std::string& name) {
            Status result;
            if (disk_name_ != name) {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << disk_name_ << " != " << name;
                result = IOError(context.str(), -1);
            }
            else {
                const int fd = open(disk_name_.c_str(), O_RDWR);
                char *buf;
                if (posix_memalign((void**)&buf, page_size_, Disk::SUPER_ZONE_SIZE)) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << disk_name_;
                    return IOError(context.str(), errno);
                }
                memset(buf, 0, Disk::SUPER_ZONE_SIZE);
                lseek(fd, 0, SEEK_SET);
                int r = write(fd, buf, Disk::SUPER_ZONE_SIZE);
                if ( r < 0 ) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": ";
                    return IOError(context.str(), errno);
                }
                free(buf);
                close(fd);

                disk_name_.clear();
                // TODO: Remove init info on disk first block
                delete disk_;
                disk_ = NULL;
                DriveEnv::getInstance()->DeleteFile(Disk::LEVEL_INFO_DIR + name.substr(4, std::string::npos) + "/LOCK");
                result = DriveEnv::getInstance()->DeleteDir(Disk::LEVEL_INFO_DIR + name.substr(4, std::string::npos));
            }
            return result;
        }

        virtual Status DeleteFile(const std::string& fname) {
            Status s;
            uint64_t number;
            FileType type;

            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }

                if ( type == kTableFile || type == kLogFile || type == kDescriptorFile ||
		     type == kValueFile || type == kUSTableFile) {
                    return disk_->deallocateFile(fname, number, type);
                }
                else if (type == kCurrentFile) {
                    std::string empty;
                    disk_->putCURRENT(&empty);
                    return s;
                }
                else {
                    return DriveEnv::getInstance()->DeleteFile(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos));
                }
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file: " << fname;
                return Status::NotSupported(context.str());
            }
        }
        virtual Status DeleteFile(uint64_t fnumber, FileType type) {
            Status status = disk_->deallocateFile("", fnumber, type);
            if ((status.ok() || status.IsNotFound()) && type == kValueFile) {
                disk_->removeObsoleteValueFile(fnumber);
            }
            return status;
        }

        virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
            Status s;
            uint64_t number;
            FileType type;

            *size = 0;
            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }
                if( type == kTableFile || type == kLogFile || type == kDescriptorFile || type == kUSTableFile) {
                    FileInfo* finfo = disk_->getFileInfo(number, type);
                    if (!finfo) {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
                        return Status::NotFound(context.str());
                    } else {
                        *size = finfo->getSize();
                        return Status::OK();
                    }
                }
                else if (type == kCurrentFile){
                    *size = disk_->getCURRENTsize();
                    return s;
                }
                else{
                    return DriveEnv::getInstance()->GetFileSize(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos), size);
                }
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file " << fname;
                return Status::NotSupported(context.str());
            }
        }

        virtual Status RenameFile(const std::string& src, const std::string& target) {
            Status s, ts;
            uint64_t number, tnumber;
            FileType type, ttype;

            bool src_file = DetermineFile(src, &s, &number, &type);
            bool target_file = DetermineFile(target, &ts, &tnumber, &ttype);
            if (src_file && target_file){
                if (!s.ok() || !ts.ok()) {
                    if (!s.ok() && !ts.ok()) {
                        stringstream context;
                        context << s.ToString() << ". " << ts.ToString();
                        s = IOError(context.str(), -1);
                    } else if (!ts.ok()) {
                        s = ts;
                    }
                    return s;
                }

                if (type == kTableFile || type == kLogFile || type == kDescriptorFile || type == kUSTableFile
                        || type == kSSTTempFile) {
                    assert(number == tnumber);
                    FileInfo* finfo = disk_->getFileInfo(number, type);
                    if (finfo) {
                        finfo->setNumber(tnumber);
                        finfo->setType(ttype);
                    } else {
                        stringstream context;
                        context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << src;
                        return Status::NotFound(context.str());
                    }
                    return Status::OK();
                }
                else if (type == kTempFile && ttype == kCurrentFile) {
                    std::string curr;
                    s = ReadFileToString(this, src, &curr);
                    if (s.ok()) {
                        disk_->putCURRENT(&curr);
                    }
                    s = DeleteFile(src);
                    return s;
                }
                else {
                    return DriveEnv::getInstance()->RenameFile(Disk::LEVEL_INFO_DIR + src.substr(4, std::string::npos),
                            Disk::LEVEL_INFO_DIR + target.substr(6, std::string::npos));
                }
            }
            else if (!src_file && !target_file) {
                s = DriveEnv::getInstance()->RenameFile(Disk::LEVEL_INFO_DIR + src.substr(4, std::string::npos),
                        Disk::LEVEL_INFO_DIR + target.substr(6, std::string::npos));
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Either fname is null, src = "
                        << src << ", target = " << target;
                s = IOError(context.str(), -1);
            }
            return s;
        }

        virtual Status LockFile(const std::string& fname, FileLock** lock) {
            Status s;
            uint64_t number;
            FileType type;

            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }
                return DriveEnv::getInstance()->LockFile(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos), lock);
            } else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file: " << fname;
                return s; //Status::NotSupported(context.str());
            }
            return s;
        }

        virtual Status UnlockFile(FileLock* lock) {
            return Status::NotSupported("Call PosixEnv::UnlockFile()");
        }

        virtual void ClearBG();
        virtual void Schedule(void (*function)(void*), void* arg, void (*bg_function)(void*));
        virtual void ScheduleDefrag(void (*function)(void*), void* arg, void (*bg_function)(void*));

        virtual void StartThread(void (*function)(void* arg), void* arg);

        virtual Status GetTestDirectory(std::string* result) {
            *result = "/dev/sda3";
            CreateDir(*result);
            return Status::OK();
        }

        static uint64_t gettid() {
            pthread_t tid = pthread_self();
            uint64_t thread_id = 0;
            memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
            return thread_id;
        }

        virtual Status NewLogger(const std::string& fname, Logger** result) {
            Status s;
            uint64_t number;
            FileType type;

            if (DetermineFile(fname, &s, &number, &type)){
                if(!s.ok()){
                    return s;
                }
                return NewLogger(Disk::LEVEL_INFO_DIR + fname.substr(4, std::string::npos), result);
            }
            FILE* f = fopen(fname.c_str(), "w");
            if (f == NULL) {
                *result = NULL;
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
                return IOError(context.str(), errno);
            } else {
                *result = new PosixLogger(f, &SmrEnv::gettid);
                return Status::OK();
            }
        }

        virtual uint64_t NowMicros() {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
        }

        virtual void SleepForMicroseconds(time_t micros) {
            struct timespec sleep_time;
            sleep_time.tv_sec= 0;
            sleep_time.tv_nsec = micros * 1000;
            nanosleep(&sleep_time, NULL);
        }

        virtual Status Defragment(int level) {
            return disk_->defragment(level);
        }
        virtual Status DefragmentExternal(const Options& options, put_func_t putFunc)  {
            return disk_->defragmentExternal(options, putFunc);
        }
        virtual bool IsFragmented() {
            return disk_->isFragmented(0);
        }
        virtual bool IsValueFragmented() {
            return disk_->isValueFragmented();
        }
        virtual bool IsHighDiskUsage() {
            return disk_->isHighDiskUsage();
        }
        virtual bool GetCapacity(uint64_t* totalBytes, uint64_t* usedBytes) {
            disk_->getCapacity(totalBytes, usedBytes);
            return true;
        }
        virtual bool GetZoneUsage(std::string& s) {
            return disk_->zoneUsageLogBuilder(s);
        }
        virtual int GetNumberFreeZones() {
            return disk_->GetNumFreeZones();
        }
        virtual bool IsBlockedFile(uint64_t fnumber) {
            return disk_->isBlockedFile(fnumber);
        }
        virtual void GetObsoleteValueFiles(set<uint64_t>& obsoleteFiles) {
            disk_->getObsoleteValueFiles(obsoleteFiles);
        }

    private:
        void PthreadCall(const char* label, int result) {
            if (result != 0 && result!=ETIMEDOUT) {
                fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
                abort();
            }
        }

        // BGThread() is the body of the background thread
        void BGThread();
        void BGDefragThread();
        static void* BGThreadWrapper(void* arg) {
            reinterpret_cast<SmrEnv*>(arg)->BGThread();
            return NULL;
        }
        static void* BGDefragThreadWrapper(void* arg) {
            reinterpret_cast<SmrEnv*>(arg)->BGDefragThread();
            return NULL;
        }

        size_t page_size_;
        pthread_mutex_t mu_;
        pthread_cond_t bgsignal_;
        pthread_mutex_t sl_mu_;
        pthread_cond_t slsignal_;
        pthread_mutex_t sl_bg_mu_;
        pthread_cond_t sl_bgsignal_;
        pthread_t bgthread_;
        bool started_bgthread_;
        bool stop_bgthread_;

        // Entry per Schedule() call
        struct BGItem { void* arg; void (*function)(void*); };
        typedef std::deque<BGItem> BGQueue;
        BGQueue queue_;

        bool has_bg_item_;
        struct BGItem bg_item_;

        PosixLockTable locks_;
        MmapLimiter mmap_limit_;

        std::string disk_name_;
        Disk *disk_;

        pthread_mutex_t defragmu_;
        bool started_bgDefragThread_;
        bool has_bg_defrag_item_;
        struct BGItem  bg_defrag_item_;
        BGQueue defrag_queue_;
        pthread_cond_t bgDefragSignal_;
        pthread_t bgDefragThread_;

        bool DetermineFile(const std::string& fname, Status *s, uint64_t *number, FileType *type) {
            bool is_disk_file;
            Slice path = fname;

            if (!path.starts_with("/dev/")) {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Not an smr file: " << fname;
                *s = Status::NotSupported(context.str());
                is_disk_file = false;
            } else if(disk_) {
                is_disk_file = true;
                if (!path.starts_with(disk_name_ + "/")) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": " << fname;
                    *s = IOError(context.str(), -1);
                }
                else if (!ParseFileName(fname.substr(disk_name_.size() + 1, std::string::npos), number, type)) {
                    stringstream context;
                    context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": Failed to parse file name: " << fname;
                    *s = IOError(context.str(), -1);
                }
            }
            else {
                stringstream context;
                context << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": disk_ is NULL; "
                        << fname;
                *s = Status::InvalidArgument(context.str());
                is_disk_file = false;
            }
            return is_disk_file;
        }
};
}  // namespace smr
//}  // namespace leveldb


#endif /* SMRENV_H_ */
