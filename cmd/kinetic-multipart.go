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
// #cgo LDFLAGS -L../lib -lkinetic -lseapubcmds -l:kernel_mem_mgr.a -lssl -lcrypto -lgmock -lgtest -lsmrenv -lleveldb -lmemenv -lkinetic_client -l:zac_kin.a -lprotobuf -lgflags -lpthread -ldl -lrt -lglog

// #include "minio_skinny_waist.h"
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
	key := bucket + "/" + object + "." + fs.metaJSONFile
        // metadata file
        buf := allocateValBuf(len(fsMetaBytes))
        copy(buf, fsMetaBytes)
	//log.Println(" WRITE TO: ", metaKey)
        kineticMutex.Lock()
        kc := GetKineticConnection()
        _, err = kc.CPut(key, buf, len(fsMetaBytes), kopts)
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
	key :=  bucket + "/" + object + "." + fs.encodePartFile(partID, etag, data.ActualSize())
        meta := make(map[string]string)
        fsMeta := newFSMetaV1()
	fsMeta.Meta = meta
        fsMeta.Meta["etag"] = r.MD5CurrentHexString()
        fsMeta.Meta["size"] = strconv.FormatInt(data.Size(), 10)
        fsMeta.KoInfo = KOInfo{Name: object, Size: data.Size(), CreatedTime: time.Now()}
	// metadata file
        bytes, _ := json.Marshal(fsMeta)
        buf := allocateValBuf(len(bytes))
        copy(buf, bytes)
	kineticMutex.Lock()
	kc := GetKineticConnection()
        _, err = kc.CPut(key, goBuf, int(bufSize), kopts)
	if err != nil {
		ReleaseConnection(kc.Idx)
	        kineticMutex.Unlock()
		return pi, err
	}
	_, err = kc.CPutMeta(key, buf, len(bytes), kopts)
        if err != nil {
		ReleaseConnection(kc.Idx)
	        kineticMutex.Unlock()
                return pi, err
        }
        ReleaseConnection(kc.Idx)
	kineticMutex.Unlock()
	//log.Println("END: PutObjectPart", key, bufSize)
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

	startKey := "meta." + bucket + "/" + object + "."
	endKey := common.IncStr(startKey)
        kineticMutex.Lock()
        kc := GetKineticConnection()
        keys, err := kc.CGetKeyRange(startKey, endKey, true, true, 800, false, kopts)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        if err != nil {
                debug.FreeOSMemory()
                return result, toObjectErr(err, bucket)
        }

        partsMap := make(map[int]string)
	//TODO: need to eliminate bucket, and object name
	var Keys [][]byte
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
		stat1, serr := koStat(getPartKO(Keys, partNumber, etag1))
                if serr != nil {
                        return result, toObjectErr(serr)
                }
                //log.Println("GET PART FILE 2",  getPartKO(Keys, partNumber, etag2))
                stat2, serr := koStat(getPartKO(Keys, partNumber, etag2))
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
	    keyPrefix := bucket + "/" + object + "."
        for i, part := range result.Parts {
                var stat KVInfo
                stat, err = koStat(keyPrefix + fs.encodePartFile(part.PartNumber, part.ETag, part.ActualSize))
                if err != nil {
                        return result, toObjectErr(err)
                }
                result.Parts[i].LastModified = stat.ModTime()
                result.Parts[i].Size = part.ActualSize
        }

	key := bucket + "/" + object + "." + fs.metaJSONFile
    common.KTrace(fmt.Sprintf("json file key = %s", key))
	//log.Println(" GET JSON FILE FOR", key)
        kineticMutex.Lock()
        kc = GetKineticConnection()
        cvalue, size, err := kc.CGet(key, 0,  kopts)
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
		if err != nil {
	                kineticMutex.Unlock()
			return result, err
		}
		result.UserDefined = fsMeta.Meta
	}
        //ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
	//log.Println("FSMETA", result.UserDefined)
	return result, nil
}


// CompleteMultipartUpload - completes an ongoing multipart
// transaction after receiving all the parts indicated by the client.
// Returns an md5sum calculated by concatenating all the individual
// md5sums of all the parts.
//
// Implements S3 compatible Complete multipart API.
func (fs *KineticObjects) CompleteMultipartUpload(ctx context.Context, bucket string, object string, uploadID string, parts []CompletePart, opts ObjectOptions) (oi ObjectInfo, e error) {
	//log.Println("CompleteMultipartUpload", parts)
    defer common.KUntrace(common.KTrace("Enter"))
        var actualSize int64

        s3MD5 := getCompleteMultipartMD5(parts)
        partSize := int64(-1) // Used later to ensure that all parts sizes are same.

        fsMeta := fsMetaV1{}

        // Allocate parts similar to incoming slice.
        fsMeta.Parts = make([]ObjectPartInfo, len(parts))
        kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }

        startKey := "meta." + bucket + "/" + object + "."
        endKey := common.IncStr(startKey)
	kineticMutex.Lock()
        kc := GetKineticConnection()
        keys, err := kc.CGetKeyRange(startKey, endKey, true, true, 800, false, kopts)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        if err != nil {
                debug.FreeOSMemory()
                return oi, toObjectErr(err, bucket)
        }
	var Keys [][]byte
        for _, key := range keys {
                k  := key[len(startKey):]
                Keys = append(Keys, k)
        }
        debug.FreeOSMemory()

        // Save consolidated actual size.
        var objectActualSize int64
        // Validate all parts and then commit to disk.
        for i, part := range parts {
                partFile := getPartKO(Keys, part.PartNumber, part.ETag)
		//log.Println("PART FILE ", partFile)
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
                        return oi, InvalidPart{
                                PartNumber: part.PartNumber,
                                GotETag:    part.ETag,
                        }
                }

                var fi KVInfo
                keyPrefix := bucket + "/" + object + "."
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

                fsMeta.Parts[i] = ObjectPartInfo{
                        Number:     part.PartNumber,
                        ETag:       part.ETag,
                        Size:       fi.Size(),
                        ActualSize: actualSize,
                }
		if hiddenMultiParts {
                //DELETE meta data of PARTs so that it will not show up on client "ls" command
			kopts := Opts {
				ClusterVersion:  0,
				Force:           true,
				Tag:             []byte{},
				Algorithm:       kinetic_proto.Command_SHA1,
				Synchronization: kinetic_proto.Command_WRITEBACK,
				Timeout:         60000, //60 sec
				Priority:        kinetic_proto.Command_NORMAL,
			}
            keyPrefix := "meta." + bucket + "/" + object + "."
			metaKey := keyPrefix + getPartKO(Keys, part.PartNumber, part.ETag)
	                kineticMutex.Lock()
			kc := GetKineticConnection()
			kc.Delete(metaKey, kopts)
			ReleaseConnection(kc.Idx)
                        kineticMutex.Unlock()
		}
                // Consolidate the actual size.
                objectActualSize += actualSize
                if i == len(parts)-1 {
                        break
                }

                // All parts except the last part has to be atleast 5MB.
                if !isMinAllowedPartSize(actualSize) {
                        return oi, PartTooSmall{
                                PartNumber: part.PartNumber,
                                PartSize:   actualSize,
                                PartETag:   part.ETag,
                        }
                }
        }

        key := bucket + "/" + object + "." + fs.metaJSONFile
        kineticMutex.Lock()
        kc = GetKineticConnection()
        cvalue, size, err := kc.CGet(key, 0, kopts)
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
	fsMeta.Meta["size"] =  strconv.FormatInt(objectActualSize, 10)
        fsMeta.Meta["etag"] = s3MD5
        // Save consolidated actual size.
        fsMeta.Meta[ReservedMetadataPrefix+"actual-size"] = strconv.FormatInt(objectActualSize, 10)

        //key = bucket + "/" + object + "/" + fs.metaJSONFile
        key = bucket + "/" + object

        bytes, _ := json.Marshal(&fsMeta)
        value := allocateValBuf(len(bytes))
	copy(value, bytes)
        kc = GetKineticConnection()

        _, err = kc.CPut(key, value, 0, kopts)
	if err != nil {
	        ReleaseConnection(kc.Idx)
                kineticMutex.Unlock()
		return oi, err
	}
	//Only Meta data for the object
	_, err = kc.CPutMeta(key, value, len(value), kopts)
	if err != nil {
	        ReleaseConnection(kc.Idx)
                kineticMutex.Unlock()
		return oi, err
	}
	ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        ki := KVInfo{
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

func (fs *KineticObjects) AbortMultipartUpload(ctx context.Context, bucket, object, uploadID string) error {
    defer common.KUntrace(common.KTrace("Enter"))
    startKey := "meta." + bucket + "/" + object + "."
    endKey := common.IncStr(startKey)
    kopts := Opts {
        ClusterVersion:  0,
        Force:           true,
        Tag:             []byte{},
        Algorithm:       kinetic_proto.Command_SHA1,
        Synchronization: kinetic_proto.Command_WRITEBACK,
        Timeout:         60000, //60 sec
        Priority:        kinetic_proto.Command_NORMAL,
    }
    kc := GetKineticConnection()
    keys, err := kc.CGetKeyRange(startKey, endKey, true, true, 800, false, kopts)
    ReleaseConnection(kc.Idx)
    if err != nil {
        return err
    }
    if len(keys) > 0 {
        kineticMutex.Lock()
        kc := GetKineticConnection()
        for _, metaKey := range keys {
            common.KTrace(fmt.Sprintf("metaKey = %s", string(metaKey)))
            kc.Delete(string(metaKey), kopts)
            objKey := metaKey[len("meta."):len(metaKey)]
            common.KTrace(fmt.Sprintf("objKey = %s", string(objKey)))
            kc.Delete(string(objKey), kopts)
        }
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
    }
	return nil
}
