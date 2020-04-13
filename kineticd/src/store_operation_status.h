#ifndef KINETIC_STORE_OPERATION_STATUS_H_
#define KINETIC_STORE_OPERATION_STATUS_H_

namespace com {
namespace seagate {
namespace kinetic {
    enum StoreOperationStatus {
        StoreOperationStatus_SUCCESS,
        StoreOperationStatus_NOT_FOUND,
        StoreOperationStatus_VERSION_MISMATCH,
        StoreOperationStatus_AUTHORIZATION_FAILURE,
        StoreOperationStatus_NO_SPACE,
        StoreOperationStatus_FROZEN,
        StoreOperationStatus_MEDIA_FAULT,
        StoreOperationStatus_INTERNAL_ERROR,
        StoreOperationStatus_STORE_CORRUPT,
        StoreOperationStatus_DATA_CORRUPT,
        StoreOperationStatus_BLOCK_POINT_MISMATCH,
        StoreOperationSTatus_PRECHECK_FAILED,
        StoreOperationStatus_FIRMWARE_INVALID,
        StoreOperationStatus_VERSION_FAILURE,
        StoreOperationStatus_INVALID_REQUEST,
        StoreOperationStatus_ISE_FAILED_VAILD_DB,
        StoreOperationStatus_UNSUPPORTABLE,
        StoreOperationStatus_SUPERBLOCK_IO,
        StoreOperationStatus_EXCEED_LIMIT,
        StoreOperationStatus_DUPLICATE_ID
    };
} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_STORE_OPERATION_STATUS_H_
