/*
 * BatchCollection.h
 *
 *  Created on: Mar 30, 2016
 *      Author: tri
 */

#ifndef BATCHCOLLECTION_H_ //NOLINT
#define BATCHCOLLECTION_H_

#include <unordered_map>

#include "util/mutexlock.h"
#include "BatchCmd.h"
#include "BatchSet.h"
#include "kinetic.pb.h"

using namespace com::seagate::kinetic::proto; //NOLINT
using namespace leveldb; //NOLINT
using namespace leveldb::port; //NOLINT
using namespace std; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {

class BatchSetCollection {
 public:
    // For OPEN VSTORE
    static const size_t MAX_BATCHES = 10000;
    //static const size_t MAX_BATCHES = 5;
    friend ostream& operator<<(ostream& os, const BatchSetCollection& batchSetCollection);

 public:
    BatchSetCollection() {}
    virtual ~BatchSetCollection() {
        deleteAllBatches();
    }
    BatchSet* createBatchSet(Command* command, std::shared_ptr<Connection> connection,
                             std::chrono::high_resolution_clock::time_point enqueuedTime, Command& response);
    set<int>* getBatchSetConnectionFds();
    BatchSet* getBatchSet(int id, int64_t connId) {
        string batchId = BatchSet::createBatchId(connId, id);
        MutexLock lock(&mu_);
        unordered_map<string, BatchSet*>::iterator it = batchSets_.find(batchId);
        if (it == batchSets_.end()) {
            return NULL;
        } else {
            return it->second;
        }
    }
    void clockTick(uint64_t seconds);
    bool addCommand(Message* req, Command* cmd, IncomingValueInterface* value,
         int64_t connId, Command& resp);
    bool isBatchCommand(Command* cmd) const {
        Command_MessageType message_type = cmd->header().messagetype();
        return (isBatch(cmd) &&
          (message_type == Command_MessageType_PUT || message_type == Command_MessageType_DELETE ||
           message_type == Command_MessageType_START_BATCH ||
           message_type == Command_MessageType_END_BATCH ||
           message_type == Command_MessageType_ABORT_BATCH));
    }
    bool isCommand(Command* cmd) const {
        Command_MessageType message_type = cmd->header().messagetype();
        return (!(message_type == Command_MessageType_START_BATCH ||
           message_type == Command_MessageType_END_BATCH ||
           message_type == Command_MessageType_ABORT_BATCH));
    }
    bool isBatchableCommand(Command* cmd) const {
        Command_MessageType message_type = cmd->header().messagetype();
        return (isBatch(cmd) &&
           (message_type == Command_MessageType_PUT || message_type == Command_MessageType_DELETE));
    }

    bool isPutBatchCommand(Command* cmd) const {
        Command_MessageType message_type = cmd->header().messagetype();
        return (isBatch(cmd) &&
           (message_type == Command_MessageType_PUT));
    }

    bool isDelBatchCommand(Command* cmd) const {
        Command_MessageType message_type = cmd->header().messagetype();
        return (isBatch(cmd) &&
           (message_type == Command_MessageType_DELETE));
    }
    bool isStartBatchCommand(Command* cmd) const {
        return ( cmd->header().messagetype() == Command_MessageType_START_BATCH);
    }
    bool isAbortBatchCommand(Command* cmd) const {
        return ( cmd->header().messagetype() == Command_MessageType_ABORT_BATCH);
    }
    bool isEndBatchCommand(Command* cmd) const {
        return ( cmd->header().messagetype() == Command_MessageType_END_BATCH);
    }
    bool isBatch(Command* cmd) const {
        return cmd->header().has_batchid();
    }
    void deleteBatchesOnConnection(int64_t connId, bool shuttingDown = false) {
        MutexLock lock(&mu_);
        unordered_map<string, BatchSet*>::iterator it = batchSets_.begin();
        while (it != batchSets_.end())  {
           BatchSet* batchSet = it->second;
           if (connId == batchSet->getConnectionId()) {
               it = batchSets_.erase(it);
               batchSet->unref(shuttingDown);
           } else {
               ++it;
           }
        }
    }
    vector<BatchSet*>* removeAllBatchSetOnConnection(int64_t connId) {
        vector<BatchSet*>* batchSets = new vector<BatchSet*>();
        MutexLock lock(&mu_);
        unordered_map<string, BatchSet*>::iterator it = batchSets_.begin();
        while (it != batchSets_.end()) {
           BatchSet* batchSet = it->second;
           if (batchSet->getConnectionId() == connId) {
               batchSets->push_back(batchSet);
               it = batchSets_.erase(it);
           } else {
               ++it;
           }
        }
        return batchSets;
    }
    vector<BatchSet*>* getAllBatchSetOnConnection(int64_t connId) {
        vector<BatchSet*>* batchSets = new vector<BatchSet*>();
        MutexLock lock(&mu_);
        unordered_map<string, BatchSet*>::iterator it = batchSets_.begin();
        int i= 0;
        for (; it != batchSets_.end(); ++it)  {
           BatchSet* batchSet = it->second;
           if (batchSet->getConnectionId() == connId) {
               batchSet->ref();
               batchSets->push_back(batchSet);
               batchSet->unref();
              i++;
           }
        }
        return batchSets;
    }
    bool hasBatchSetOnConnection(int64_t connId) {
        bool found = false;
        MutexLock lock(&mu_);
        unordered_map<string, BatchSet*>::iterator it = batchSets_.begin();
        for (; !found && it != batchSets_.end(); ++it)  {
           BatchSet* batchSet = it->second;
           if (batchSet->getConnectionId() == connId) {
               found = true;
           }
        }
        return found;
    }
    unordered_map<string, BatchSet*>::iterator begin() {
        return batchSets_.begin();
    }
    unordered_map<string, BatchSet*>::iterator end() {
        return batchSets_.end();
    }
    unordered_map<string, BatchSet*>::iterator deleteBatchSet(string& batchId) {
        MutexLock lock(&mu_);
        unordered_map<string, BatchSet*>::iterator it = batchSets_.find(batchId);
        if (it != batchSets_.end()) {
           BatchSet* batchSet = it->second;
           it = batchSets_.erase(it);
           batchSet->unref();
        }
        return it;
    }
    void putBack(vector<BatchSet*>* bSets) {
        MutexLock lock(&mu_);
        vector<BatchSet*>::iterator it = bSets->begin();
        while (it != bSets->end()) {
           BatchSet* batchSet = *it;
           it = bSets->erase(it);
           batchSets_[batchSet->getId()] = batchSet;
        }
    }
    size_t numberOfBatchsets() {
        return batchSets_.size();
    }

 private:
    void deleteAllBatches();

 private:
    unordered_map<string, BatchSet*> batchSets_;
    port::Mutex mu_;
};

} /* namespace cmd */
} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */

#endif  /* BATCHCOLLECTION_H_ */ //NOLINT
