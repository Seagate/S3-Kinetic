/*
 * Util.h
 *
 *  Created on: Apr 21, 2015
 *      Author: tri
 */

#ifndef UTIL_H_
#define UTIL_H_

//namespace leveldb {
namespace smr {

#define ROUNDUP(x, y)  ((((x) + (y) - 1) / (y)) * (y))
#define TRUNCATE(x, y) ((x) - ((x) & ((y) - 1)))

}  // namespace smr
//}  // namespace leveldb



#endif /* UTIL_H_ */
