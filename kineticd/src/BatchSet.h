/*
 * BatchSet.h
 *
 *  Created on: Mar 30, 2016
 *      Author: tri
 */

#ifndef BATCHSET_H_ //NOLINT
#define BATCHSET_H_

#include <sstream>
#include <time.h>
#include <list>

#include "util/mutexlock.h"
#include "mem/DynamicMemory.h"
#include "mem/KineticMemory.h"
#include "BatchCmd.h"
#include "CommandValidator.h"
#include "kinetic.pb.h"
#include "common/event/Event.h"
#include "common/event/Publisher.h"
#include "common/event/EventService.h"
#include "command_line_flags.h"

using namespace com::seagate::kinetic::proto; //NOLINT
using namespace std; //NOLINT
using namespace smr; //NOLINT
using namespace leveldb; //NOLINT
using namespace leveldb::port; //NOLINT
using namespace com::seagate::common::event; //NOLINT

namespace leveldb {
    class WriteBatch;
}

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {

using leveldb::WriteBatch;

class BatchSetCollection;

class BatchSet : public Publisher {
 public:
    static const uint64_t TIMEOUT = 3600000; // milliseconds or 3600 seconds

    static string createBatchId(int64_t connId, int id) {
        stringstream ss;
        ss << std::dec << connId << "-" << id;
        return ss.str();
    }
    friend ostream& operator<<(ostream& os, const BatchSet& batchSet);

 public:
    BatchSet(Command* command, std::shared_ptr<Connection> connection,
              std::chrono::high_resolution_clock::time_point enqueuedTime, BatchSetCollection* batchSets):
              batchSets_(batchSets)  {
        id_ = command->header().batchid();
        connFd_ = connection->fd();
        connId_ = connection->id();
        cmdPriority_ = command->header().priority();
        timeout_ = command->header().timeout();
        if (timeout_ == 0) {
            timeout_ = TIMEOUT;
        }
        clockticktimeout_ = timeout_;
        size_ = 0;
        timequanta_ = command->header().timequanta();
        earlyexit_ = command->header().earlyexit();
        clusterversion_ = command->header().clusterversion();
        memory_ = new KineticMemory();
        startTime_ = (uint64_t)time(NULL);
        enqueuedTime_ = enqueuedTime;
        ref_ = 1;
        hasPutCmd_ = false;
        hasDelCmd_ = false;
        doNotFreeBuf_ = false;
        complete_= false;
    }
    void createWriteBatch(WriteBatch& writeBatch, Command& commandResponse);
    bool isValid(CommandValidator& validator, Command& response, int64_t user_id, RequestContext& request_context);
    int connFd() const {
        return connFd_;
    }
    uint getNumBatchedCmds() const {
        return batchCmds_.size();
    }
    bool addCommand(Message_AuthType authType, Command* cmd, IncomingValueInterface* value,
        Command& cmdResponse) {
        BatchCmd* batchCmd = new BatchCmd(authType, cmd, value, cmdResponse, this);
        MutexLock lock(&mu_);
        bool success = addCommand(batchCmd, cmdResponse);
       /* if (!success) {
            batchCmd->setValue(NULL);
            delete batchCmd;
        }*/
        if (batchCmd->isPut()) {
            size_ += value->size();
        } else {
            //For delete command, add 4k to limit the number of deletes in a batch
            size_ += 4096;
        }
        return success;
    }
    string getId() const {
        return createBatchId(connId_, id_);
    }
    KineticMemory* getMemory() const {
        return memory_;
    }
    void setMemory(KineticMemory* memory) {
        MutexLock lock(&mu_);
        memory_ = memory;
    }
    char* allocateMemory(int size) {
        MutexLock lock(&mu_);
        char* buff = NULL;
        if (memory_) {
            buff = memory_->allocate(size);
        }
        return buff;
    }
    int64_t getConnectionId() const {
        return connId_;
    }
    int getConnectionFd() const {
        return connFd_;
    }
    int getCmdPriority() const {
        return cmdPriority_;
    }
    uint64_t getTimeOut() const {
        return timeout_;
    }
    uint64_t getTimeQuanta() const {
        return timequanta_;
    }
    bool getEarlyExit() const {
        return earlyexit_;
    }
    int64_t getClusterVersion() const {
        return clusterversion_;
    }
    void setTimeout(uint64_t timeout) {
        timeout_ = timeout;
    }
    void setTimeQuanta(uint64_t timequanta) {
        timequanta_ = timequanta;
    }
    void setEarlyExit(bool earlyexit) {
        earlyexit_ = earlyexit;
    }
    void setClusterVersion(int64_t clusterversion) {
        clusterversion_ = clusterversion;
    }

    void clockTick(uint64_t seconds) {
        if (clockticktimeout_ == 0 || complete_) {
           // timeout was handled.  No need to be handled again
           return;
        }
        if (clockticktimeout_ > seconds*1000) {
            clockticktimeout_ -= seconds*1000;
        } else {
            clockticktimeout_ = 0;
        }
        if (clockticktimeout_ == 0) {
            TimeoutEvent event(this);
            EventService::getInstance()->publish(&event);
        }
    }
    bool isTimeOut() {
        return (timeout_ == 0);
    }
    bool isAtMax() {
        return (size_ == (uint64_t) FLAGS_max_batch_size);
    }
    bool isGreaterMax() {
        return (size_ > (uint64_t) FLAGS_max_batch_size);
    }
    void setHasPutCmd() {
        hasPutCmd_ = true;
    }
    bool hasPutCmd() {
        return hasPutCmd_;
    }
    void setHasDelCmd() {
        hasDelCmd_ = true;
    }
    bool hasDelCmd() {
        return hasDelCmd_;
    }
    void ref() {
        MutexLock lock(&mu_);
        ++ref_;
    }
    void freeBuf() {
        int nAllocatedSize = 0;
        for (std::list<BatchCmd*>::iterator it = batchCmds_.begin(); it != batchCmds_.end(); ++it) {
            BatchCmd* cmd = *it;
            if (cmd->isPut()) {
               if (cmd->getValue()->GetUserValue()) {
                   nAllocatedSize += ROUNDUP(cmd->getValue()->size(), 4096);
                   free(cmd->getValue()->GetUserValue());
                   cmd->getValue()->SetBuffValueToNull();
               }
            }
        }
        DynamicMemory::getInstance()->deallocate(nAllocatedSize);
    }

    void setDoNotFreeBuf() { doNotFreeBuf_ = true;}

    void unref(bool shuttingDown = false) {
        if (shuttingDown) {
            delete this;
            return;
        }
        mu_.Lock();
        --ref_;
        if (!ref_) {
            mu_.Unlock();
            delete this;
        } else {
            mu_.Unlock();
        }
    }
    bool isComplete() {
        return complete_;
    }
    void complete() {
        MutexLock l(&mu_);
        complete_ = true;
    }

    std::chrono::high_resolution_clock::time_point getEnqueuedTime() { return enqueuedTime_;}

 private:
    virtual ~BatchSet() {
        if (!doNotFreeBuf_) {
            freeBuf();
        }
        for (std::list<BatchCmd*>::iterator it = batchCmds_.begin(); it != batchCmds_.end(); ++it) {
            delete *it;
        }
        delete memory_;
    }
    bool isValidVersion(CommandValidator& validator, int cmdIdx, Command& commandResponse);
    bool addCommand(BatchCmd* cmd, Command& commandResponse);

 private:
    uint32_t id_;
    int connFd_;
    int64_t connId_;
    ::com::seagate::kinetic::proto::Command_Priority cmdPriority_;
    uint32_t size_;
    BatchSetCollection* batchSets_;
    uint64_t clockticktimeout_;
    uint64_t timeout_;
    uint64_t timequanta_;
    bool earlyexit_;
    int64_t clusterversion_;
    uint64_t startTime_;
    std::chrono::high_resolution_clock::time_point enqueuedTime_;
    KineticMemory* memory_;
    std::list<BatchCmd*> batchCmds_;
    port::Mutex mu_;
    int ref_;
    bool hasPutCmd_;
    bool hasDelCmd_;
    bool doNotFreeBuf_;
    bool complete_;
};

} /* namespace cmd */
} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */

#endif /* BATCHSET_H_ */ //NOLINT
