/*
 * Zone.cc
 *
 *  Created on: May 19, 2015
 *      Author: tri
 */
#include "Zone.h"
#include "Level.h"

//namespace leveldb {
namespace smr {
bool Zone::isInWriting() {
    return level_->isZoneInWriting(this);
}

void Zone::setAsWritable() {
    level_->setZoneAsWritable(this);
}
void Zone::setAsNotWritable() {
    level_->setZoneAsNotWritable(this);
}
bool Zone::isWritable() {
    return this->level_->isWritable(this);
}


}
//}





