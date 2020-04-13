/*
 * ReplayObj.cc
 *
 *  Created on: Dec 9, 2016
 *      Author: tri
 */

#include "ReplayObj.h"

namespace smr {

ostream& operator<<(ostream& out, ReplayObj& src) {
    out << "(tag " << src.tag_ << ", id " << src.id_ << ", ref " << src.ref_ <<")";
    return out;
}

} /* namespace smr */
