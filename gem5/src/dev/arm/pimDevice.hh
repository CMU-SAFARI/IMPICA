#include "config/pimDevice.hh"

#if (USE_PIM_DEVICE == PIM_DEVICE_BTREE)

#include "pimDevice_btree.hh"

#elif (USE_PIM_DEVICE == PIM_DEVICE_HASH_TABLE)

#include "pimDevice_hashtable.hh"

#elif (USE_PIM_DEVICE == PIM_DEVICE_LINK_LIST)

#include "pimDevice_linklist.hh"

#endif
