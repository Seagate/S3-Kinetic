package cmd

import (
	"errors"
)


type KineticError int
const (
        StoreOperationStatus_SUCCESS  KineticError =  iota
        StoreOperationStatus_NOT_FOUND
        StoreOperationStatus_VERSION_MISMATCH
        StoreOperationStatus_AUTHORIZATION_FAILURE
        StoreOperationStatus_NO_SPACE
        StoreOperationStatus_FROZEN
        StoreOperationStatus_MEDIA_FAULT
        StoreOperationStatus_INTERNAL_ERROR
        StoreOperationStatus_STORE_CORRUPT
        StoreOperationStatus_DATA_CORRUPT
        StoreOperationStatus_BLOCK_POINT_MISMATCH
        StoreOperationSTatus_PRECHECK_FAILED
        StoreOperationStatus_FIRMWARE_INVALID
        StoreOperationStatus_VERSION_FAILURE
        StoreOperationStatus_INVALID_REQUEST
        StoreOperationStatus_ISE_FAILED_VAILD_DB
        StoreOperationStatus_UNSUPPORTABLE
        StoreOperationStatus_SUPERBLOCK_IO
        StoreOperationStatus_EXCEED_LIMIT
        StoreOperationStatus_DUPLICATE_ID
)
        var errKineticNotFound   = errors.New("NOT FOUND")
        var errKineticVersionMismatch = errors.New("Version Mismatch")
        var errKineticAuthorizationFailure = errors.New("Authorization Failure")
        var errKineticNoSpace = errors.New("No Space Left")
        var errKineticFrozen = errors.New("Frozen")
        var errKineticMediaFault = errors.New("Media Fault")
        var errKineticInternalError = errors.New("Internal Error")
        var errKineticStoreCorrupt = errors.New("Store Corrupt")
        var errKineticDataCorrupt = errors.New("Data Corrupt")
        var errKineticBlockPointMismatch = errors.New("Block Point Mismatch")
        var errKineticPrecheckFailed = errors.New("Precheck Failed")
        var errKineticFirmwareInvalid = errors.New("Firmware Invalid")
        var errKineticInvalidError = errors.New("Invalid Error")

func toKineticError(err KineticError) error {
	switch err {
		case StoreOperationStatus_SUCCESS:
			return nil
		case StoreOperationStatus_NOT_FOUND:
			return errKineticNotFound
		case StoreOperationStatus_VERSION_MISMATCH:
			return errKineticVersionMismatch
		case StoreOperationStatus_AUTHORIZATION_FAILURE: 
			return errKineticAuthorizationFailure
		case StoreOperationStatus_NO_SPACE:
			return errKineticNoSpace
		case StoreOperationStatus_FROZEN:
			return errKineticFrozen
		case StoreOperationStatus_MEDIA_FAULT:
			return errKineticMediaFault
	}
	return errKineticInvalidError

}
