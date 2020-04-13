#ifndef KINETIC_TYPED_TEST_HELPERS_H_
#define KINETIC_TYPED_TEST_HELPERS_H_

#include "key_value_store_interface.h"

using com::seagate::kinetic::KeyValueStoreInterface;

template <class T>
KeyValueStoreInterface* CreateKeyValueStore();

#endif  // KINETIC_TYPED_TEST_HELPERS_H_
