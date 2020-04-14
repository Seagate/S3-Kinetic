/*
 * BatchCmd.cc
 *
 *  Created on: Mar 30, 2016
 *      Author: tri
 */

#include "BatchCmd.h"

#include <string>
#include "internal_value_record.pb.h"
#include "primary_store_interface.h"
#include "BatchSet.h"

using namespace smr; //NOLINT

namespace com {
namespace seagate {
namespace kinetic {
namespace cmd {

bool BatchCmd::toInternalValue(LevelDBData*& internalVal, Command& response) {
    internalVal = new LevelDBData();
    internalVal->type = LevelDBDataType::MEM_INTERNAL;
    internalVal->dataSize = value_->size();
    internalVal->memType = MEMORYType::MEM_FOR_CLIENT;

//    internalVal->data = ValueMemory::getInstance()->allocate(value_->size());
//    if (internalVal->data != NULL) {
//        memcpy(internalVal->data, value_->GetUserValue(), value_->size());
//    } else {
//        delete internalVal;
//        response.mutable_status()->set_code(Command_Status_StatusCode_NO_SPACE);
//        response.mutable_status()->set_statusmessage("Drive is full");
//        return false;
//    }
    internalVal->data = value_->GetUserValue();
    InternalValueRecord internal_value_record;
    Command_KeyValue const& keyValue = cmd_->body().keyvalue();

    //Always has empty value;
    string value_str = "";
    internal_value_record.set_value(value_str);
    internal_value_record.set_version(keyValue.newversion());
    internal_value_record.set_tag(keyValue.tag());
    internal_value_record.set_algorithm((uint32_t)keyValue.algorithm());
    string packed_value;

    if (!internal_value_record.SerializeToString(&packed_value)) {
        free(internalVal->data);
        delete internalVal;
        response.mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
        response.mutable_status()->set_statusmessage("Failed to serialize internal value record.");
        return false;
    };
    internalVal->headerSize = packed_value.size();
    internalVal->header = new char[internalVal->headerSize];
    memcpy(internalVal->header, packed_value.data(), packed_value.size());
    return true;
}

} /* namespace cmd */
} /* namespace kinetic */
} /* namespace seagate */
} /* namespace com */
