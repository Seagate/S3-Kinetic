/*
 * MinIO Cloud Storage, (C) 2016, 2017, 2018 MinIO, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package cmd

import (
// #cgo CXXFLAGS: --std=c++0x  -DNDEBUG -DNDEBUGW -DSMR_ENABLED
// #cgo LDFLAGS: -L../lib -lkinetic 

// #include "C_Operations.h"
        "C"
        "unsafe"
	"context"
	"encoding/json"
	"fmt"
	//"log"
	//"io/ioutil"
	//"os"
	//pathutil "path"
	"sort"
	"strconv"
	"strings"
	"time"
	//"github.com/minio/minio/pkg/kinetic"
        "github.com/minio/minio/pkg/kinetic_proto"
        "github.com/minio/minio/cmd/logger"
        "github.com/minio/minio/common"
        "runtime/debug"
        //"log"
	jsoniter "github.com/json-iterator/go"
	//mioutil "github.com/minio/minio/pkg/ioutil"
)

var hiddenMultiParts = true

func (fs *KineticObjects) encodePartFile(partNumber int, etag string, actualSize int64) string {
        //log.Println("encodePartFile", partNumber, etag, actualSize)
        return fmt.Sprintf("%.5d.%s.%d", partNumber, etag, actualSize)
}

// Returns partNumber and etag
func (fs *KineticObjects) decodePartFile(name string) (partNumber int, etag string, actualSize int64, err error) {
        //log.Println("decodePartFile")

        result := strings.Split(name, ".")
        if len(result) != 3 {
                return 0, "", 0, errUnexpected
        }
        partNumber, err = strconv.Atoi(result[0])
        if err != nil {
                return 0, "", 0, errUnexpected
        }
        actualSize, err = strconv.ParseInt(result[2], 10, 64)
        if err != nil {
                return 0, "", 0, errUnexpected
        }
	//log.Println("DECODE PART FILE", partNumber, result[1], actualSize)
        return partNumber, result[1], actualSize, nil
}


func (fs *KineticObjects) backgroundAppend(ctx context.Context, bucket, object, uploadID string) {
        //log.Println("backgroundAppend")
}


func (fs *KineticObjects) ListMultipartUploads(ctx context.Context, bucket, object, keyMarker, uploadIDMarker, delimiter string, maxUploads int) (result ListMultipartsInfo, e error) {
	//log.Println("ListMultipartUploads")

	return result, nil
}


func (fs *KineticObjects) NewMultipartUpload(ctx context.Context, bucket, object string, opts ObjectOptions) (string, error) {
	//log.Println("NewMultipartUpload", bucket, object, opts)
        defer common.KUntrace(common.KTrace("Enter"))
        common.KTrace(fmt.Sprintf("bucket: %s, object: %s", bucket, object))

        if err := checkNewMultipartArgs(ctx, bucket, object, fs); err != nil {
                return "", toObjectErr(err, bucket)
        }
        if _, err := fs.GetBucketInfo(ctx, bucket); err != nil {
                return "", toObjectErr(err, bucket)
        }

        uploadID := mustGetUUID()

        // Initialize fs.json values.
        fsMeta := newFSMetaV1()

        fsMeta.Meta = opts.UserDefined
        fsMeta.Meta["size"] = strconv.FormatInt(0, 10)
        fsMeta.KoInfo = KOInfo{Name: object, Size: 0, CreatedTime: time.Now()}

        fsMetaBytes, err := json.Marshal(fsMeta)
        if err != nil {
                logger.LogIf(ctx, err)
                return "", err
        }
	//log.Println(" META BYTES", string(fsMetaBytes))
	kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{}, //(fsMeta.Meta),
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
	key := bucket + "/" + object + "." + fs.metaJSONFile + "." + uploadID
        // metadata file
        buf := allocateValBuf(len(fsMetaBytes))
        copy(buf, fsMetaBytes)
        kineticMutex.Lock()
        kc := GetKineticConnection()
        _, err = kc.CPut(key, buf, len(buf), buf, len(buf), kopts)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        return uploadID, err
}

// CopyObjectPart - similar to PutObjectPart but reads data from an existing
// object. Internally incoming data is written to '.minio.sys/tmp' location
// and safely renamed to '.minio.sys/multipart' for reach parts.
func (fs *KineticObjects) CopyObjectPart(ctx context.Context, srcBucket, srcObject, dstBucket, dstObject, uploadID string, partID int,
	startOffset int64, length int64, srcInfo ObjectInfo, srcOpts, dstOpts ObjectOptions) (pi PartInfo, e error) {
	//log.Println("CopyObjectPart")
	return pi, nil
}

// an ongoing multipart transaction. Internally incoming data is
// written to '.minio.sys/tmp' location and safely renamed to
// '.minio.sys/multipart' for reach parts.
func (fs *KineticObjects) PutObjectPart(ctx context.Context, bucket, object, uploadID string, partID int, r *PutObjReader, opts ObjectOptions) (pi PartInfo, e error) {
    defer common.KUntrace(common.KTrace("Enter"))
        data := r.Reader
        if err := checkPutObjectPartArgs(ctx, bucket, object, fs); err != nil {
                return pi, toObjectErr(err, bucket)
        }

        if _, err := fs.GetBucketInfo(ctx, bucket); err != nil {
                return pi, toObjectErr(err, bucket)
        }

        // Validate input data size and it can never be less than -1.
        if data.Size() < -1 {
                logger.LogIf(ctx, errInvalidArgument, logger.Application)
                return pi, toObjectErr(errInvalidArgument)
        }

        bufSize := int64(blockSizeV1)
        if size := data.Size(); size > 0 && bufSize > size {
                bufSize = size
        }
        goBuf := allocateValBuf(int(bufSize))
        //Read data to buf
        _, err := readToBuffer(r, goBuf)
        if err != nil {
		//log.Println("READ ERROR")
                C.free(unsafe.Pointer(&goBuf[0]))
                return pi, err
        }
        kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }

        etag := r.MD5CurrentHexString()

        if etag == "" {
                etag = GenETag()
        }
    objKey := fs.makeKey(bucket, object)
    objVer, _ := fs.currentVersion(objKey)
    nxtVers := fs.newVersion(objVer)
    key :=  bucket + "/" + object + "." + nxtVers + "." + fs.encodePartFile(partID, etag, data.ActualSize())
    common.KTrace(fmt.Sprintf("key: %s", key))
        meta := make(map[string]string)
        fsMeta := newFSMetaV1()
	fsMeta.Meta = meta
        fsMeta.Meta["etag"] = r.MD5CurrentHexString()
        fsMeta.Meta["size"] = strconv.FormatInt(data.Size(), 10)
        fsMeta.Meta["hidden"] = "1"

        fsMeta.KoInfo = KOInfo{Name: object, Size: data.Size(), CreatedTime: time.Now()}
	// metadata file
        bytes, _ := json.Marshal(fsMeta)
        buf := allocateValBuf(len(bytes))
        defer C.free(unsafe.Pointer(&buf[0]))
        copy(buf, bytes)
	kineticMutex.Lock()
	kc := GetKineticConnection()
	_, err = kc.CPut(key, buf, len(bytes), goBuf, int(bufSize), kopts)
	if err != nil {
		C.free(unsafe.Pointer(&goBuf[0]))
		ReleaseConnection(kc.Idx)
		kineticMutex.Unlock()
		return pi, err
	}
        ReleaseConnection(kc.Idx)
	kineticMutex.Unlock()
        return PartInfo{
                PartNumber:   partID,
                LastModified: time.Now(),
                ETag:         etag,
                Size:         bufSize,
                ActualSize:   data.ActualSize(),
        }, nil
}


func (fs *KineticObjects) ListObjectParts(ctx context.Context, bucket, object, uploadID string, partNumberMarker, maxParts int, opts ObjectOptions) (result ListPartsInfo, e error) {
    defer common.KUntrace(common.KTrace("Enter"))
        if err := checkListPartsArgs(ctx, bucket, object, fs); err != nil {
                return result, toObjectErr(err)
        }
        result.Bucket = bucket
        result.Object = object
        result.UploadID = uploadID
        result.MaxParts = maxParts
        result.PartNumberMarker = partNumberMarker

        // Check if bucket exists
        if _, err := fs.GetBucketInfo(ctx, bucket); err != nil {
                return result, toObjectErr(err, bucket)
        }

        kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
    objKey := fs.makeKey(bucket, object)
    vers, _ := fs.currentVersion(objKey)
    common.KTrace(fmt.Sprintf("version: %s", vers))
    startKey := bucket + "/" + object + "." + vers + "."
	endKey := common.IncStr(startKey)
    common.KTrace(fmt.Sprintf("startKey: %s, endKey: %s", startKey, endKey))
        kineticMutex.Lock()
        kc := GetKineticConnection()
        keys, err := kc.CGetKeyRange(startKey, endKey, true, false, 800, false, kopts)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        if err != nil {
                debug.FreeOSMemory()
                return result, toObjectErr(err, bucket)
        }

        partsMap := make(map[int]string)
	//TODO: need to eliminate bucket, and object name
	var Keys [][]byte
    //common.KTrace(fmt.Sprintf("keys: %+v", keys))
    for _, key := range keys {
        k  := key[len(startKey):]
		Keys = append(Keys, k)
	}
        debug.FreeOSMemory()

        for _, key := range Keys {
		k := string(key)
		if k == fs.metaJSONFile {
                        continue
                }
                partNumber, etag1, _, derr := fs.decodePartFile(k)

                if derr != nil {
                        // Skip part files whose name don't match expected format. These could be backend filesystem specific files.
                        continue
                }
                etag2, ok := partsMap[partNumber]
		//log.Println(" ETAG1 ETAG2", partNumber, etag1, etag2)
                if !ok {
                    partsMap[partNumber] = etag1
                    continue
                }
                //log.Println("GET PART FILE 1 ",  getPartKO(Keys, partNumber, etag1))
		stat1, serr := koStat(startKey + getPartKO(Keys, partNumber, etag1))
                if serr != nil {
                        return result, toObjectErr(serr)
                }
                //log.Println("GET PART FILE 2",  getPartKO(Keys, partNumber, etag2))
                stat2, serr := koStat(startKey + getPartKO(Keys, partNumber, etag2))
                if serr != nil {
                        return result, toObjectErr(serr)
                }
                if stat1.ModTime().After(stat2.ModTime()) {
                        partsMap[partNumber] = etag1
                }

	}
        var parts []PartInfo
        var actualSize int64
        for partNumber, etag := range partsMap {
                partFile := getPartKO(Keys, partNumber, etag)
                if partFile == "" {
                        return result, InvalidPart{}
                }
                // Read the actualSize from the pathFileName.
                subParts := strings.Split(partFile, ".")
                actualSize, err = strconv.ParseInt(subParts[len(subParts)-1], 10, 64)
                if err != nil {
                        return result, InvalidPart{}
                }
                parts = append(parts, PartInfo{PartNumber: partNumber, ETag: etag, ActualSize: actualSize})
        }
        sort.Slice(parts, func(i int, j int) bool {
                return parts[i].PartNumber < parts[j].PartNumber
        })
        i := 0
        if partNumberMarker != 0 {
                // If the marker was set, skip the entries till the marker.
                for _, part := range parts {
                        i++
                        if part.PartNumber == partNumberMarker {
                                break
                        }
                }
        }
        partsCount := 0
        for partsCount < maxParts && i < len(parts) {
                result.Parts = append(result.Parts, parts[i])
                i++
                partsCount++
        }

        if i < len(parts) {
                result.IsTruncated = true
                if partsCount != 0 {
                        result.NextPartNumberMarker = result.Parts[partsCount-1].PartNumber
                }
        }
	    keyPrefix := bucket + "/" + object + "." + vers + "."
        for i, part := range result.Parts {
                var stat KVInfo
                stat, err = koStat(keyPrefix + fs.encodePartFile(part.PartNumber, part.ETag, part.ActualSize))
                if err != nil {
                        return result, toObjectErr(err)
                }
                result.Parts[i].LastModified = stat.ModTime()
                result.Parts[i].Size = part.ActualSize
        }

	key := bucket + "/" + object + "." + fs.metaJSONFile + "." + uploadID
    common.KTrace(fmt.Sprintf("json file key = %s", key))
	//log.Println(" GET JSON FILE FOR", key)
        kineticMutex.Lock()
        kc = GetKineticConnection()
        cvalue, size, err := kc.CGetMeta(key, kopts)
        defer C.free(unsafe.Pointer(cvalue))
        ReleaseConnection(kc.Idx)
        if err != nil {
            common.KTrace(fmt.Sprintf("err = %+v", err))
            err = errFileNotFound
	    kineticMutex.Unlock()
            return result, err
        }
        var fsMetaBytes []byte
        var fsMeta fsMetaV1
        if (cvalue != nil) {
		fsMetaBytes = (*[1 << 30 ]byte)(unsafe.Pointer(cvalue))[:size:size]
	        var json = jsoniter.ConfigCompatibleWithStandardLibrary
	        err = json.Unmarshal(fsMetaBytes, &fsMeta);
                common.KTrace("Free meta")
                //C.free(unsafe.Pointer(cvalue))
		if err != nil {
	                kineticMutex.Unlock()
			return result, err
		}
		result.UserDefined = fsMeta.Meta
	}
        //ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
	return result, nil
}


// CompleteMultipartUpload - completes an ongoing multipart
// transaction after receiving all the parts indicated by the client.
// Returns an md5sum calculated by concatenating all the individual
// md5sums of all the parts.
//
// Implements S3 compatible Complete multipart API.
func (fs *KineticObjects) CompleteMultipartUpload(ctx context.Context, bucket string, object string, uploadID string, parts []CompletePart, opts ObjectOptions) (oi ObjectInfo, e error) {
    defer common.KUntrace(common.KTrace("Enter"))
    common.KTrace(fmt.Sprintf("bucket: %s, object: %s", bucket, object))
    var actualSize int64

    s3MD5 := getCompleteMultipartMD5(parts)
    partSize := int64(-1) // Used later to ensure that all parts sizes are same.

    fsMeta := fsMetaV1{}

    // Allocate parts similar to incoming slice.
    fsMeta.Parts = make([]ObjectPartInfo, len(parts))
    kopts := Opts {
        ClusterVersion:  0,
        Force:           true,
        Tag:             []byte{},
        Algorithm:       kinetic_proto.Command_SHA1,
        Synchronization: kinetic_proto.Command_WRITEBACK,
        Timeout:         60000, //60 sec
        Priority:        kinetic_proto.Command_NORMAL,
    }
    objKey := fs.makeKey(bucket, object)
    objVer, _ := fs.currentVersion(objKey)
    nxtVer := fs.newVersion(objVer)
    startKey := bucket + "/" + object + "." + nxtVer + "."
    endKey := common.IncStr(startKey)
	kineticMutex.Lock()
    kc := GetKineticConnection()
    keys, err := kc.CGetKeyRange(startKey, endKey, true, false, 800, false, kopts)
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    if err != nil {
        debug.FreeOSMemory()
        return oi, toObjectErr(err, bucket)
    }
    var Keys [][]byte
    for _, key := range keys {
        // key has format meta.bucket/object.ver.partId.etag-1.fsize (where etag = md5?)
        // k has format partId.etag-1.fsize
        k  := key[len(startKey):]
        Keys = append(Keys, k)
    }
    debug.FreeOSMemory()

    // Save consolidated actual size.
    var objectActualSize int64
    // Validate all parts and then commit to disk.
    for i, part := range parts {
        partFile := getPartKO(Keys, part.PartNumber, part.ETag)
        if partFile == "" {
            return oi, InvalidPart{
                    PartNumber: part.PartNumber,
                    GotETag:    part.ETag,
                }
        }

        // Read the actualSize from the pathFileName.
        subParts := strings.Split(partFile, ".")
        actualSize, err = strconv.ParseInt(subParts[len(subParts)-1], 10, 64)
        if err != nil {
            return oi, InvalidPart {
                    PartNumber: part.PartNumber,
                    GotETag:    part.ETag,
                }
        }

        var fi KVInfo
        keyPrefix := bucket + "/" + object + "." + nxtVer + "."
        fi, err := koStat(keyPrefix + getPartKO(Keys, part.PartNumber, part.ETag))
        if err != nil {
            if err == errFileNotFound || err == errFileAccessDenied {
                return oi, InvalidPart{}
            }
            return oi, err
        }
        if partSize == -1 {
            partSize = actualSize
        }

        fsMeta.Parts[i] = ObjectPartInfo {
                Number:     part.PartNumber,
                ETag:       part.ETag,
                Size:       fi.Size(),
                ActualSize: actualSize,
            }
        // Consolidate the actual size.
        objectActualSize += actualSize
        if i == len(parts)-1 {
            break
        }

        // All parts except the last part has to be atleast 5MB.
        if !isMinAllowedPartSize(actualSize) {
            return oi, PartTooSmall {
                    PartNumber: part.PartNumber,
                    PartSize:   actualSize,
                    PartETag:   part.ETag,
                }
        }
    }

    key := bucket + "/" + object + "." + fs.metaJSONFile + "." + uploadID
    kineticMutex.Lock()
    kc = GetKineticConnection()
    cvalue, size, err := kc.CGet(key, -1, kopts, 0, -1) // -1 to indicate it doesn't know the size
    if err == nil {
        // Remove the temporary metaJSONFile
        kc.Delete(key, kopts)
    }
    ReleaseConnection(kc.Idx)
    if err != nil {
        err = errFileNotFound
        kineticMutex.Unlock()
        return oi, err
    }
    var fsMetaBytes []byte
    if (cvalue != nil) {
        fsMetaBytes = (*[1 << 30 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        err = json.Unmarshal(fsMetaBytes[:size], &fsMeta)
        if err != nil {
            kineticMutex.Unlock()
            return oi, err
        }
    }
    // Save additional metadata.
    if len(fsMeta.Meta) == 0 {
        fsMeta.Meta = make(map[string]string)
    }
    fsMeta.Meta["version"] = nxtVer
    fsMeta.Meta["size"] =  strconv.FormatInt(objectActualSize, 10)
    fsMeta.Meta["etag"] = s3MD5
    // Save consolidated actual size.
    fsMeta.Meta[ReservedMetadataPrefix+"actual-size"] = strconv.FormatInt(objectActualSize, 10)
    key = bucket + "/" + object
    bytes, _ := json.Marshal(&fsMeta)
    metaValue := allocateValBuf(len(bytes))
    defer C.free(unsafe.Pointer(&metaValue[0]))
    copy(metaValue, bytes)
    val := allocateValBuf(0)
    kc = GetKineticConnection()
    _, err = kc.CPut(key, metaValue, len(metaValue), val, 0, kopts)
    if err != nil {
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        return oi, err
    }
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    // Delete previous partial objects
    if objVer != "" {
        fs.deleteParts(objKey, objVer)
    }
    ki := KVInfo {
            name:    object,
            size:    objectActualSize,
            modTime: time.Now(),
        }

    return fsMeta.ToObjectKVInfo(bucket, object, ki), nil
}

// AbortMultipartUpload - aborts an ongoing multipart operation
// signified by the input uploadID. This is an atomic operation
// doesn't require clients to initiate multiple such requests.
//
// All parts are purged from all disks and reference to the uploadID
// would be removed from the system, rollback is not possible on this
// operation.
//
// Implements S3 compatible Abort multipart API, slight difference is
// that this is an atomic idempotent operation. Subsequent calls have
// no affect and further requests to the same uploadID would not be
// honored.

func (ko *KineticObjects) AbortMultipartUpload(ctx context.Context, bucket, object, uploadID string) error {
    defer common.KUntrace(common.KTrace("Enter"))
    common.KTrace(fmt.Sprintf("bucket: %s, object: %s", bucket, object))
    // Delete temorary json file.
    // If this file name has version in it then it doesn't have to be deleted separately
    objKey := bucket + SlashSeparator + object
    jsonKey := objKey + "." + ko.metaJSONFile + "." + uploadID
    kineticMutex.Lock()
    kc := GetKineticConnection()
    kc.Delete(jsonKey, ko.option())
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    // Delete all multiparts belong to the new version object
    objVer, _ := ko.currentVersion(objKey)
    nxtVer := ko.newVersion(objVer)
    err := ko.deleteParts(objKey, nxtVer)
    return err
}

/*
// Description:
//  An object/file can be optionally decomposed into smaller parts for storing. We call that
//  decomposed object a multipart object because it has multiple parts. This method copies a
//  multipart object from a source bucket to a destination bucket.
*/
func (ko *KineticObjects) copyMultipartObject(ctx context.Context, srcInfo ObjectInfo,
    dstBucket string, dstObj string) (objInfo ObjectInfo, err error) {
    defer common.KUntrace(common.KTrace("Enter"))
    // Make up common variables
    dstObjKey := ko.makeKey(dstBucket, dstObj)
    dstObjVers, _ := ko.currentVersion(dstObjKey)
    dstObjNxtVers := ko.newVersion(dstObjVers)
    dstOpt := ko.option()
    objMeta := newFSMetaV1()
    objMeta.Parts = make([]ObjectPartInfo, len(srcInfo.Parts))
    // Copy object parts first
    for i, part := range srcInfo.Parts {
        // Make part key
        dstPartKey := dstObjKey + "." + dstObjNxtVers + "." +
                        ko.encodePartFile(part.Number, part.ETag, part.Size)
        // Populate meta data
        metaMap := make(map[string]string)
        meta := newFSMetaV1()
        meta.Meta = metaMap
        meta.Meta["etag"] = part.ETag
        meta.Meta["size"] = strconv.FormatInt(part.Size, 10)
        meta.Meta["hidden"] = "1"
        meta.KoInfo = KOInfo{Name: dstObj, Size: part.Size, CreatedTime: time.Now()}
        bytes, _ := json.Marshal(meta)
        metaBuf := allocateValBuf(len(bytes))
        defer C.free(unsafe.Pointer(&metaBuf[0]))
        copy(metaBuf, bytes)
        // Read source object part into buffer
        valBuf := allocateValBuf(int(part.Size))
        _, err = readToBuffer(srcInfo.PutObjReader, valBuf)
        // Put the source object part into destination
        kineticMutex.Lock()
        kc := GetKineticConnection()
        _, err = kc.CPut(dstPartKey, metaBuf, len(metaBuf), valBuf, int(part.Size), dstOpt)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        if (err != nil) {
            break
        }
        objMeta.Parts[i] = ObjectPartInfo {
            Number:     part.Number,
            ETag:       part.ETag,
            Size:       part.Size,
            ActualSize: part.Size,
        }
    }
    if (err != nil) {
        // Error.  Delete all new object parts
        ko.deleteParts(dstObjKey, dstObjNxtVers)
        return objInfo, err
    }
    // Populate meta data for the new multipart object
    metaMap := make(map[string]string)
    objMeta.Meta = metaMap
    objMeta.Meta["version"] = dstObjNxtVers
    objMeta.Meta["etag"] = srcInfo.ETag
    objMeta.Meta["size"] = strconv.FormatInt(srcInfo.Size, 10)
    objMeta.Meta[ReservedMetadataPrefix+"actual-size"] = strconv.FormatInt(srcInfo.Size, 10)
    objMeta.KoInfo = KOInfo{Name: dstObj, Size: 0, CreatedTime: time.Now()}
    bytes, _ := json.Marshal(objMeta)
    metaBuf := allocateValBuf(len(bytes))
    defer C.free(unsafe.Pointer(&metaBuf[0]))
    copy(metaBuf, bytes)
    // Multipart object has no value
    valBuf := allocateValBuf(0)
    // Put the multipart object into destination
    kineticMutex.Lock()
    kc := GetKineticConnection()
    _, err = kc.CPut(dstObjKey, metaBuf, len(metaBuf), valBuf, 0, dstOpt)
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    if (err == nil) {
        // Finally, succeeded. Delete previous object parts
        ko.deleteParts(dstObjKey, dstObjVers)
        kineticInfo := KVInfo {
            name:    dstObj,
            size:    srcInfo.Size,
            modTime: time.Now(),
        }
        objInfo = objMeta.ToObjectKVInfo(dstBucket, dstObj, kineticInfo)
    } else {
        // Finally, failed. Delete new object parts
        ko.deleteParts(dstObjKey, dstObjNxtVers)
    }

    return objInfo, err
}
