/*
 * DriveEnv.h
 *
 *  Created on: Mar 20, 2015
 *      Author: tri
 */

#ifndef DRIVEENV_H_
#define DRIVEENV_H_

#include <string>
#include <iostream>
#include "db/filename.h"
#include "leveldb/env.h"

using namespace leveldb;
using namespace std;

//namespace leveldb {
namespace smr {
using leveldb::Status;

class ReplaySegment;

class DriveEnv: public Env {
    private:
        static DriveEnv* instance_;

        static void* BGThreadWrapper(void* arg);

    public:
        friend ostream& operator<<(ostream& out, DriveEnv& env);

        static DriveEnv* getInstance();

        virtual ~DriveEnv() {
            delete posixEnv_;
            delete smrEnv_;
        }
        virtual Status NewExternalValueDeleter(Logger* logger, const string& dbName, ExternalValueDeleter** result) {
            return smrEnv_->NewExternalValueDeleter(logger, dbName, result);
        }
        virtual Status NewWritableFile(const string& fname, WritableFile** result) {
            return getEnv(fname)->NewWritableFile(fname, result);
        }
        virtual Status NewRandomAccessFile(const string& fname, RandomAccessFile** result) {
            return getEnv(fname)->NewRandomAccessFile(fname, result);
        }
        virtual Status NewSequentialFile(const string& fname, SequentialFile** result) {
            return getEnv(fname)->NewSequentialFile(fname, result);
        }
        virtual bool FileExists(const string& fname) {
            return getEnv(fname)->FileExists(fname);
        }
        virtual Status GetChildren(const string& dir, vector<string>* result) {
            return getEnv(dir)->GetChildren(dir, result);
        }
        virtual Status CreateDir(const string& name, bool create_if_missing = false) {
            return getEnv(name)->CreateDir(name, create_if_missing);
        }
        virtual int GetZonesUsed() {
            return smrEnv_->GetZonesUsed();
        }
        virtual Status DeleteDir(const string& name){
            return getEnv(name)->DeleteDir(name);
        }
        virtual Status DeleteFile(const string& fname) {
            return getEnv(fname)->DeleteFile(fname);
        }
        virtual Status DeleteFile(uint64_t fnumber, FileType type) {
            return smrEnv_->DeleteFile(fnumber, type);
        }
        virtual Status GetFileSize(const string& fname, uint64_t* size) {
            return getEnv(fname)->GetFileSize(fname, size);
        }
        virtual Status RenameFile(const string& src, const string& target) {
            return getEnv(src)->RenameFile(src, target);
        }
        virtual Status LockFile(const string& fname, FileLock** lock) {
            return getEnv(fname)->LockFile(fname, lock);
        }
        virtual Status UnlockFile(FileLock* lock) {
            return posixEnv_->UnlockFile(lock);
        }
        virtual void ClearBG() {
            smrEnv_->ClearBG();
        }
        virtual void Schedule(void (*function)(void* arg), void* arg, void (*bg_function)(void* arg)) {
            smrEnv_->Schedule(function, arg, bg_function);
        }
        virtual void StartThread(void (*function)(void* arg), void* arg) {
            smrEnv_->StartThread(function, arg);
        }
        virtual void ScheduleDefrag(void (*function)(void* arg), void* arg, void (*bg_function)(void* arg)) {
            smrEnv_->ScheduleDefrag(function, arg, bg_function);
        }
        virtual Status GetTestDirectory(string* result) {
            return smrEnv_->GetTestDirectory(result);
        }
        static uint64_t gettid();

        virtual Status NewLogger(const string& fname, Logger** result) {
            return getEnv(fname)->NewLogger(fname, result);
        }
        virtual uint64_t NowMicros() {
            return posixEnv_->NowMicros();
        }
        virtual void SleepForMicroseconds(int micros) {
            posixEnv_->SleepForMicroseconds(micros);
        }
        virtual Status Defragment(int level) {
            return smrEnv_->Defragment(level);
        }
        virtual Status DefragmentExternal(const Options& options, put_func_t putFunc) {
            return smrEnv_->DefragmentExternal(options, putFunc);
        }
        virtual bool IsFragmented() {
            return smrEnv_->IsFragmented();
        }
        virtual bool IsValueFragmented() {
            return smrEnv_->IsValueFragmented();
        }
        virtual bool IsHighDiskUsage() {
            return smrEnv_->IsHighDiskUsage();
        }
        virtual bool GetCapacity(uint64_t* totalBytes, uint64_t* usedBytes) {
            return smrEnv_->GetCapacity(totalBytes, usedBytes);
        }
        virtual bool ISE() {
            return smrEnv_->ISE();
        }
        virtual void clearDisk() {
        	smrEnv_->clearDisk();
        	posixEnv_->clearDisk();
        }
        virtual bool checkDiskInfoValid() {
            return smrEnv_->checkDiskInfoValid();
        }
        virtual Status Sync() {
            return smrEnv_->Sync();
        }
        virtual bool IsCorrupted() { //status() {
            return smrEnv_->IsCorrupted(); //status();
        }
        virtual bool GetZoneUsage(std::string& s) {
            return smrEnv_->GetZoneUsage(s);
        }
        virtual void FillZoneMap() {
            smrEnv_->FillZoneMap();
        }
        void fileDeleted(uint64_t fnumber, FileType ftype) {
            smrEnv_->fileDeleted(fnumber, ftype);
        }
        void segmentCompleted(ReplaySegment* seg) {
            smrEnv_->segmentCompleted(seg);
        }
        void segmentUpdated(ReplaySegment* seg) {
            smrEnv_->segmentUpdated(seg);
        }
        virtual int GetNumberFreeZones() {
            return smrEnv_->GetNumberFreeZones();
        }
        void storePartition(string& partition) {
            storePartition_ = partition;
        }
        string storePartition() {
            return storePartition_;
        }

    private:
        bool isSmrFile(const string& fname) const {
            Slice path = fname;
            return path.starts_with("/dev/");
        }
        Env* getEnv(const string& fname) const {
            if (isSmrFile(fname)) {
                return smrEnv_;
            } else {
                return posixEnv_;
            }
        }
        virtual bool isSuperblockSyncable() {
            return smrEnv_->isSuperblockSyncable();
        }
        virtual int numberOfGoodSuperblocks() {
            return smrEnv_->numberOfGoodSuperblocks();
        }
        virtual Status status() const {
            return smrEnv_->status();
        }
        virtual bool IsBlockedFile(uint64_t fnumber) {
            return smrEnv_->IsBlockedFile(fnumber);
        }
        virtual void GetObsoleteValueFiles(set<uint64_t>& obsoleteFiles) {
            smrEnv_->GetObsoleteValueFiles(obsoleteFiles);
        }

    private:
        DriveEnv();
        DriveEnv(const DriveEnv& src);
        DriveEnv& operator=(const DriveEnv& src);

    private:
        Env* smrEnv_;
        Env* posixEnv_;
        string storePartition_;
};

} // namespace smr
//} /* namespace smrdbdisk */

#endif /* DRIVEENV_H_ */
