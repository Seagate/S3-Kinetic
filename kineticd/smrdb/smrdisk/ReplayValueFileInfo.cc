/*
 * ReplayValueFileInfo.cc
 *
 *  Created on: May 5, 2017
 *      Author: tri
 */

#include "ReplayValueFileInfo.h"

namespace smr {
ostream& operator<<(ostream& out, ReplayValueFileInfo& src) {
    out << "{" << src.fileValueInfo_ << "}";
    return out;
}
} /* namespace smr */
