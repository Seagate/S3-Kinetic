/*
 * BatchSetCollection.cc
 *
 *  Created on: Mar 30, 2016
 *      Author: tri
 */

#include "BatchSetCollection.h"
#include "connection_handler.h"

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {

ostream& operator<<(ostream& os, const BatchSetCollection& batchSetCollection) {
    os << "=== Batch Set Collection: #batches = " << batchSetCollection.batchSets_.size() << endl;
    unordered_map<string, BatchSet*>::const_iterator it = batchSetCollection.batchSets_.begin();
    for (; it != batchSetCollection.batchSets_.end(); ++it) {
        os << *(it->second) << endl;
    }
    os << "=== End of batch set collection" << endl;
    return os;
}
void BatchSetCollection::deleteAllBatches() {
    unordered_map<string, BatchSet*>::iterator it = batchSets_.begin();
    for (; it != batchSets_.end(); ++it) {
        it->second->unref(); //delete it->second;
    }
    batchSets_.clear();
}
bool BatchSetCollection::addCommand(Message* req, Command* cmd, IncomingValueInterface* value,
        int64_t connId, Command& resp) {
    if (!isBatch(cmd)) {
        return false;
    }
    if (!isBatchableCommand(cmd)) {
        resp.mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
        resp.mutable_status()->set_statusmessage("Non-batchable command");
    }
    BatchSet* batchSet = NULL;
    string batchId = BatchSet::createBatchId(connId, cmd->header().batchid());
    bool success = false;
    MutexLock lock(&mu_);
    unordered_map<string, BatchSet*>::iterator it = batchSets_.find(batchId);
    if (it != batchSets_.end()) {
        batchSet = it->second;
        success = batchSet->addCommand(req->authtype(), cmd, value, resp);
    } else {
        resp.mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
        resp.mutable_status()->set_statusmessage("There was no start batch");
    }
    if (success) {
        if (isPutBatchCommand(cmd)) {
            batchSet->setHasPutCmd();
        } else if (isDelBatchCommand(cmd)) {
            batchSet->setHasDelCmd();
        }
    }
    return success;
}

BatchSet* BatchSetCollection::createBatchSet(Command* command, std::shared_ptr<Connection> connection,
                          std::chrono::high_resolution_clock::time_point enqueuedTime, Command& response) {
    int cmdBatchId =  command->header().batchid();
    int64_t connId = connection->id();
    BatchSet* batchSet = NULL;
    string batchId = BatchSet::createBatchId(connId, cmdBatchId);
    MutexLock lock(&mu_);
    unordered_map<string, BatchSet*>::iterator it = batchSets_.find(batchId);
    if (it == batchSets_.end()) {
        batchSet = new BatchSet(command, connection, enqueuedTime, this);
        batchSets_[batchId] = batchSet;
    } else {
        response.mutable_status()->set_code(Command_Status_StatusCode_INVALID_BATCH);
        response.mutable_status()->set_statusmessage("Previous same batch ID still active");
    }
    return batchSet;
}
void BatchSetCollection::clockTick(uint64_t seconds) {
    mu_.Lock();
    unordered_map<string, BatchSet*> batchSetsCopy = batchSets_;
    unordered_map<string, BatchSet*>::iterator it;
    for (it = batchSetsCopy.begin(); it != batchSetsCopy.end(); ++it) {
       if (batchSets_.find(it->first) != batchSets_.end()) {
           // Batch set was not destroyed, call clockTick
           BatchSet* batchSet = it->second;
           batchSet->ref();
           mu_.Unlock();
           batchSet->clockTick(seconds);
           mu_.Lock();
           batchSet->unref();
       }
    }
    mu_.Unlock();
}
set<int>* BatchSetCollection::getBatchSetConnectionFds() {
    MutexLock lock(&mu_);
    set<int>* connFds = new set<int>();
    unordered_map<string, BatchSet*>::const_iterator it = batchSets_.begin();
    for (it = batchSets_.begin(); it != batchSets_.end(); ++it) {
        BatchSet* batchSet = it->second;
        if (batchSet->connFd() >= 0) {
            connFds->insert(batchSet->connFd());
        }
    }
    return connFds;
}


} // namespace cmd
} // namespace kinetic
} // namespace seagate
} // namespace com
