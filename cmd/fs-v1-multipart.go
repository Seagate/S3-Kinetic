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
// #cgo LDFLAGS: libkinetic.a kernel_mem_mgr.a libssl.a libcrypto.a libglog.a libgmock.a libgtest.a libsmrenv.a libleveldb.a libmemenv.a libkinetic_client.a zac_kin.a lldp_kin.a  qual_kin.a libprotobuf.a libgflags.a  libgflags_nothreads.a libprotoc.a libksapi.a libpbkdf.a libapi.a libtransports.a  libseapubcmds.a libapi.a -lpthread -ldl -lrt 
// #include "minio_skinny_waist.h"
        "C"
        "unsafe"
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	pathutil "path"
	"sort"
	"strconv"
	"strings"
	"time"
	"github.com/minio/minio/pkg/kinetic"
        "github.com/minio/minio/pkg/kinetic_proto"
        "github.com/minio/minio/cmd/logger"
        "log"
	jsoniter "github.com/json-iterator/go"
	mioutil "github.com/minio/minio/pkg/ioutil"
)


func (fs *KineticObjects) getUploadIDDir(bucket, object, uploadID string) string {
	//log.Println("K: getUploadIDDir", fs.fsPath, minioMetaMultipartBucket)
        return pathJoin(fs.fsPath, minioMetaMultipartBucket, getSHA256Hash([]byte(pathJoin(bucket, object))), uploadID)
}

// Returns EXPORT/.minio.sys/multipart/SHA256/UPLOADID
func (fs *FSObjects) getUploadIDDir(bucket, object, uploadID string) string {
	//log.Println("getUploadIDDir", fs.fsPath, minioMetaMultipartBucket)
	return pathJoin(fs.fsPath, minioMetaMultipartBucket, getSHA256Hash([]byte(pathJoin(bucket, object))), uploadID)
}

// Returns EXPORT/.minio.sys/multipart/SHA256
func (fs *KineticObjects) getMultipartSHADir(bucket, object string) string {
        //log.Println("getMultipartSHADir", minioMetaMultipartBucket,)
        return pathJoin(fs.fsPath, minioMetaMultipartBucket, getSHA256Hash([]byte(pathJoin(bucket, object))))
}

// Returns EXPORT/.minio.sys/multipart/SHA256
func (fs *FSObjects) getMultipartSHADir(bucket, object string) string {
	//log.Println("getMultipartSHADir", minioMetaMultipartBucket,)
	return pathJoin(fs.fsPath, minioMetaMultipartBucket, getSHA256Hash([]byte(pathJoin(bucket, object))))
}

func (fs *KineticObjects) encodePartFile(partNumber int, etag string, actualSize int64) string {
        //log.Println("encodePartFile", partNumber, etag, actualSize)

        return fmt.Sprintf("%.5d.%s.%d", partNumber, etag, actualSize)
}


// Returns partNumber.etag
func (fs *FSObjects) encodePartFile(partNumber int, etag string, actualSize int64) string {
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



// Returns partNumber and etag
func (fs *FSObjects) decodePartFile(name string) (partNumber int, etag string, actualSize int64, err error) {
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
	return partNumber, result[1], actualSize, nil
}

func (fs *KineticObjects) backgroundAppend(ctx context.Context, bucket, object, uploadID string) {
        //log.Println("backgroundAppend")
}
// Appends parts to an appendFile sequentially.
func (fs *FSObjects) backgroundAppend(ctx context.Context, bucket, object, uploadID string) {
	//log.Println("backgroundAppend")

	fs.appendFileMapMu.Lock()
	logger.GetReqInfo(ctx).AppendTags("uploadID", uploadID)
	file := fs.appendFileMap[uploadID]
	if file == nil {
		file = &fsAppendFile{
			filePath: pathJoin(fs.fsPath, minioMetaTmpBucket, fs.fsUUID, fmt.Sprintf("%s.%s", uploadID, mustGetUUID())),
		}
		fs.appendFileMap[uploadID] = file
	}
	fs.appendFileMapMu.Unlock()

	file.Lock()
	defer file.Unlock()

	// Since we append sequentially nextPartNumber will always be len(file.parts)+1
	nextPartNumber := len(file.parts) + 1
	uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)

	entries, err := readDir(uploadIDDir)
	if err != nil {
		logger.GetReqInfo(ctx).AppendTags("uploadIDDir", uploadIDDir)
		logger.LogIf(ctx, err)
		return
	}
	sort.Strings(entries)

	for _, entry := range entries {
		if entry == fs.metaJSONFile {
			continue
		}
		partNumber, etag, actualSize, err := fs.decodePartFile(entry)
		if err != nil {
			// Skip part files whose name don't match expected format. These could be backend filesystem specific files.
			continue
		}
		if partNumber < nextPartNumber {
			// Part already appended.
			continue
		}
		if partNumber > nextPartNumber {
			// Required part number is not yet uploaded.
			return
		}

		partPath := pathJoin(uploadIDDir, entry)
		err = mioutil.AppendFile(file.filePath, partPath)
		if err != nil {
			reqInfo := logger.GetReqInfo(ctx).AppendTags("partPath", partPath)
			reqInfo.AppendTags("filepath", file.filePath)
			logger.LogIf(ctx, err)
			return
		}

		file.parts = append(file.parts, PartInfo{PartNumber: partNumber, ETag: etag, ActualSize: actualSize})
		nextPartNumber++
	}
}

func (fs *KineticObjects) ListMultipartUploads(ctx context.Context, bucket, object, keyMarker, uploadIDMarker, delimiter string, maxUploads int) (result ListMultipartsInfo, e error) {
	//log.Println("ListMultipartUploads")

	return result, nil
}

// ListMultipartUploads - lists all the uploadIDs for the specified object.
// We do not support prefix based listing.
func (fs *FSObjects) ListMultipartUploads(ctx context.Context, bucket, object, keyMarker, uploadIDMarker, delimiter string, maxUploads int) (result ListMultipartsInfo, e error) {
	//log.Println("1. ListMultipartUploads")

	if err := checkListMultipartArgs(ctx, bucket, object, keyMarker, uploadIDMarker, delimiter, fs); err != nil {
		return result, toObjectErr(err)
	}

	if _, err := fs.statBucketDir(ctx, bucket); err != nil {
		return result, toObjectErr(err, bucket)
	}

	result.MaxUploads = maxUploads
	result.KeyMarker = keyMarker
	result.Prefix = object
	result.Delimiter = delimiter
	result.NextKeyMarker = object
	result.UploadIDMarker = uploadIDMarker

	uploadIDs, err := readDir(fs.getMultipartSHADir(bucket, object))
	if err != nil {
		if err == errFileNotFound {
			result.IsTruncated = false
			return result, nil
		}
		logger.LogIf(ctx, err)
		return result, toObjectErr(err)
	}

	// S3 spec says uploadIDs should be sorted based on initiated time. ModTime of fs.json
	// is the creation time of the uploadID, hence we will use that.
	var uploads []MultipartInfo
	for _, uploadID := range uploadIDs {
		metaFilePath := pathJoin(fs.getMultipartSHADir(bucket, object), uploadID, fs.metaJSONFile)
		fi, err := fsStatFile(ctx, metaFilePath)
		if err != nil {
			return result, toObjectErr(err, bucket, object)
		}
		uploads = append(uploads, MultipartInfo{
			Object:    object,
			UploadID:  strings.TrimSuffix(uploadID, SlashSeparator),
			Initiated: fi.ModTime(),
		})
	}
	sort.Slice(uploads, func(i int, j int) bool {
		return uploads[i].Initiated.Before(uploads[j].Initiated)
	})

	uploadIndex := 0
	if uploadIDMarker != "" {
		for uploadIndex < len(uploads) {
			if uploads[uploadIndex].UploadID != uploadIDMarker {
				uploadIndex++
				continue
			}
			if uploads[uploadIndex].UploadID == uploadIDMarker {
				uploadIndex++
				break
			}
			uploadIndex++
		}
	}
	for uploadIndex < len(uploads) {
		result.Uploads = append(result.Uploads, uploads[uploadIndex])
		result.NextUploadIDMarker = uploads[uploadIndex].UploadID
		uploadIndex++
		if len(result.Uploads) == maxUploads {
			break
		}
	}

	result.IsTruncated = uploadIndex < len(uploads)

	if !result.IsTruncated {
		result.NextKeyMarker = ""
		result.NextUploadIDMarker = ""
	}

	return result, nil
}

func (fs *KineticObjects) NewMultipartUpload(ctx context.Context, bucket, object string, opts ObjectOptions) (string, error) {
	//log.Println("NewMultipartUpload")

        if err := checkNewMultipartArgs(ctx, bucket, object, fs); err != nil {
                return "", toObjectErr(err, bucket)
        }
        if _, err := fs.GetBucketInfo(ctx, bucket); err != nil {
                return "", toObjectErr(err, bucket)
        }

        uploadID := mustGetUUID()
        //uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)
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

	kopts := kinetic.CmdOpts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{}, //(fsMeta.Meta),
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
	key := bucket + "/" + object + "/" + fs.metaJSONFile
        // metadata file
	metaKey := "meta." + key
        buf := allocateValBuf(len(fsMetaBytes))
        copy(buf, fsMetaBytes)
	//log.Println(" WRITE TO: ", metaKey)
        kc := GetKineticConnection()
        kc.Put(metaKey, buf, len(fsMetaBytes), kopts) 
        ReleaseConnection(kc.Idx)
        return uploadID, nil
}

// NewMultipartUpload - initialize a new multipart upload, returns a
// unique id. The unique id returned here is of UUID form, for each
// subsequent request each UUID is unique.
//
// Implements S3 compatible initiate multipart API.
func (fs *FSObjects) NewMultipartUpload(ctx context.Context, bucket, object string, opts ObjectOptions) (string, error) {
	//log.Println("1. NewMultipartUpload")

	if err := checkNewMultipartArgs(ctx, bucket, object, fs); err != nil {
		return "", toObjectErr(err, bucket)
	}

	if _, err := fs.statBucketDir(ctx, bucket); err != nil {
		return "", toObjectErr(err, bucket)
	}

	uploadID := mustGetUUID()
	uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)

	err := mkdirAll(uploadIDDir, 0755)
	if err != nil {
		logger.LogIf(ctx, err)
		return "", err
	}

	// Initialize fs.json values.
	fsMeta := newFSMetaV1()
	fsMeta.Meta = opts.UserDefined

	fsMetaBytes, err := json.Marshal(fsMeta)
	if err != nil {
		logger.LogIf(ctx, err)
		return "", err
	}
	//log.Println(" WRITE TO FILE ", pathJoin(uploadIDDir, fs.metaJSONFile))
	if err = ioutil.WriteFile(pathJoin(uploadIDDir, fs.metaJSONFile), fsMetaBytes, 0644); err != nil {
		logger.LogIf(ctx, err)
		return "", err
	}

	return uploadID, nil
}

// CopyObjectPart - similar to PutObjectPart but reads data from an existing
// object. Internally incoming data is written to '.minio.sys/tmp' location
// and safely renamed to '.minio.sys/multipart' for reach parts.
func (fs *KineticObjects) CopyObjectPart(ctx context.Context, srcBucket, srcObject, dstBucket, dstObject, uploadID string, partID int,
	startOffset int64, length int64, srcInfo ObjectInfo, srcOpts, dstOpts ObjectOptions) (pi PartInfo, e error) {
	//log.Println("CopyObjectPart")

	return pi, nil
}

func (fs *FSObjects) CopyObjectPart(ctx context.Context, srcBucket, srcObject, dstBucket, dstObject, uploadID string, partID int,
	startOffset int64, length int64, srcInfo ObjectInfo, srcOpts, dstOpts ObjectOptions) (pi PartInfo, e error) {
	//log.Println("1. CopyObjectPart")

	if err := checkNewMultipartArgs(ctx, srcBucket, srcObject, fs); err != nil {
		return pi, toObjectErr(err)
	}

	partInfo, err := fs.PutObjectPart(ctx, dstBucket, dstObject, uploadID, partID, srcInfo.PutObjReader, dstOpts)
	if err != nil {
		return pi, toObjectErr(err, dstBucket, dstObject)
	}

	return partInfo, nil
}

// PutObjectPart - reads incoming data until EOF for the part file on
// an ongoing multipart transaction. Internally incoming data is
// written to '.minio.sys/tmp' location and safely renamed to
// '.minio.sys/multipart' for reach parts.
func (fs *KineticObjects) PutObjectPart(ctx context.Context, bucket, object, uploadID string, partID int, r *PutObjReader, opts ObjectOptions) (pi PartInfo, e error) {
	//log.Println("PutObjectPart", bucket, object, partID, uploadID)

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

        //uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)
	//fmt.Println(" uploadIDDir", uploadIDDir)
        // Just check if the uploadID exists to avoid copy if it doesn't.
/*
        _, err := fsStatFile(ctx, pathJoin(uploadIDDir, fs.metaJSONFile))
        if err != nil {
                if err == errFileNotFound || err == errFileAccessDenied {
                        return pi, InvalidUploadID{UploadID: uploadID}
                }
                return pi, toObjectErr(err, bucket, object)
        }
*/
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
        kopts := kinetic.CmdOpts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{}, //(fsMeta.Meta),
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }

        etag := r.MD5CurrentHexString()

        if etag == "" {
                etag = GenETag()
        }
	key :=  bucket + "/" + object + "/" +  fs.encodePartFile(partID, etag, data.ActualSize())
        meta := make(map[string]string)
        fsMeta := newFSMetaV1()
	fsMeta.Meta = meta
        fsMeta.Meta["etag"] = r.MD5CurrentHexString()
        fsMeta.Meta["size"] = strconv.FormatInt(data.Size(), 10)
        fsMeta.KoInfo = KOInfo{Name: object, Size: data.Size(), CreatedTime: time.Now()}
	// metadata file
        bytes, _ := json.Marshal(fsMeta)
        metakey := "meta." +  key
        buf := allocateValBuf(len(bytes))
        copy(buf, bytes)


	kc := GetKineticConnection()
        kc.Put(key, goBuf, int(bufSize), kopts)
	kc.Put(metakey, buf, len(bytes), kopts)
        ReleaseConnection(kc.Idx)
	////log.Println("END: PutObjectPart", key, bufSize)
        return PartInfo{
                PartNumber:   partID,
                LastModified: time.Now(),
                ETag:         etag,
                Size:         bufSize,
                ActualSize:   data.ActualSize(),
        }, nil
}

func (fs *FSObjects) PutObjectPart(ctx context.Context, bucket, object, uploadID string, partID int, r *PutObjReader, opts ObjectOptions) (pi PartInfo, e error) {
	////log.Println("1. PutObjectPart")
	data := r.Reader
	if err := checkPutObjectPartArgs(ctx, bucket, object, fs); err != nil {
		return pi, toObjectErr(err, bucket)
	}

	if _, err := fs.statBucketDir(ctx, bucket); err != nil {
		return pi, toObjectErr(err, bucket)
	}

	// Validate input data size and it can never be less than -1.
	if data.Size() < -1 {
		logger.LogIf(ctx, errInvalidArgument, logger.Application)
		return pi, toObjectErr(errInvalidArgument)
	}

	uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)

	// Just check if the uploadID exists to avoid copy if it doesn't.
	_, err := fsStatFile(ctx, pathJoin(uploadIDDir, fs.metaJSONFile))
	if err != nil {
		if err == errFileNotFound || err == errFileAccessDenied {
			return pi, InvalidUploadID{UploadID: uploadID}
		}
		return pi, toObjectErr(err, bucket, object)
	}

	bufSize := int64(blockSizeV1)
	if size := data.Size(); size > 0 && bufSize > size {
		bufSize = size
	}
	buf := make([]byte, bufSize)

	tmpPartPath := pathJoin(fs.fsPath, minioMetaTmpBucket, fs.fsUUID, uploadID+"."+mustGetUUID()+"."+strconv.Itoa(partID))
	////log.Println("TEMP PART PATH ", tmpPartPath)
	bytesWritten, err := fsCreateFile(ctx, tmpPartPath, data, buf, data.Size())
	if err != nil {
		fsRemoveFile(ctx, tmpPartPath)
		return pi, toObjectErr(err, minioMetaTmpBucket, tmpPartPath)
	}

	// Should return IncompleteBody{} error when reader has fewer
	// bytes than specified in request header.
	if bytesWritten < data.Size() {
		fsRemoveFile(ctx, tmpPartPath)
		return pi, IncompleteBody{}
	}

	// Delete temporary part in case of failure. If
	// PutObjectPart succeeds then there would be nothing to
	// delete in which case we just ignore the error.
	defer fsRemoveFile(ctx, tmpPartPath)
	etag := r.MD5CurrentHexString()

	if etag == "" {
		etag = GenETag()
	}

	partPath := pathJoin(uploadIDDir, fs.encodePartFile(partID, etag, data.ActualSize()))
	////log.Println(" PART PATH: ", partPath)
	// Make sure not to create parent directories if they don't exist - the upload might have been aborted.
	if err = fsSimpleRenameFile(ctx, tmpPartPath, partPath); err != nil {
		if err == errFileNotFound || err == errFileAccessDenied {
			return pi, InvalidUploadID{UploadID: uploadID}
		}
		return pi, toObjectErr(err, minioMetaMultipartBucket, partPath)
	}

	go fs.backgroundAppend(ctx, bucket, object, uploadID)

	fi, err := fsStatFile(ctx, partPath)
	if err != nil {
		return pi, toObjectErr(err, minioMetaMultipartBucket, partPath)
	}
	return PartInfo{
		PartNumber:   partID,
		LastModified: fi.ModTime(),
		ETag:         etag,
		Size:         fi.Size(),
		ActualSize:   data.ActualSize(),
	}, nil
}

func (fs *KineticObjects) ListObjectParts(ctx context.Context, bucket, object, uploadID string, partNumberMarker, maxParts int, opts ObjectOptions) (result ListPartsInfo, e error) {
	//log.Println("ListObjectParts", bucket, object, uploadID, partNumberMarker, maxParts)
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

//        uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)
/*        if _, err := fsStatFile(ctx, pathJoin(uploadIDDir, fs.metaJSONFile)); err != nil {
                if err == errFileNotFound || err == errFileAccessDenied {
                        return result, InvalidUploadID{UploadID: uploadID}
                }
                return result, toObjectErr(err, bucket, object)
        }

        entries, err := readDir(uploadIDDir)
        if err != nil {
                logger.LogIf(ctx, err)
                return result, toObjectErr(err, bucket)
        }

*/
        kopts := kinetic.CmdOpts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }

	startKey := "meta." + bucket + "/" + object
	endKey := startKey +"0" 
	log.Println(" START/END KEY", startKey, endKey)
        kc := GetKineticConnection()
        keys, err := kc.GetKeyRange(startKey, endKey, true, true, 800, false, kopts)
        if err != nil {
		//log.Println("ERROR", err)
                ReleaseConnection(kc.Idx)
                //kineticMutex.Unlock()
                return result, toObjectErr(err, bucket)
        }

        partsMap := make(map[int]string)
	//TODO: need to eliminate bucket, and object name
	var Keys [][]byte
        for _, key := range keys {
                k  := key[len("meta.") + len(bucket) + len(object) + 2:]
		Keys = append(Keys, k)
	}	
	
        for _, key := range Keys {
		//k  := string(key[len("meta.") + len(bucket) + len(object) + 2:])
		k := string(key)
                //log.Println("ENTRY: ", k)
		if k == fs.metaJSONFile {
			//log.Println("***** CONTINUE******")
                        continue
                }
                partNumber, etag1, _, derr := fs.decodePartFile(k)
                        //log.Println("***** PART******", partNumber, etag1)

                if derr != nil {
                        // Skip part files whose name don't match expected format. These could be backend filesystem specific files.
                        continue
                }
                etag2, ok := partsMap[partNumber]
		//log.Println(" ETAG1 ETAG2", partNumber, etag1, etag2)
                if !ok {
			//log.Println("NOT OK")
                	partsMap[partNumber] = etag1
                	continue
                }
                log.Println("GET PART FILE 1 ",  getPartKO(Keys, partNumber, etag1))
		stat1, serr := koStat(getPartKO(keys, partNumber, etag1))
                if serr != nil {
                        return result, toObjectErr(serr)
                }
                //log.Println("GET PART FILE 2",  getPartKO(keys, partNumber, etag2))
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
        for i, part := range result.Parts {
                var stat KVInfo
                stat, err = koStat(bucket+ "/" + object + "/" + fs.encodePartFile(part.PartNumber, part.ETag, part.ActualSize))
                if err != nil {
                        return result, toObjectErr(err)
                }
                result.Parts[i].LastModified = stat.ModTime()
                result.Parts[i].Size = part.ActualSize
        }
/*
        fsMetaBytes, err := ioutil.ReadFile(pathJoin(uploadIDDir, fs.metaJSONFile))
        if err != nil {
                logger.LogIf(ctx, err)
                return result, err
        }
*/

	metaKey := "meta." + bucket + "/" + object + "/" + fs.metaJSONFile
	log.Println(" GET JSON FILE", metaKey)
        //kineticMutex.Lock()
        kc = GetKineticConnection()
        //var ptr *C.char
        //size, err := kc.Get(metakey, value, kopts)
        cvalue, ptr, size, err := kc.CGet(metaKey, kopts)
        var fsMetaBytes []byte
        if (cvalue != nil) {
            fsMetaBytes = (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        }
        ReleaseConnection(kc.Idx)
        //kineticMutex.Unlock()
        if err != nil {
                err = errFileNotFound 
                C.deallocate_gvalue_buffer((*C.char)(ptr))
		return result, err
	}

        var fsMeta fsMetaV1
        var json = jsoniter.ConfigCompatibleWithStandardLibrary
        if err = json.Unmarshal(fsMetaBytes, &fsMeta); err != nil {
                C.deallocate_gvalue_buffer((*C.char)(ptr))
                return result, err
        }
        C.deallocate_gvalue_buffer((*C.char)(ptr))

        result.UserDefined = fsMeta.Meta
	log.Println("FSMETA", result.UserDefined)

	return result, nil
}

// ListObjectParts - lists all previously uploaded parts for a given
// object and uploadID.  Takes additional input of part-number-marker
// to indicate where the listing should begin from.
//
// Implements S3 compatible ListObjectParts API. The resulting
// ListPartsInfo structure is unmarshalled directly into XML and
// replied back to the client.
func (fs *FSObjects) ListObjectParts(ctx context.Context, bucket, object, uploadID string, partNumberMarker, maxParts int, opts ObjectOptions) (result ListPartsInfo, e error) {
	log.Println("1. ListObjectParts")

	if err := checkListPartsArgs(ctx, bucket, object, fs); err != nil {
		return result, toObjectErr(err)
	}
	result.Bucket = bucket
	result.Object = object
	result.UploadID = uploadID
	result.MaxParts = maxParts
	result.PartNumberMarker = partNumberMarker

	// Check if bucket exists
	if _, err := fs.statBucketDir(ctx, bucket); err != nil {
		return result, toObjectErr(err, bucket)
	}

	uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)
	if _, err := fsStatFile(ctx, pathJoin(uploadIDDir, fs.metaJSONFile)); err != nil {
		if err == errFileNotFound || err == errFileAccessDenied {
			return result, InvalidUploadID{UploadID: uploadID}
		}
		return result, toObjectErr(err, bucket, object)
	}
	log.Println(" UPLOADIDDIR", uploadIDDir)
	entries, err := readDir(uploadIDDir)
	log.Println("1. UPLOADIDDIR", entries)
	if err != nil {
		logger.LogIf(ctx, err)
		return result, toObjectErr(err, bucket)
	}

	partsMap := make(map[int]string)
	for _, entry := range entries {
		log.Println("ENTRY: ", entry)
		if entry == fs.metaJSONFile {
			continue
		}
		partNumber, etag1, _, derr := fs.decodePartFile(entry)
		if derr != nil {
			// Skip part files whose name don't match expected format. These could be backend filesystem specific files.
			continue
		}
		etag2, ok := partsMap[partNumber]
                log.Println(" ETAG1 ETAG2", partNumber, etag1, etag2)

		if !ok {
			partsMap[partNumber] = etag1
			continue
		}
		log.Println("GET PART FILE 1 ",  getPartFile(entries, partNumber, etag1))
		stat1, serr := fsStatFile(ctx, pathJoin(uploadIDDir, getPartFile(entries, partNumber, etag1)))
		if serr != nil {
			return result, toObjectErr(serr)
		}
                log.Println("GET PART FILE 2 ",  getPartFile(entries, partNumber, etag2))

		stat2, serr := fsStatFile(ctx, pathJoin(uploadIDDir, getPartFile(entries, partNumber, etag2)))
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
		partFile := getPartFile(entries, partNumber, etag)
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
	for i, part := range result.Parts {
		var stat os.FileInfo
		stat, err = fsStatFile(ctx, pathJoin(uploadIDDir,
			fs.encodePartFile(part.PartNumber, part.ETag, part.ActualSize)))
		if err != nil {
			return result, toObjectErr(err)
		}
		result.Parts[i].LastModified = stat.ModTime()
		result.Parts[i].Size = part.ActualSize
	}

	fsMetaBytes, err := ioutil.ReadFile(pathJoin(uploadIDDir, fs.metaJSONFile))
	if err != nil {
		logger.LogIf(ctx, err)
		return result, err
	}

	var fsMeta fsMetaV1
	var json = jsoniter.ConfigCompatibleWithStandardLibrary
	if err = json.Unmarshal(fsMetaBytes, &fsMeta); err != nil {
		return result, err
	}

	result.UserDefined = fsMeta.Meta
	return result, nil
}

// CompleteMultipartUpload - completes an ongoing multipart
// transaction after receiving all the parts indicated by the client.
// Returns an md5sum calculated by concatenating all the individual
// md5sums of all the parts.
//
// Implements S3 compatible Complete multipart API.
func (fs *KineticObjects) CompleteMultipartUpload(ctx context.Context, bucket string, object string, uploadID string, parts []CompletePart, opts ObjectOptions) (oi ObjectInfo, e error) {
	log.Println("CompleteMultipartUpload", parts)
        var actualSize int64

        s3MD5 := getCompleteMultipartMD5(parts)
        log.Println("Complete MD5", s3MD5)
        partSize := int64(-1) // Used later to ensure that all parts sizes are same.

        fsMeta := fsMetaV1{}

        // Allocate parts similar to incoming slice.
        fsMeta.Parts = make([]ObjectPartInfo, len(parts))
        kopts := kinetic.CmdOpts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }

        startKey := "meta." + bucket + "/" + object
        endKey := startKey +"0" 
        log.Println(" START/END KEY", startKey, endKey)
        kc := GetKineticConnection()
        keys, err := kc.GetKeyRange(startKey, endKey, true, true, 800, false, kopts)
        if err != nil {
                ReleaseConnection(kc.Idx)
                return oi, toObjectErr(err, bucket)
        }
	var Keys [][]byte
        for _, key := range keys {
                k  := key[len("meta.") + len(bucket) + len(object) + 2:]
                Keys = append(Keys, k)
        }       

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
                        return oi, InvalidPart{
                                PartNumber: part.PartNumber,
                                GotETag:    part.ETag,
                        }
                }

                var fi KVInfo
                fi, err := koStat(bucket+ "/" + object + "/" + getPartKO(Keys, part.PartNumber, part.ETag))
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


        metaKey := "meta." + bucket + "/" + object + "/" + fs.metaJSONFile
        log.Println(" 1. GET JSON FILE", metaKey)
        //kineticMutex.Lock()
        kc = GetKineticConnection()
        //var ptr *C.char
        //size, err := kc.Get(metakey, value, kopts)
        cvalue, ptr, size, err := kc.CGet(metaKey, kopts)
        var fsMetaBytes []byte
        if (cvalue != nil) {
            fsMetaBytes = (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        }
        ReleaseConnection(kc.Idx)

        //kineticMutex.Unlock()
        if err != nil {
                err = errFileNotFound 
                C.deallocate_gvalue_buffer((*C.char)(ptr))
                return oi, err
        }
        err = json.Unmarshal(fsMetaBytes[:size], &fsMeta)
        // Save additional metadata.
        if len(fsMeta.Meta) == 0 {
                fsMeta.Meta = make(map[string]string)
        }
        fsMeta.Meta["etag"] = s3MD5
        // Save consolidated actual size.
        fsMeta.Meta[ReservedMetadataPrefix+"actual-size"] = strconv.FormatInt(objectActualSize, 10)
        //if _, err = fsMeta.WriteTo(metaFile); err != nil {
        //        logger.LogIf(ctx, err)
        //        return oi, toObjectErr(err, bucket, object)
        //}

        metaKey = "meta." + bucket + "/" + object + "/" + fs.metaJSONFile
        bytes, _ := json.Marshal(&fsMeta)
        value := allocateValBuf(len(bytes))
	copy(value, bytes)
        kc = GetKineticConnection()
        kc.Put(metaKey, value, len(value), kopts)
        ReleaseConnection(kc.Idx)

	log.Println("END: CompleteMultiParts", fsMeta)
	return oi, nil
}

func (fs *FSObjects) CompleteMultipartUpload(ctx context.Context, bucket string, object string, uploadID string, parts []CompletePart, opts ObjectOptions) (oi ObjectInfo, e error) {
	log.Println("1. CompleteMultipartUpload", parts)
	var actualSize int64

	if err := checkCompleteMultipartArgs(ctx, bucket, object, fs); err != nil {
		return oi, toObjectErr(err)
	}

	// Check if an object is present as one of the parent dir.
	if fs.parentDirIsObject(ctx, bucket, pathutil.Dir(object)) {
		return oi, toObjectErr(errFileParentIsFile, bucket, object)
	}

	if _, err := fs.statBucketDir(ctx, bucket); err != nil {
		return oi, toObjectErr(err, bucket)
	}

	uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)
	// Just check if the uploadID exists to avoid copy if it doesn't.
	_, err := fsStatFile(ctx, pathJoin(uploadIDDir, fs.metaJSONFile))
	if err != nil {
		if err == errFileNotFound || err == errFileAccessDenied {
			return oi, InvalidUploadID{UploadID: uploadID}
		}
		return oi, toObjectErr(err, bucket, object)
	}

	// Calculate s3 compatible md5sum for complete multipart.
	s3MD5 := getCompleteMultipartMD5(parts)
	log.Println(" COMPLETE MD5 ", s3MD5)
	partSize := int64(-1) // Used later to ensure that all parts sizes are same.

	fsMeta := fsMetaV1{}

	// Allocate parts similar to incoming slice.
	fsMeta.Parts = make([]ObjectPartInfo, len(parts))

	entries, err := readDir(uploadIDDir)
	if err != nil {
		logger.GetReqInfo(ctx).AppendTags("uploadIDDir", uploadIDDir)
		logger.LogIf(ctx, err)
		return oi, err
	}

	// ensure that part ETag is canonicalized to strip off extraneous quotes
	for i := range parts {
		parts[i].ETag = canonicalizeETag(parts[i].ETag)
	}

	// Save consolidated actual size.
	var objectActualSize int64
	// Validate all parts and then commit to disk.
	for i, part := range parts {
		partFile := getPartFile(entries, part.PartNumber, part.ETag)
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

		partPath := pathJoin(uploadIDDir, partFile)

		var fi os.FileInfo
		fi, err = fsStatFile(ctx, partPath)
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

	appendFallback := true // In case background-append did not append the required parts.
	appendFilePath := pathJoin(fs.fsPath, minioMetaTmpBucket, fs.fsUUID, fmt.Sprintf("%s.%s", uploadID, mustGetUUID()))

	// Most of the times appendFile would already be fully appended by now. We call fs.backgroundAppend()
	// to take care of the following corner case:
	// 1. The last PutObjectPart triggers go-routine fs.backgroundAppend, this go-routine has not started yet.
	// 2. Now CompleteMultipartUpload gets called which sees that lastPart is not appended and starts appending
	//    from the beginning
	//log.Println("Calling backgroundAppend")
	fs.backgroundAppend(ctx, bucket, object, uploadID)

	fs.appendFileMapMu.Lock()
	file := fs.appendFileMap[uploadID]
	delete(fs.appendFileMap, uploadID)
	fs.appendFileMapMu.Unlock()

	if file != nil {
		file.Lock()
		defer file.Unlock()
		// Verify that appendFile has all the parts.
		if len(file.parts) == len(parts) {
			for i := range parts {
				if parts[i].ETag != file.parts[i].ETag {
					break
				}
				if parts[i].PartNumber != file.parts[i].PartNumber {
					break
				}
				if i == len(parts)-1 {
					appendFilePath = file.filePath
					appendFallback = false
				}
			}
		}
	}

	if appendFallback {
		if file != nil {
			fsRemoveFile(ctx, file.filePath)
		}
		for _, part := range parts {
			partPath := getPartFile(entries, part.PartNumber, part.ETag)
			if err = mioutil.AppendFile(appendFilePath, pathJoin(uploadIDDir, partPath)); err != nil {
				logger.LogIf(ctx, err)
				return oi, toObjectErr(err)
			}
		}
	}

	// Hold write lock on the object.
	destLock := fs.NewNSLock(ctx, bucket, object)
	if err = destLock.GetLock(globalObjectTimeout); err != nil {
		return oi, err
	}
	defer destLock.Unlock()
	fsMetaPath := pathJoin(fs.fsPath, minioMetaBucket, bucketMetaPrefix, bucket, object, fs.metaJSONFile)
	metaFile, err := fs.rwPool.Create(fsMetaPath)
	if err != nil {
		logger.LogIf(ctx, err)
		return oi, toObjectErr(err, bucket, object)
	}
	defer metaFile.Close()

	// Read saved fs metadata for ongoing multipart.
	fsMetaBuf, err := ioutil.ReadFile(pathJoin(uploadIDDir, fs.metaJSONFile))
	if err != nil {
		logger.LogIf(ctx, err)
		return oi, toObjectErr(err, bucket, object)
	}
	err = json.Unmarshal(fsMetaBuf, &fsMeta)
	if err != nil {
		logger.LogIf(ctx, err)
		return oi, toObjectErr(err, bucket, object)
	}
	// Save additional metadata.
	if len(fsMeta.Meta) == 0 {
		fsMeta.Meta = make(map[string]string)
	}
	fsMeta.Meta["etag"] = s3MD5
	// Save consolidated actual size.
	fsMeta.Meta[ReservedMetadataPrefix+"actual-size"] = strconv.FormatInt(objectActualSize, 10)
	if _, err = fsMeta.WriteTo(metaFile); err != nil {
		logger.LogIf(ctx, err)
		return oi, toObjectErr(err, bucket, object)
	}

	// Deny if WORM is enabled
	if globalWORMEnabled {
		if _, err := fsStatFile(ctx, pathJoin(fs.fsPath, bucket, object)); err == nil {
			return ObjectInfo{}, ObjectAlreadyExists{Bucket: bucket, Object: object}
		}
	}

	err = fsRenameFile(ctx, appendFilePath, pathJoin(fs.fsPath, bucket, object))
	if err != nil {
		logger.LogIf(ctx, err)
		return oi, toObjectErr(err, bucket, object)
	}
	fsRemoveAll(ctx, uploadIDDir)
	// It is safe to ignore any directory not empty error (in case there were multiple uploadIDs on the same object)
	fsRemoveDir(ctx, fs.getMultipartSHADir(bucket, object))
	fi, err := fsStatFile(ctx, pathJoin(fs.fsPath, bucket, object))
	if err != nil {
		return oi, toObjectErr(err, bucket, object)
	}

	return fsMeta.ToObjectInfo(bucket, object, fi), nil
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
	return nil
}

func (fs *FSObjects) AbortMultipartUpload(ctx context.Context, bucket, object, uploadID string) error {
	if err := checkAbortMultipartArgs(ctx, bucket, object, fs); err != nil {
		return err
	}

	if _, err := fs.statBucketDir(ctx, bucket); err != nil {
		return toObjectErr(err, bucket)
	}

	fs.appendFileMapMu.Lock()
	// Remove file in tmp folder
	file := fs.appendFileMap[uploadID]
	if file != nil {
		fsRemoveFile(ctx, file.filePath)
	}
	delete(fs.appendFileMap, uploadID)
	fs.appendFileMapMu.Unlock()

	uploadIDDir := fs.getUploadIDDir(bucket, object, uploadID)
	// Just check if the uploadID exists to avoid copy if it doesn't.
	_, err := fsStatFile(ctx, pathJoin(uploadIDDir, fs.metaJSONFile))
	if err != nil {
		if err == errFileNotFound || err == errFileAccessDenied {
			return InvalidUploadID{UploadID: uploadID}
		}
		return toObjectErr(err, bucket, object)
	}

	// Ignore the error returned as Windows fails to remove directory if a file in it
	// is Open()ed by the backgroundAppend()
	fsRemoveAll(ctx, uploadIDDir)

	// It is safe to ignore any directory not empty error (in case there were multiple uploadIDs on the same object)
	fsRemoveDir(ctx, fs.getMultipartSHADir(bucket, object))

	return nil
}

// Removes multipart uploads if any older than `expiry` duration
// on all buckets for every `cleanupInterval`, this function is
// blocking and should be run in a go-routine.
func (fs *FSObjects) cleanupStaleMultipartUploads(ctx context.Context, cleanupInterval, expiry time.Duration, doneCh chan struct{}) {
	ticker := time.NewTicker(cleanupInterval)
	defer ticker.Stop()

	for {
		select {
		case <-doneCh:
			return
		case <-ticker.C:
			now := time.Now()
			entries, err := readDir(pathJoin(fs.fsPath, minioMetaMultipartBucket))
			if err != nil {
				continue
			}
			for _, entry := range entries {
				uploadIDs, err := readDir(pathJoin(fs.fsPath, minioMetaMultipartBucket, entry))
				if err != nil {
					continue
				}

				// Remove the trailing slash separator
				for i := range uploadIDs {
					uploadIDs[i] = strings.TrimSuffix(uploadIDs[i], SlashSeparator)
				}

				for _, uploadID := range uploadIDs {
					fi, err := fsStatDir(ctx, pathJoin(fs.fsPath, minioMetaMultipartBucket, entry, uploadID))
					if err != nil {
						continue
					}
					if now.Sub(fi.ModTime()) > expiry {
						fsRemoveAll(ctx, pathJoin(fs.fsPath, minioMetaMultipartBucket, entry, uploadID))
						// It is safe to ignore any directory not empty error (in case there were multiple uploadIDs on the same object)
						fsRemoveDir(ctx, pathJoin(fs.fsPath, minioMetaMultipartBucket, entry))

						// Remove uploadID from the append file map and its corresponding temporary file
						fs.appendFileMapMu.Lock()
						bgAppend, ok := fs.appendFileMap[uploadID]
						if ok {
							err := fsRemoveFile(ctx, bgAppend.filePath)
							logger.LogIf(ctx, err)
							delete(fs.appendFileMap, uploadID)
						}
						fs.appendFileMapMu.Unlock()
					}
				}
			}
		}
	}
}
