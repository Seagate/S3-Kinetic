/*
 * BatchCmd.h
 *
 *  Created on: Mar 30, 2016
 *      Author: tri
 */

#ifndef BATCHCMD_H_ //NOLINT
#define BATCHCMD_H_

#include <string>

#include "connection.h"
#include "store_operation_status.h"
#include "leveldb/mydata.h"
#include "kinetic.pb.h"
#include <iostream>

using namespace std; //NOLINT
using namespace kinetic; //NOLINT
using namespace com::seagate::kinetic::proto; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {

class BatchSet;

class BatchCmd {
 public:
    friend ostream& operator<<(ostream& os, BatchCmd& batchCmd) {
        os << batchCmd.cmd_->DebugString();
        return os;
    }

 public:
    BatchCmd(Message_AuthType authType, Command* cmd, IncomingValueInterface* value,
        Command& response,
        BatchSet* batchSet): authType_(authType), value_(NULL), batchSet_(batchSet) {
        cmd_ = new Command(*cmd);
        response_ = new Command(response);
        if (isPut()) {
            value_ = value;
        }
    }
    virtual ~BatchCmd() {
        delete value_;
        delete cmd_;
        delete response_;
    }

    bool toInternalValue(LevelDBData*& internalVal, Command& response);

    bool isPut() {
        return (cmd_->header().messagetype() == Command_MessageType_PUT);
    }
    bool isDelete() {
        return (cmd_->header().messagetype() == Command_MessageType_DELETE);
    }
    const string& getKey() {
        return cmd_->body().keyvalue().key();
    }
    Message_AuthType getAuthType() {
        return authType_;
    }
    Command* getCommand() {
        return cmd_;
    }
    Command* getResponse() {
        return response_;
    }
    void setValue(IncomingValueInterface* val) {
        value_ = val;
    }
    IncomingValueInterface* getValue() {
        return(value_);
    }


 private:
    Message_AuthType authType_;
    Command* cmd_;
    Command* response_;
    IncomingValueInterface* value_;
    BatchSet* batchSet_;
};

} /* namespace cmd */
} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */

#endif // BATCHCMD_H_ //NOLINT
