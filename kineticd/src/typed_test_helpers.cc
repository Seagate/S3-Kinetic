#include "gtest/gtest.h"

#include "typed_test_helpers.h"
#include "std_map_key_value_store.h"
#include "key_value_store.h"
#include "command_line_flags.h"

using com::seagate::kinetic::KeyValueStoreInterface;
using com::seagate::kinetic::StdMapKeyValueStore;
using com::seagate::kinetic::KeyValueStore;

using testing::Types;

template <>
KeyValueStoreInterface* CreateKeyValueStore<StdMapKeyValueStore>() {
    return new StdMapKeyValueStore();
}

template <>
KeyValueStoreInterface* CreateKeyValueStore<KeyValueStore>() {
    return new KeyValueStore(FLAGS_store_test_partition,
                FLAGS_table_cache_size,
                FLAGS_block_size,
                FLAGS_sst_size);
}

/*
template <>
void CleanUpKeyValueStore<KeyValueStore>() {
    std::stringstream command;
    uint64_t seek_to;
    seek_to = smr::Disk::SUPERBLOCK_0_ADDR/1048576;
    command << "dd if=/dev/zero of=" << FLAGS_store_test_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    std::string system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
    }
    command.str("");
    seek_to = smr::Disk::SUPERBLOCK_1_ADDR/1048576;
    command << "dd if=/dev/zero of=" << FLAGS_store_test_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
    }

    command.str("");
    seek_to = smr::Disk::SUPERBLOCK_2_ADDR/1048576;
    command << "dd if=/dev/zero of=" << FLAGS_store_test_partition << " bs=1048576 count=10 seek=" << seek_to  << " 2>&1"; //NOLINT

    system_command = command.str();
    if (!com::seagate::kinetic::execute_command(system_command)) {
        LOG(ERROR) << "Failed to ISE on ";//NO_SPELL
    }

} */
