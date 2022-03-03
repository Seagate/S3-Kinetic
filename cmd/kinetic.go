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
// #include "C_Kineticd.h"
// #include "C_Operations.h"
	"C"
	"unsafe"
	"bytes"
	"context"
	"encoding/gob"
	"fmt"
	//"errors"
	"io"
	"net/http"
	"os"
	//"os/user"
	"path"
	"runtime"
	"runtime/debug"
	"sort"
	"time"
	"strings"
	"sync"
	"sync/atomic"
	//"github.com/minio/minio/pkg/kinetic"
	"github.com/minio/minio/pkg/kinetic_proto"
	"github.com/minio/minio/pkg/mimedb"

	"github.com/minio/minio-go/pkg/s3utils"
	"github.com/minio/minio/cmd/logger"

	"log"
    "regexp"
	"github.com/minio/minio/pkg/lock"
	"github.com/minio/minio/pkg/madmin"
        bucketsse "github.com/minio/minio/pkg/bucket/encryption"
	"github.com/minio/minio/pkg/bucket/object/tagging"
	"encoding/json"
	"github.com/minio/minio/pkg/bucket/lifecycle"
	"github.com/minio/minio/pkg/bucket/policy"
	"strconv"
	"github.com/minio/minio/common"
    "github.com/minio/minio/pkg/event"
)

var numberOfKinConns int = 2 
var maxQueue int = 100

type KConnsPool struct {
	kcs		map[int]*Client
	inUsed		map[int]bool
	totalInUsed	int
	totalInUsedMax	int
	sync.Mutex
	cond *sync.Cond
}

var kConnsPool KConnsPool
var identity int64 = 1
var hmacKey string = "asdfasdf"

var kineticMutex = &sync.Mutex{}


func bToMb(b uint64) uint64 {
	return b/1024/1024
}

func PrintMemUsage() {
    defer common.KUntrace(common.KTrace("Enter"))
	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	log.Println("Alloc: ", bToMb(m.Alloc))
	log.Println("HeapAlloc: ", bToMb(m.HeapAlloc))
	log.Println("NextGC: ", bToMb(m.NextGC))
	log.Println("System: ", bToMb(m.Sys))
	log.Println("NumForcedGC: ", m.NumForcedGC)
	log.Println("PauseTotal(ms): ", m.PauseTotalNs/1000000)
}

func InitKineticd(argv []string) {
    defer common.KUntrace(common.KTrace("Enter"))
    argc := C.int(len(argv))
	CArgv := (*[0xfff]*C.char)(C.allocArgv(argc))
	defer C.free(unsafe.Pointer(CArgv))
	for i, arg := range argv {
        CArgv[i] = C.CString(arg)
        defer C.free(unsafe.Pointer(CArgv[i]))
    }
    go C.startKineticd(argc, (**C.char)(unsafe.Pointer(CArgv)))
    time.Sleep(5 * time.Second)  // TODO:  sleep does not always work
}
func InitKineticConnection(IP string, tls bool, kc *Client) error {
        defer common.KUntrace(common.KTrace("Enter"))
        var err error = nil
	kc.ConnectionID = 0
	kc.Identity = identity
	kc.HmacKey = hmacKey
        //return err
	if !tls {
		err = kc.Connect(IP + ":8123")

	} else {
		err = kc.TLSConnect(IP + ":8443")
	}

	if err != nil {
		return err
	}
	err = kc.GetSignOnMessageFor()
	return err
}

type KineticObjects struct {
	totalUsed	uint64 //Total usage
	activeIOCount	int64 
        maxActiveIOCount int64
	fsPath		string
	metaJSONFile	string
	rwPool		*fsIOPool
	listPool	*TreeWalkPool
	nsMutex		*nsLockMap
	//kcsMutex     *sync.Mutex
	//kcsIdx       int
	//kcs          map[int]*kinetic.Client
}

type KOInfo struct {
        Name    string   `json:"Name"`      // base name of the file
        Size    int64    `json:"SiZe"`   // length in bytes for regular files; system-dependent for others
        CreatedTime time.Time `json:"CreatedTime"`
        ModTime time.Time `json:"ModTime"`  // modification time
}


type KVInfo struct {
	name    string   `json:"Name"`      // base name of the file
	size    int64    `json:"Size"`   // length in bytes for regular files; system-dependent for others
	mode    FileMode  `json:"Mode"`  // file mode bits
	createdTime time.Time `json:"CreatedTime"`
	modTime time.Time `json:"ModTime"`  // modification time
    hidden bool
}

func ReleaseConnection(ix int) {
    defer common.KUntrace(common.KTrace("Enter"))
	kConnsPool.Lock()
    if kConnsPool.inUsed[ix] == true {
        kConnsPool.inUsed[ix] = false
        kConnsPool.totalInUsed--
        common.KTrace(fmt.Sprintf("totalInused = %d", kConnsPool.totalInUsed))
        if  kConnsPool.totalInUsed < 0 {
            panic("ERROR: Total number of connections in use is negative")
        }
        kConnsPool.cond.Broadcast()
    }
    kConnsPool.Unlock()
}

func GetKineticConnection() *Client {
        defer common.KUntrace(common.KTrace("Enter"))
	kConnsPool.Lock()
	var kc *Client = nil
	for true {
		if kConnsPool.totalInUsed < maxQueue {
			for i:=0; i< maxQueue; i++ {
				if kConnsPool.inUsed[i] == false {
					kc = kConnsPool.kcs[i]
					kc.Idx = i
					kc.ReleaseConn = func(x int) {
						ReleaseConnection(x)
					}
					kConnsPool.inUsed[i] = true
					kConnsPool.totalInUsed++;
                    common.KTrace(fmt.Sprintf("totalInused = %d", kConnsPool.totalInUsed))
					break;
				}
			}
		}
	        if kc == nil {
			//log.Println("WAIT HERE")
			kConnsPool.cond.Wait()
		} else {
			break;
		}
	}
	if kConnsPool.totalInUsed > kConnsPool.totalInUsedMax {
		kConnsPool.totalInUsedMax = kConnsPool.totalInUsed
		log.Println(" MAX TOTAL IN USED", kConnsPool.totalInUsedMax)
	}
	kConnsPool.Unlock()
	return kc
}


func (ki KVInfo) Name() string       { return ki.name }
func (ki KVInfo) Size() int64        { return ki.size }
func (ki KVInfo) Mode() FileMode     { return ki.mode }
func (ki KVInfo) CreatedTime() time.Time { return ki.createdTime}
func (ki KVInfo) ModTime() time.Time { return ki.modTime }
func (ki KVInfo) IsDir() bool        { return false }
func (ki KVInfo) Sys() interface{}   { return nil }


func allocateValBuf(bufSize int) []byte {
        defer common.KUntrace(common.KTrace("Enter"))
	buf := C.allocate_pvalue_buffer(C.int(bufSize))
        goBuf := (*[1 << 30 ]byte)(unsafe.Pointer(buf))[:bufSize:bufSize]
	return goBuf
}

func initKineticMeta(kc *Client) error {
        defer common.KUntrace(common.KTrace("Enter"))
	kopts := Opts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
        var bucketInfo BucketInfo
        var enc *gob.Encoder
        var err error = nil
	value := allocateValBuf(0)
	bucketKey := "bucket." + minioMetaBucket
        if (!kc.DoesObjExist(bucketKey, kopts)) {
            common.KTrace(fmt.Sprintf("Create key %s", bucketKey))
	    bucketInfo.Name = bucketKey
	    bucketInfo.Created = time.Now()
	    var buf bytes.Buffer
	    enc = gob.NewEncoder(&buf)
	    enc.Encode(bucketInfo)
            gbuf := allocateValBuf(buf.Len())
	    copy(gbuf, buf.Bytes())
            _, err = kc.CPut(bucketKey, gbuf, buf.Len(), value, 0, kopts)
            if err != nil {
                return  err
            }
        }
	bucketKey = "bucket." + minioMetaTmpBucket
        if (!kc.DoesObjExist(bucketKey, kopts)) {
            common.KTrace(fmt.Sprintf("Create key %s", bucketKey))
	    bucketInfo.Name = bucketKey
	    bucketInfo.Created = time.Now()
            var buf1 bytes.Buffer
	    enc = gob.NewEncoder(&buf1)
	    enc.Encode(bucketInfo)
            gbuf1 := allocateValBuf(buf1.Len())
            copy(gbuf1, buf1.Bytes())
	    _, err = kc.CPut(bucketKey, gbuf1, buf1.Len(), value, 0, kopts)
            if err != nil {
                return  err
            }
        }
	bucketKey = "bucket." + minioMetaMultipartBucket
        if (!kc.DoesObjExist(bucketKey, kopts)) {
            common.KTrace(fmt.Sprintf("Create key %s", bucketKey))
	    bucketInfo.Name = bucketKey
	    bucketInfo.Created = time.Now()
            var buf2 bytes.Buffer
	    enc = gob.NewEncoder(&buf2)
	    enc.Encode(bucketInfo)
            gbuf2 := allocateValBuf(buf2.Len())
            copy(gbuf2, buf2.Bytes())
	    _, err = kc.CPut(bucketKey, gbuf2, buf2.Len(), value, 0, kopts)
        }

	return err
}

func NewKineticObjectLayer(IP string) (ObjectLayer, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	var err error = nil
	if IP[:6] == "skinny" {
		SkinnyWaistIF =  true
		//storePartition := []byte(IP[7:])
		//InitKineticd(storePartition)
	}
	if err != nil {
		return nil, err
	}
	kConnsPool.kcs = make(map[int]*Client, maxQueue)
	kConnsPool.inUsed = make(map[int]bool)
	kConnsPool.totalInUsed = 0
	kConnsPool.totalInUsedMax = 0
	kConnsPool.cond = sync.NewCond(&kConnsPool)
	for i := 0; i < maxQueue; i++ {
		kc := new(Client)
		kc.Idx = i
		kc.NextPartNumber = new(int)
		kConnsPool.kcs[i] = kc
		kConnsPool.inUsed[i] = false
		if i < numberOfKinConns {
			if !SkinnyWaistIF {
				err = InitKineticConnection(IP, false, kConnsPool.kcs[i])
			}  //else {
			//	err = InitKineticConnection("127.0.0.1", false, kConnsPool.kcs[i])
			//}
		}
		if err != nil {
			return nil, err
		}
	}
	initKineticMeta(kConnsPool.kcs[0])
	Ko := &KineticObjects{
		metaJSONFile: fsMetaJSONFile,
		rwPool: &fsIOPool{
			readersMap: make(map[string]*lock.RLockedFile),
		},
		nsMutex:  newNSLock(false),
		listPool: NewTreeWalkPool(globalLookupTimeout),
	}
	//PrintMemUsage()
	return Ko, nil
}


// NewNSLock - initialize a new namespace RWLocker instance.
func (ko *KineticObjects) NewNSLock(ctx context.Context, bucket string, objects ...string) RWLocker {
        // lockers are explicitly 'nil' for FS mode since there are only local lockers
        return ko.nsMutex.NewNSLock(ctx, nil, bucket, objects...)
}

func (ko *KineticObjects) Shutdown(ctx context.Context) error {
        kineticMutex.Lock()
        kc := GetKineticConnection()
        kc.Flush()
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
	return nil
}

func (ko *KineticObjects) StorageInfo(ctx context.Context, _ bool) StorageInfo {
	return StorageInfo{}
}

func (ko *KineticObjects) waitForLowActiveIO() {
        for atomic.LoadInt64(&ko.activeIOCount) >= ko.maxActiveIOCount {
                time.Sleep(lowActiveIOWaitTick)
        }
}


// CrawlAndGetDataUsage returns data usage stats of the current FS deployment
func (ko *KineticObjects) CrawlAndGetDataUsage(ctx context.Context, endCh <-chan struct{}) DataUsageInfo {
        defer common.KUntrace(common.KTrace("Enter"))
        dataUsageInfo := updateUsage(ko.fsPath, endCh, ko.waitForLowActiveIO, func(item Item) (int64, error) {
                // Get file size, symlinks which cannot bex
                // followed are automatically filtered by fastwalk.
                fi, err := os.Stat(item.Path)
                if err != nil {
                        return 0, errSkipFile
                }
                return fi.Size(), nil
        })

        dataUsageInfo.LastUpdate = UTCNow()
        atomic.StoreUint64(&ko.totalUsed, dataUsageInfo.ObjectsTotalSize)

        return dataUsageInfo
}

/// Bucket operations

// getBucketDir - will convert incoming bucket names to
// corresponding valid bucket names on the backend in a platform
// compatible way for all operating systems.
func (ko *KineticObjects) getBucketDir(ctx context.Context, bucket string) (string, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	if bucket == "" || bucket == "." || bucket == ".." {
		return "", errVolumeNotFound
	}
	bucketDir := bucket + ".bucket"
	return bucketDir, nil
}

func (ko *KineticObjects) statBucketDir(ctx context.Context, bucket string) (*KVInfo, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
        key := "bucket." + bucket
        kineticMutex.Lock()
        kc := GetKineticConnection()
        cvalue, size, err := kc.CGetMeta(key, kopts)
        ReleaseConnection(kc.Idx)
        if err != nil {
                err = errFileNotFound
                kineticMutex.Unlock()
                return &KVInfo{}, err
        }
	bi := BucketInfo{}
        if (cvalue != nil) {
                value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
                buf := bytes.NewBuffer(value[:size])
                dec := gob.NewDecoder(buf)
                dec.Decode(&bi)
                common.KTrace("Free meta")
                C.free(unsafe.Pointer(cvalue))
        }
        kineticMutex.Unlock()

        st := &KVInfo{
                name:    bi.Name,
                size:    0,
                modTime: bi.Created,
	}
	return st, nil
}

//MakeBucketWithLocation - create a new bucket, returns if it
// already exists.
func (ko *KineticObjects) MakeBucketWithLocation(ctx context.Context, bucket, location string) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Printf(" CREATE NEW BUCKET %s LOCATION %s\n", bucket, location)
	bucketLock := ko.NewNSLock(ctx, bucket, "")
	if err := bucketLock.GetLock(globalObjectTimeout); err != nil {
		return err
	}
	defer bucketLock.Unlock()
	// Verify if bucket is valid.
	if s3utils.CheckValidBucketNameStrict(bucket) != nil {
		return BucketNameInvalid{Bucket: bucket}
	}
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()

	//TODO: Check if Bucket exist before create new one
        kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
        key := "bucket." + bucket
        kineticMutex.Lock()
        kc := GetKineticConnection()
        _, _, err := kc.CGetMeta(key, kopts)
        ReleaseConnection(kc.Idx)
	if err == nil {
	        kineticMutex.Unlock()
		return  toObjectErr(errVolumeExists, bucket)
	}
	bucketKey := "bucket." + bucket
	var bucketInfo BucketInfo
	bucketInfo.Name = bucketKey
	bucketInfo.Created = time.Now()
	var buf bytes.Buffer
	enc := gob.NewEncoder(&buf)
	enc.Encode(bucketInfo)
        gbuf := allocateValBuf(buf.Len())
        copy(gbuf, buf.Bytes())
	value := allocateValBuf(0)
    kc = GetKineticConnection()
    kc.CPut(bucketKey, gbuf, buf.Len(), value, 0, kopts)
	ReleaseConnection(kc.Idx)
	kineticMutex.Unlock()
    err = nil
        //ReleaseConnection(kc.Idx)
	return err
}

func (ko *KineticObjects) GetBucketInfo(ctx context.Context, bucket string) (bi BucketInfo, err error) {
        defer common.KUntrace(common.KTrace("Enter"))
        common.KTrace(fmt.Sprintf("bucket: %s", bucket))

	//log.Println(" GET BUCKET INFO ", bucket)
        bucketLock := ko.NewNSLock(ctx, bucket, "")
        if e := bucketLock.GetRLock(globalObjectTimeout); e != nil {
                return bi, e
        }
        defer bucketLock.RUnlock()
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()

	err = nil;
        kopts := Opts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
        key := "bucket." + bucket
        kineticMutex.Lock()
        kc := GetKineticConnection()
	kc.Key = []byte(key)
        cvalue, size, err := kc.CGetMeta(key, kopts)
        ReleaseConnection(kc.Idx)
        if err != nil {
                kineticMutex.Unlock()
                return bi, toObjectErr(errVolumeNotFound, bucket)
        }
        if (cvalue != nil) {
		value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
		buf := bytes.NewBuffer(value[:size])
		dec := gob.NewDecoder(buf)
		dec.Decode(&bi)
                common.KTrace(fmt.Sprintf("bucket info: %+v", bi))
        common.KTrace("Free meta")
        C.free(unsafe.Pointer(cvalue))
	} else {
        common.KTrace("cvalue is nil")
    }
        kineticMutex.Unlock()
	return bi, nil
}

func (ko *KineticObjects) ListBuckets(ctx context.Context) ([]BucketInfo, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("LIST BUCKET")
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()

	kopts := Opts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
	startKey := "bucket."
	endKey := "bucket/"
        var bucketInfos []BucketInfo
        var value []byte
        var lastKey []byte 
	for true {
        	kineticMutex.Lock()
		kc := GetKineticConnection()
		keys, err := kc.CGetKeyRange(startKey, endKey, true, true, 800, false, kopts)
        	ReleaseConnection(kc.Idx)
        	kineticMutex.Unlock()
		if err != nil {
			debug.FreeOSMemory()
			return nil, err
		}
		for _, key := range keys {
			var bucketInfo BucketInfo
			if string(key[:7]) == "bucket." && (string(key[:8]) != "bucket..") {
				cvalue, size, err := kc.CGetMeta(string(key), kopts)
                if err != nil {
                    debug.FreeOSMemory()
                    return nil, err
                }
				if (cvalue != nil) {
                    common.KTrace(fmt.Sprintf("size = %d", size))
					value = (*[1<<30]byte)(unsafe.Pointer(cvalue))[:size:size]
					buf := bytes.NewBuffer(value[:size])
					dec := gob.NewDecoder(buf)
					dec.Decode(&bucketInfo)
                    common.KTrace("Free meta")
                    C.free(unsafe.Pointer(cvalue))
                    common.KTrace(fmt.Sprintf("MyBucketInfo: %+v", bucketInfo))
					name := []byte(bucketInfo.Name)
					bucketInfo.Name = string(name[7:])
					bucketInfos = append(bucketInfos, bucketInfo)
				}
			}
		}
		debug.FreeOSMemory()
		if len(keys) < 800 {
			break
		} else {
			startKey = string(lastKey)
			endKey = ""
		}
	}
        return bucketInfos, nil
}

func (ko *KineticObjects) isBucketEmpty(ctx context.Context, bucket string) (empty bool, err error) {
    empty = false
    var prefix, marker, delimiter = "", "", ""
    maxKeys := 1
    objLst, err := ko.ListObjects(ctx, bucket, prefix, marker, delimiter, maxKeys)
    if err == nil {
        empty = (len(objLst.Objects) == 0 && len(objLst.Prefixes) == 0)
    }
    return empty, err
}

// DeleteBucket - delete a bucket and all the metadata associated
// with the bucket including pending multipart, object metadata.
func (ko *KineticObjects) DeleteBucket(ctx context.Context, bucket string) error {
    defer common.KUntrace(common.KTrace("Enter"))
    bucketLock := ko.NewNSLock(ctx, bucket, "")
    var err error
    if err = bucketLock.GetLock(globalObjectTimeout); err != nil {
        logger.LogIf(ctx, err)
	return err
    }
    defer bucketLock.Unlock()
    atomic.AddInt64(&ko.activeIOCount, 1)
    defer func() {
        atomic.AddInt64(&ko.activeIOCount, -1)
    }()

    var empty bool
    empty, err = ko.isBucketEmpty(ctx, bucket)
    if err != nil {
        return err
    }
    if !empty {
        return errBucketNotEmpty
    }

    commonPrefix := ""

    // Allocate new results channel to receive ObjectInfo.
    objInfoCh := make(chan ObjectInfo)
    // Walk through all objects
    if err = ko.Walk(ctx, bucket, commonPrefix, objInfoCh); err != nil {
        return err
    }
    var lastErr error
    lastErr = nil

    for {
        var objects []string
        for obj := range objInfoCh {
            objects = append(objects, obj.Name)
            if len(objects) == maxObjectList {
                // Reached maximum delete requests, attempt a delete for now.
                break
            }
        }

        // All objects in the bucket have been deleted.  Nothing to do.
        if len(objects) == 0 {
            break
        }

        // Deletes a list of objects.
        deleteErrs, err := ko.DeleteObjects(ctx, bucket, objects)

        if err != nil {
            lastErr = err
            logger.LogIf(ctx, err)
        } else {
            for i := range deleteErrs {
                if deleteErrs[i] != nil {
                    lastErr = deleteErrs[i]
                    common.KTrace(fmt.Sprintf("---- error: %s", deleteErrs[i]))
                    logger.LogIf(ctx, deleteErrs[i])
                    continue
                }
                // Notify object deleted event.
                sendEvent(eventArgs{
                    EventName:  event.ObjectRemovedDelete,
                    BucketName: bucket, //bucket.Name,
                    Object: ObjectInfo{
                        Name: objects[i],
                    },
                    Host: "Internal: [ILM-EXPIRY]",
               })

            }
        }
    }

    if lastErr == nil {
	    bucketKey := string(make([]byte, 1024))
	    bucketKey = "bucket." + bucket
	    kopts := Opts{
		    ClusterVersion:  0,
		    Force:           false,
		    Tag:             []byte{}, //(fsMeta.Meta),
		    Algorithm:       kinetic_proto.Command_SHA1,
		    Synchronization: kinetic_proto.Command_WRITEBACK,
		    Timeout:         60000, //60 sec
		    Priority:        kinetic_proto.Command_NORMAL,
	    }
	    kineticMutex.Lock()
	    kc := GetKineticConnection()
        err = kc.Delete(bucketKey, kopts)
        if err == nil {
            metaKey := "bucketKey"
            err = kc.Delete(metaKey, kopts)
        }
        lastErr = err
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
    } // End of if !hasError
    return lastErr
}

/// Object Operations

// CopyObject - copy object source object to destination object.
// if source object and destination object are same we only
// update metadata.
func (ko *KineticObjects) CopyObject(ctx context.Context, srcBucket, srcObject,
    dstBucket, dstObject string, srcInfo ObjectInfo, srcOpts,
    dstOpts ObjectOptions) (objInfo ObjectInfo, err error) {
    defer common.KUntrace(common.KTrace("Enter"))
        cpSrcDstSame := isStringEqual(pathJoin(srcBucket, srcObject), pathJoin(dstBucket, dstObject))
        if !cpSrcDstSame {
                objectDWLock := ko.NewNSLock(ctx, dstBucket, dstObject)
                if err = objectDWLock.GetLock(globalObjectTimeout); err != nil {
                        return objInfo, err
                }
                defer objectDWLock.Unlock()
        }
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()

        if _, err = ko.statBucketDir(ctx, srcBucket); err != nil {
                return objInfo, toObjectErr(err, srcBucket)
        }

	if cpSrcDstSame && srcInfo.metadataOnly {
                key := srcBucket + SlashSeparator + srcObject
                // Save objects' metadata in `fs.json`.
                fsMeta := newFSMetaV1()
		kopts := Opts{
                        ClusterVersion:  0,
                        Force:           true,
                        Tag:             []byte{}, //(fsMeta.Meta),
                        Algorithm:       kinetic_proto.Command_SHA1,
                        Synchronization: kinetic_proto.Command_WRITEBACK,
                        Timeout:         60000, //60 sec
                        Priority:        kinetic_proto.Command_NORMAL,
                }
	        kineticMutex.Lock()
		kc := GetKineticConnection()
		cvalue, size, err := kc.CGetMeta(key, kopts)
	        ReleaseConnection(kc.Idx)
		if err != nil {
			err = errFileNotFound
                        // For any error to read fsMeta, set default ETag and proceed.
                        fsMeta = ko.defaultFsJSON(srcObject)
		}
		if (cvalue != nil) {
			value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
			fsMeta.Meta = make(map[string]string)
			err = json.Unmarshal(value[:size], &fsMeta)
            common.KTrace("Free meta")
            C.free(unsafe.Pointer(cvalue))
			if err != nil {
	                        // For any error to read fsMeta, set default ETag and proceed.
	                        fsMeta = ko.defaultFsJSON(srcObject)
		                kineticMutex.Unlock()
	                        return objInfo, toObjectErr(err, srcBucket, srcObject)
			}
		}
        key = dstBucket + SlashSeparator + dstObject
                fsMeta.Meta = srcInfo.UserDefined
                fsMeta.Meta["etag"] = srcInfo.ETag
        fsMeta.Meta["size"] =  strconv.FormatInt(fsMeta.KoInfo.Size, 10)
	        bytes, _ := json.Marshal(&fsMeta)
	        buf := allocateValBuf(len(bytes))
		copy(buf, bytes)

	        kc = GetKineticConnection()
                // get file size.
	        fi := &KVInfo{
			name:    fsMeta.KoInfo.Name,
			size:    fsMeta.KoInfo.Size,
			modTime: fsMeta.KoInfo.CreatedTime,
		}

                // Return the new object info.
	        ReleaseConnection(kc.Idx)
                kineticMutex.Unlock()
                return fsMeta.KVInfoToObjectInfo(srcBucket, srcObject, fi), nil
	} // End of IF cpSrcDstSame && srcInfo.metadataOnly
        if err = checkPutObjectArgs(ctx, dstBucket, dstObject, ko, srcInfo.PutObjReader.Size()); err != nil {
                return ObjectInfo{}, err
        }
        if (len(srcInfo.Parts) == 0) {
            objInfo, err = ko.putObject(ctx, dstBucket, dstObject, srcInfo.PutObjReader,
                                ObjectOptions{ServerSideEncryption: dstOpts.ServerSideEncryption,
                                        UserDefined: srcInfo.UserDefined})
        } else {
            objInfo, err = ko.copyMultipartObject(ctx, srcInfo, dstBucket, dstObject)
        }
        if err != nil {
                return objInfo, toObjectErr(err, dstBucket, dstObject)
        }
        return objInfo, nil
}


//THAI:
func (ko *KineticObjects) GetObjectNInfo(ctx context.Context, bucket, object string, rs *HTTPRangeSpec, h http.Header, lockType LockType, opts ObjectOptions) (gr *GetObjectReader, err error) {
        defer common.KUntrace(common.KTrace("Enter"))
	if err = checkGetObjArgs(ctx, bucket, object); err != nil {
		return nil, err
	}
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()

	//Check to see if Bucket exists
	//TODO: Kinetic Get(bucket.bucket....)

	var nsUnlocker = func() {}
	if lockType != noLock {
		// Lock the object before reading.
		lock := ko.NewNSLock(ctx, bucket, object)
		switch lockType {
		case writeLock:
			if err = lock.GetLock(globalObjectTimeout); err != nil {
				logger.LogIf(ctx, err)
				return nil, err
			}
			nsUnlocker = lock.Unlock
		case readLock:
			if err = lock.GetRLock(globalObjectTimeout); err != nil {
				logger.LogIf(ctx, err)
				return nil, err
			}
			nsUnlocker = lock.RUnlock
		}
	}
	// Otherwise we get the object info
	var objInfo ObjectInfo
	if objInfo, err = ko.getObjectInfo(ctx, bucket, object); err != nil {
		nsUnlocker()
		return nil, toObjectErr(err, bucket, object)
	}

	// For a directory, we need to send an reader that returns no bytes.
	if HasSuffix(object, SlashSeparator) {
		// The lock taken above is released when
		// objReader.Close() is called by the caller.
		return NewGetObjectReaderFromReader(bytes.NewBuffer(nil), objInfo, opts.CheckCopyPrecondFn, nsUnlocker)
	}
	// Take a rwPool lock for NFS gateway type deployment
	rwPoolUnlocker := func() {}
	if bucket != minioMetaBucket && lockType != noLock {
		fsMetaPath := pathJoin(ko.fsPath, minioMetaBucket, bucketMetaPrefix, bucket, object, ko.metaJSONFile)
		_, err = ko.rwPool.Open(fsMetaPath)
		if err != nil && err != errFileNotFound {
			logger.LogIf(ctx, err)
			nsUnlocker()
			return nil, toObjectErr(err, bucket, object)
		}
		// Need to clean up lock after getObject is
		// completed.
		rwPoolUnlocker = func() { ko.rwPool.Close(fsMetaPath) }
	}

	objReaderFn, off, length, rErr := NewGetObjectReader(rs, objInfo, opts.CheckCopyPrecondFn, nsUnlocker, rwPoolUnlocker)
	if rErr != nil {
		return nil, rErr
	}
	// Read the object, doesn't exist returns an s3 compatible error.
	size := objInfo.Size
	// Check if range is valid
	if off > size || off+length > size {
		err = InvalidRange{off, length, size}
		logger.LogIf(ctx, err)
		//closeFn()
		//rwPoolUnlocker()
		//nsUnlocer()
		return nil, err
	}
	kc := GetKineticConnection()
	kc.Key = []byte(bucket + "/" + object)
        kc.DataOffset = int(off)
	var reader1 io.Reader = kc
	reader := io.LimitReader(reader1, length)
    closeFn := func() {
        kc.ReleaseConn(kc.Idx)
    }
	return objReaderFn(reader, h, opts.CheckCopyPrecondFn, closeFn)
}

// Used to return default etag values when a pre-existing object's meta data is queried.
func (ko *KineticObjects) defaultFsJSON(object string) fsMetaV1 {
        defer common.KUntrace(common.KTrace("Enter"))
	fsMeta := newFSMetaV1()
	fsMeta.Meta = make(map[string]string)
	fsMeta.Meta["etag"] = defaultEtag
	contentType := mimedb.TypeByExtension(path.Ext(object))
	fsMeta.Meta["content-type"] = contentType
	return fsMeta
}

// Create a new fs.json file, if the existing one is corrupt. Should happen very rarely.
func (ko *KineticObjects) createFsJSON(object, fsMetaPath string) error {
        defer common.KUntrace(common.KTrace("Enter"))
	fsMeta := newFSMetaV1()
	fsMeta.Meta = make(map[string]string)
	fsMeta.Meta["etag"] = GenETag()
	contentType := mimedb.TypeByExtension(path.Ext(object))
	fsMeta.Meta["content-type"] = contentType
	wlk, werr := ko.rwPool.Create(fsMetaPath)
	if werr == nil {
		_, err := fsMeta.WriteTo(wlk)
		wlk.Close()
		return err
	}
	return werr
}

// getObjectInfo - wrapper for reading object metadata and constructs ObjectInfo.
func (ko *KineticObjects) getObjectInfo(ctx context.Context, bucket, object string) (oi ObjectInfo, e error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println(" GETOBJECTINFO META", bucket, " ", object)
	fsMeta := fsMetaV1{}
/*
	if hasSuffix(object, SlashSeparator) {
		key := bucket + "/" + object
		metakey := "meta." + key
		//value := make([]byte, 1024*1024)
		//;og.Println(" ***GET META OBJECT ", metakey)
		kopts := kinetic.Opts{
			ClusterVersion:  0,
			Force:           true,
			Tag:             []byte{}, //(fsMeta.Meta),
			Algorithm:       kinetic_proto.Command_SHA1,
			Synchronization: kinetic_proto.Command_WRITEBACK,
			Timeout:         60000, //60 sec
			Priority:        kinetic_proto.Command_NORMAL,
		}
 	        ////log.Println(" 1. KineticObject getObjectInfo ", bucket, " ", object)
		//kineticMutex.Lock()
        	////log.Println(" 2. KineticObject getObjectInfo ", bucket, " ", object)

		kc := GetKineticConnection()
		//_, err := kc.CGet(metakey, value, kopts)
                cvalue, size, err := kc.CGet(metakey, kopts)
                value := (*[1 << 30 ]byte)(unsafe.Pointer(cvalue))[:size:size]

		ReleaseConnection(kc.Idx)
		//kineticMutex.Unlock()
		if err != nil {
			return oi, err
		}
//                return fsMeta.ToObjectInfo(bucket, object, fi), nil

	}

	fsMetaPath := pathJoin(bucket, object, ko.metaJSONFile)
	//Read `ko.json` to perhaps contend with
	// parallel Put() operations.
	rlk, err := ko.rwPool.Open(fsMetaPath)
	if err == nil {
		// Read from ko metadata only if it exists.
		_, rerr := fsMeta.ReadFrom(ctx, rlk.LockedFile)
		ko.rwPool.Close(fsMetaPath)
		if rerr != nil {
			// For any error to read fsMeta, set default ETag and proceed.
			fsMeta = ko.defaultFsJSON(object)
		}
	}

	// Return a default etag and content-type based on the object's extension.
	if err == errFileNotFound {
		fsMeta = ko.defaultFsJSON(object)
	}
*/
	var key string
	if bucket == "" {
		key = object
	} else {
		key = bucket + "/" + object
	}
	kopts := Opts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{}, //(fsMeta.Meta),
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
    kineticMutex.Lock()
	kc := GetKineticConnection()
    cvalue, size, err := kc.CGetMeta(key, kopts)
    ReleaseConnection(kc.Idx)
    if err != nil {
        err = errFileNotFound
	    kineticMutex.Unlock()
	    return oi, err
	}
    if (cvalue != nil) {
		value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
		fsMeta.Meta = make(map[string]string)
		err = json.Unmarshal(value[:size], &fsMeta)
        common.KTrace("Free meta")
        C.free(unsafe.Pointer(cvalue))
		if err != nil {
			kineticMutex.Unlock()
			return oi, err
		}
	}
	    fi := &KVInfo{
		name:    fsMeta.KoInfo.Name,
		size:    fsMeta.KoInfo.Size,
                modTime: fsMeta.KoInfo.CreatedTime,
        }
        fi.hidden = (fsMeta.Meta["hidden"] != "" && fsMeta.Meta["hidden"] != "0")
    kineticMutex.Unlock()
	return fsMeta.KVInfoToObjectInfo(bucket, object, fi), err
}

// getObjectInfoWithLock - reads object metadata and replies back ObjectInfo.
func (ko *KineticObjects) getObjectInfoWithLock(ctx context.Context, bucket, object string) (oi ObjectInfo, e error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println(" GET OBJ INFO WITH LOCK ", object)
	// Lock the object before reading.
	objectLock := ko.NewNSLock(ctx, bucket, object)
	if err := objectLock.GetRLock(globalObjectTimeout); err != nil {
		return oi, err
	}
	defer objectLock.RUnlock()

	if err := checkGetObjArgs(ctx, bucket, object); err != nil {
		return oi, err
	}

	if _, err := ko.statBucketDir(ctx, bucket); err != nil {
		return oi, err
	}
	return ko.getObjectInfo(ctx, bucket, object)
}

// GetObjectInfo - reads object metadata and replies back ObjectInfo.
func (ko *KineticObjects) GetObjectInfo(ctx context.Context, bucket, object string, opts ObjectOptions) (oi ObjectInfo, e error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println(" GET OBJ INFO: ", object)
	oi, err := ko.getObjectInfoWithLock(ctx, bucket, object)
	if err == errCorruptedFormat || err == io.EOF {
            common.KTrace(fmt.Sprintf("IF erro = %+v", err))
		objectLock := ko.NewNSLock(ctx, bucket, object)
		if err = objectLock.GetLock(globalObjectTimeout); err != nil {
			return oi, toObjectErr(err, bucket, object)
		}

		fsMetaPath := pathJoin(ko.fsPath, minioMetaBucket, bucketMetaPrefix, bucket, object, ko.metaJSONFile)
		err = ko.createFsJSON(object, fsMetaPath)
		objectLock.Unlock()
		if err != nil {
			return oi, toObjectErr(err, bucket, object)
		}

		oi, err = ko.getObjectInfoWithLock(ctx, bucket, object)
	}
        //log.Println(" END: GET OBJ INFO: bucket ", bucket, " object ",object)
	return oi, toObjectErr(err, bucket, object)
}


//THAI:
func (ko *KineticObjects) GetObject(ctx context.Context, bucket, object string, offset int64, length int64, writer io.Writer, etag string, opts ObjectOptions) (err error) {
        defer common.KUntrace(common.KTrace("Enter"))
	if err = checkGetObjArgs(ctx, bucket, object); err != nil {
		return err
	}
	objectLock := ko.NewNSLock(ctx, bucket, object)
	if err := objectLock.GetRLock(globalObjectTimeout); err != nil {
		logger.LogIf(ctx, err)
		return err
	}
	defer objectLock.RUnlock()
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()

        //log.Println(" END: GET KINETIC OBJECT FROM BUCKET ", bucket, " ", object, " ", offset, " ", length)

	return ko.getObject(ctx, bucket, object, offset, length, writer, etag, true)
}

//getObject - wrapper for GetObject
func (ko *KineticObjects) getObject(ctx context.Context, bucket, object string, offset int64, length int64, writer io.Writer, etag string, lock bool) (err error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println(" GETOBJECT ", bucket, object, length)
	if _, err = ko.statBucketDir(ctx, bucket); err != nil {
		return toObjectErr(err, bucket)
	}

	// Offset cannot be negative.
	if offset < 0 {
		logger.LogIf(ctx, errUnexpected)
		return toObjectErr(errUnexpected, bucket, object)
	}
	// Writer cannot be nil.
	if writer == nil {
		logger.LogIf(ctx, errUnexpected)
		return toObjectErr(errUnexpected, bucket, object)
	}

	// If its a directory request, we return an empty body.
	if HasSuffix(object, SlashSeparator) {
		_, err = writer.Write([]byte(""))
		logger.LogIf(ctx, err)
		return toObjectErr(err, bucket, object)
	}

	key := bucket + "/" + object
	kopts := Opts{
		ClusterVersion: 0,
		Force:          true,
		Tag:            []byte{},
		Algorithm:      kinetic_proto.Command_SHA1,
		Timeout:        60000, //60 sec
		Priority:       kinetic_proto.Command_NORMAL,
	}
        kineticMutex.Lock()
	kc := GetKineticConnection()
	kc.Key = []byte(key)
    cvalue, size, err := kc.CGet(key, -1, kopts, 0, -1)  // -1 to indicate it doesn't know the size
	ReleaseConnection(kc.Idx)
	if err != nil {
		err = errFileNotFound
		kineticMutex.Unlock()
		return toObjectErr(err, bucket, object)
	}
	/*
			if bucket != minioMetaBucket {
				fsMetaPath := pathJoin(ko.fsPath, minioMetaBucket, bucketMetaPrefix, bucket, object, ko.metaJSONFile)
				if lock {
					_, err = ko.rwPool.Open(fsMetaPath)
					if err != nil && err != errFileNotFound {
						logger.LogIf(ctx, err)
						return toObjectErr(err, bucket, object)
					}
					defer ko.rwPool.Close(fsMetaPath)
				}
			}

		if etag != "" && etag != defaultEtag {
			objEtag, perr := ko.getObjectETag(ctx, bucket, object, lock)
			if perr != nil {
				return toObjectErr(perr, bucket, object)
			}
			if objEtag != etag {
				logger.LogIf(ctx, InvalidETag{})
				return toObjectErr(InvalidETag{}, bucket, object)
			}
		} */
	// Read the object, doesn't exist returns an s3 compatible error.
	//fsObjPath := pathJoin(ko.fsPath, bucket, object)
	//reader, size, err := fsOpenFile(ctx, fsObjPath, offset)
	bufSize := int64(blockSizeV1)
	if length > 0 && bufSize > length {
		bufSize = length
	}
	// For negative length we read everything.
	if length < 0 {
		length = int64(size) - offset
	}
	// Reply back invalid range if the input offset and length fall out of range.
	if offset > int64(size) || offset+length > int64(size) {
		err = InvalidRange{offset, length, int64(size)}
		logger.LogIf(ctx, err)
		kineticMutex.Unlock()
		return err
	}
	if  cvalue != nil {
	        value := (*[1 << 30 ]byte)(unsafe.Pointer(cvalue))[:size:size]
		writer.Write(value[:size])
	}
        //log.Printf(" END: 1. GET OBJ %s %s %d\n", bucket, object, length)
	kineticMutex.Unlock()
	return err
}

// PutObject - creates an object upon reading from the input stream
// until EOF, writes data directly to configured filesystem path.
// Additionally writes `ko.json` which carries the necessary metadata
// for future object operations.
func (ko *KineticObjects) PutObject(ctx context.Context, bucket string, object string, r *PutObjReader, opts ObjectOptions) (objInfo ObjectInfo, retErr error) {
        defer common.KUntrace(common.KTrace("Enter"))
        common.KTrace(fmt.Sprintf("bucket: %s, object: %s", bucket, object))
	//log.Println(" PutObject ", object, bucket, r)
	if err := checkPutObjectArgs(ctx, bucket, object, ko, r.Size()); err != nil {
		return ObjectInfo{}, err
	}
	// Lock the object.
	objectLock := ko.NewNSLock(ctx, bucket, object)
	if err := objectLock.GetLock(globalObjectTimeout); err != nil {
		logger.LogIf(ctx, err)
		return objInfo, err
	}
	defer objectLock.Unlock()
        atomic.AddInt64(&ko.activeIOCount, 1)
        defer func() {
                atomic.AddInt64(&ko.activeIOCount, -1)
        }()
	return ko.putObject(ctx, bucket, object, r, opts)
}

// putObject - wrapper for PutObject
func (ko *KineticObjects) putObject(ctx context.Context, bucket string, object string, r *PutObjReader, opts ObjectOptions) (objInfo ObjectInfo, retErr error) {
        defer common.KUntrace(common.KTrace("Enter"))
	data := r.Reader

	// Validate if bucket name is valid and exists.
	// Kinetic Get bucket to check if it exists.
	key := "bucket." + bucket
	kopts := Opts{
		ClusterVersion: 0,
		Force:          true,
		Tag:            []byte{},
		Algorithm:      kinetic_proto.Command_SHA1,
		Timeout:        60000, //60 sec
		Priority:       kinetic_proto.Command_NORMAL,
	}
        kineticMutex.Lock()
        kc := GetKineticConnection()
        _, _, err := kc.CGetMeta(key, kopts)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
	if err != nil && err != errKineticNotFound {
		return ObjectInfo{}, toObjectErr(err, bucket, object)
	}
        // No metadata is set, allocate a new one.
        //var metaBytes []byte
        meta := make(map[string]string)
        //log.Println("    OPTS USERDEFINED %+v\n", opts.UserDefined)
        for k, v := range opts.UserDefined {
                meta[k] = v
        }
	fsMeta := newFSMetaV1()
	fsMeta.Meta = meta

	// This is a special case with size as '0' and object ends
	// with a slash separator, we treat it like a valid operation
	// and return success.
	/*if isObjectDir(object, data.Size()) {
		// Check if an object is present as one of the parent dir.
		if ko.parentDirIsObject(ctx, bucket, path.Dir(object)) {
			return ObjectInfo{}, toObjectErr(errFileParentIsFile, bucket, object)
		}
		if err = mkdirAll(pathJoin(ko.fsPath, bucket, object), 0777); err != nil {
			logger.LogIf(ctx, err)
			return ObjectInfo{}, toObjectErr(err, bucket, object)
		}
		var fi KVInfo
		if fi, err = fsStatDir(ctx, pathJoin(ko.fsPath, bucket, object)); err != nil {
			return ObjectInfo{}, toObjectErr(err, bucket, object)
		}
		return fsMeta.ToObjectInfo(bucket, object, fi), nil
	}

	// Check if an object is present as one of the parent dir.
	if ko.parentDirIsObject(ctx, bucket, path.Dir(object)) {
		return ObjectInfo{}, toObjectErr(errFileParentIsFile, bucket, object)
	}
    */
	// Validate input data size and it can never be less than zero.
	if data.Size() < -1 {
		logger.LogIf(ctx, errInvalidArgument)
		return ObjectInfo{}, errInvalidArgument
	}

	// Uploaded object will first be written to the temporary location which will eventually
	// be renamed to the actual location. It is first written to the temporary location
	// so that cleaning it up will be easy if the server goes down.
	//tempObj := mustGetUUID()

	// Allocate a buffer to Read() from request body
	var bufSize int64
	if data.Size() <=  blockSizeV1 {
		bufSize = data.Size()
	} else {
		return ObjectInfo{}, errInvalidArgument
	}
        key = bucket + "/" + object
        objVer, _ := ko.currentVersion(key)
        nxtVer := ko.newVersion(objVer)
        fsMeta.Meta["version"] = nxtVer
        fsMeta.Meta["etag"] = r.MD5CurrentHexString()
        fsMeta.Meta["size"] = strconv.FormatInt(data.Size(), 10)
        fsMeta.KoInfo = KOInfo{Name: object, Size: data.Size(), CreatedTime: time.Now()}
        bytes, _ := json.Marshal(&fsMeta)
        buf := allocateValBuf(len(bytes))
	goBuf := allocateValBuf(int(bufSize))
        copy(buf, bytes)
	//Read data to buf
	_, err = readToBuffer(r, goBuf)
	if err != nil {
		return ObjectInfo{}, err
	}
	//wg.Add(1)
	//go func() {
	// Write to kinetic
        kc = GetKineticConnection()
	_, err = kc.CPut(key, buf, int(len(bytes)), goBuf, int(bufSize), kopts)
	if err != nil {
                ReleaseConnection(kc.Idx)
		return ObjectInfo{}, err
	}
        ReleaseConnection(kc.Idx)
	objectInfo := ObjectInfo{
		Bucket:  bucket,
		Name:    object,
		ModTime: time.Now(),
		Size:    bufSize,
		ETag:    r.MD5CurrentHexString(),
	}
	// Success.
	//Force Garbage Collection
	runtime.GC()
	//PrintMemUsage()
	return objectInfo, nil
}

// DeleteObjects - deletes an object from a bucket, this operation is destructive
// and there are no rollbacks supported.
func (ko *KineticObjects) DeleteObjects(ctx context.Context, bucket string, objects []string) ([]error, error) {
    defer common.KUntrace(common.KTrace("Enter"))
	errs := make([]error, len(objects))
	for idx, object := range objects {
		errs[idx] = ko.DeleteObject(ctx, bucket, object)
	}
	return errs, nil
}

// DeleteObject - deletes an object from a bucket, this operation is destructive
// and there are no rollbacks supported.
func (ko *KineticObjects) DeleteObject(ctx context.Context, bucket, object string) error {
    defer common.KUntrace(common.KTrace("Enter"))
	// Acquire a write lock before deleting the object.
	objectLock := ko.NewNSLock(ctx, bucket, object)
	if err := objectLock.GetLock(globalOperationTimeout); err != nil {
		return err
	}
    defer objectLock.Unlock()
    atomic.AddInt64(&ko.activeIOCount, 1)
    defer func() {
        atomic.AddInt64(&ko.activeIOCount, -1)
    }()

    if err := checkDelObjArgs(ctx, bucket, object); err != nil {
        return err
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
    key := bucket + SlashSeparator + object
    kineticMutex.Lock()
    kc := GetKineticConnection()
    fsMeta := fsMetaV1{}
    cvalue, size, err := kc.CGetMeta(key, kopts)
    if err != nil {
        err = errFileNotFound
        ReleaseConnection(kc.Idx)
	    kineticMutex.Unlock()
        return  err
    }
    var fsMetaBytes []byte
    if (cvalue != nil) {
        fsMetaBytes = (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        err = json.Unmarshal(fsMetaBytes[:size], &fsMeta)
        common.KTrace("Free meta")
        C.free(unsafe.Pointer(cvalue))
    }
    if len(fsMeta.Parts) == 0 {
        key := bucket + SlashSeparator + object
        err = kc.Delete(key, kopts)
        if err != nil {
            ReleaseConnection(kc.Idx)
            kineticMutex.Unlock()
            return err
        }
        metakey := key
        err = kc.Delete(metakey, kopts)
        if err != nil {
            ReleaseConnection(kc.Idx)
            kineticMutex.Unlock()
            return err
        }
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        return nil
    }
    version := fsMeta.Meta["version"]
    for _, part := range  fsMeta.Parts {
        key =  bucket + SlashSeparator + object + "." +  version + "." + fmt.Sprintf("%.5d.%s.%d", part.Number, part.ETag, part.ActualSize)
        kc.Delete(key, kopts)
	}
    key = bucket + SlashSeparator + object
    err = kc.Delete(key, kopts)
    if err != nil {
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
        return err
    }
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    return nil
}

func (ko *KineticObjects) ListObjects(ctx context.Context, bucket, prefix, marker, delimiter string, maxKeys int) (loi ListObjectsInfo, e error) {
    defer common.KUntrace(common.KTrace("Enter"))
    atomic.AddInt64(&ko.activeIOCount, 1)
    defer func() {
        atomic.AddInt64(&ko.activeIOCount, -1)
    }()
	result := ListObjectsInfo{}
	kopts := Opts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
    // Default is recursive, if delimiter is set then list non recursive.
    recursive := true
    if delimiter == SlashSeparator {
        recursive = false
    }
    prefixParts := strings.Split(prefix, "*")
    bRegexp := false
    if len(prefixParts[0]) < len(prefix) {
        bRegexp = true
    }
    bucketPrefix := bucket + SlashSeparator
    var startKey string

    if marker == "" {
	    startKey = bucketPrefix + prefixParts[0]
    } else {
        startKey = bucketPrefix + marker
    }
	endKey := bucketPrefix + prefixParts[0]
    endKey = common.IncStr(endKey)

    maxKeyRange := 400
    nRemainKeys := maxKeys

    if nRemainKeys > maxKeyRange {
        nRemainKeys = maxKeyRange
        result.IsTruncated = true
    }
    bStartKeyInclusive := true
    prefixDir := ""
    lastIndex := strings.LastIndex(prefix, SlashSeparator)
    if lastIndex != -1 {
        prefixDir = prefix[:lastIndex+1]
    }
    name := ""
	var kc *Client
    bDone := false
    // Prepare regular expression for key matching
    regexpStr := bucketPrefix
    for _, prefixPart := range prefixParts {
        regexpStr  += ".*" + prefixPart     // ".*":  zero or more of any character
    }
    for !bDone && nRemainKeys > 0 {
        kineticMutex.Lock()
        kc = GetKineticConnection()
	    keys, err := kc.CGetKeyRange(startKey, endKey, bStartKeyInclusive, true, uint32(nRemainKeys), false, kopts)
        ReleaseConnection(kc.Idx)
	    kineticMutex.Unlock()
        if len(keys) < nRemainKeys {
            bDone = true
            result.IsTruncated = false
        }
	    if err != nil {
		    return loi, err
	    }
        var key []byte
	    for _, key = range keys {
            if bRegexp {
                matched, err := regexp.MatchString(regexpStr, string(key))
                if err != nil || !matched {
                    continue
                }
            }
		    var objInfo ObjectInfo
            name = string(key[len(bucketPrefix):])
			objInfo, err = ko.getObjectInfo(ctx, bucket, name)
			if err != nil {
			    return loi, err
			}
            if  objInfo.Hidden {
                nRemainKeys -= 1
            } else {
		    if recursive {
                result.Objects = append(result.Objects, objInfo)
                nRemainKeys -= 1
            } else {
                // TODO:  Tri:  Need to rework this when DIR is considered as OBJECT when do put.
                afterPrefixDir := name[len(prefixDir):]
                afterPrefixDirSplit := strings.Split(afterPrefixDir, SlashSeparator)
                if len(afterPrefixDirSplit) > 1 {
                    name = prefixDir + afterPrefixDirSplit[0] + SlashSeparator
                    objInfo.IsDir = true
                    objInfo.Name = prefixDir + afterPrefixDirSplit[0] + SlashSeparator
                }
                if objInfo.IsDir {
                    bExist := false
                    for _, pfix := range result.Prefixes {
                        if pfix == objInfo.Name {
                            bExist = true
                            break
                        }
                    }
                    if !bExist {
                        result.Prefixes = append(result.Prefixes, objInfo.Name)
                        nRemainKeys -= 1
                    }
                } else {
                    result.Objects = append(result.Objects, objInfo)
                    nRemainKeys -= 1
                }
            }
            }
	    } // End of FOR _, key := range keys
        if len(keys) > 0 {
            // The last key becomes startKey
            startKey = string(keys[len(keys) - 1])
        }
        bStartKeyInclusive = false
    }  // End of FOR !nDone && nRemainKeys > 0
    if (nRemainKeys > 0) || maxKeys <= maxKeyRange {
        result.IsTruncated = false
    } else {
        nextKey := common.IncStr(startKey)
        result.NextMarker = nextKey[len(bucketPrefix):]
        result.IsTruncated = true
    }
    return result, nil
}

// Returns function "listDir" of the type listDirFunc.
// isLeaf - is used by listDir function to check if an entry
// is a leaf or non-leaf entry.
func (ko *KineticObjects) listDirFactory() ListDirFunc {
        defer common.KUntrace(common.KTrace("Enter"))
        // listDir - lists all the entries at a given prefix and given entry in the prefix.
        listDir := func(bucket, prefixDir, prefixEntry string) (emptyDir bool, entries []string) {
                defer common.KUntrace(common.KTrace(":listDirectoryFactory:func: Enter"))
                var err error
                entries, err = readDir(pathJoin(ko.fsPath, bucket, prefixDir))
                if err != nil && err != errFileNotFound {
                        logger.LogIf(context.Background(), err)
                        return false, nil
                }
                if len(entries) == 0 {
                        return true, nil
                }
                sort.Strings(entries)
                return false, filterMatchingPrefix(entries, prefixEntry)
        }

        // Return list factory instance.
        return listDir
}

// isObjectDir returns true if the specified bucket & prefix exists
// and the prefix represents an empty directory. An S3 empty directory
// is also an empty directory in the FS backend.
func (ko *KineticObjects) isObjectDir(bucket, prefix string) bool {
        defer common.KUntrace(common.KTrace("Enter"))
        entries, err := readDirN(pathJoin(ko.fsPath, bucket, prefix), 1)
        if err != nil {
                return false
        }
        return len(entries) == 0
}



// GetObjectTag - get object tags from an existing object
func (ko *KineticObjects) GetObjectTag(ctx context.Context, bucket, object string) (tagging.Tagging, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        oi, err := ko.GetObjectInfo(ctx, bucket, object, ObjectOptions{})
        if err != nil {
                return tagging.Tagging{}, err
        }

        tags, err := tagging.FromString(oi.UserTags)
        if err != nil {
                return tagging.Tagging{}, err
        }

        return tags, nil

}

// PutObjectTag - replace or add tags to an existing object
func (ko *KineticObjects) PutObjectTag(ctx context.Context, bucket, object string, tags string) error {
        defer common.KUntrace(common.KTrace("Enter"))
	return nil
}


// DeleteObjectTag - delete object tags from an existing object
func (ko *KineticObjects) DeleteObjectTag(ctx context.Context, bucket, object string) error {
        defer common.KUntrace(common.KTrace("Enter"))
        return ko.PutObjectTag(ctx, bucket, object, "")
}


func (ko *KineticObjects) ReloadFormat(ctx context.Context, dryRun bool) error {
        defer common.KUntrace(common.KTrace("Enter"))
	logger.LogIf(ctx, NotImplemented{})
	return NotImplemented{}
}


// HealObjects - no-op for fs. Valid only for XL.
func (ko *KineticObjects) HealObjects(ctx context.Context, bucket, prefix string, fn healObjectFn) (e error) {
        defer common.KUntrace(common.KTrace("Enter"))
        logger.LogIf(ctx, NotImplemented{})
        return NotImplemented{}
}

// GetMetrics - no op
func (ko *KineticObjects) GetMetrics(ctx context.Context) (*Metrics, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        logger.LogIf(ctx, NotImplemented{})
        return &Metrics{}, NotImplemented{}
}


// ListBucketsHeal - list all buckets to be healed. Valid only for XL
func (ko *KineticObjects) ListBucketsHeal(ctx context.Context) ([]BucketInfo, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	logger.LogIf(ctx, NotImplemented{})
	return []BucketInfo{}, NotImplemented{}
}

// ListObjectsHeal - list all objects to be healed. Valid only for XL
func (ko *KineticObjects) ListObjectsHeal(ctx context.Context, bucket, prefix, marker, delimiter string, maxKeys int) (result ListObjectsInfo, err error) {
        defer common.KUntrace(common.KTrace("Enter"))
	logger.LogIf(ctx, NotImplemented{})
	return ListObjectsInfo{}, NotImplemented{}
}

// HealObject - no-op for ko. Valid only for XL.
func (ko *KineticObjects) HealObject(ctx context.Context, bucket, object string, dryRun, remove bool, scanMode madmin.HealScanMode) (
	res madmin.HealResultItem, err error) {
        defer common.KUntrace(common.KTrace("Enter"))
	logger.LogIf(ctx, NotImplemented{})
	return res, NotImplemented{}
}

// HealBucket - no-op for ko, Valid only for XL.
func (ko *KineticObjects) HealBucket(ctx context.Context, bucket string, dryRun, remove bool) (madmin.HealResultItem,
	error) {
        defer common.KUntrace(common.KTrace("Enter"))
	logger.LogIf(ctx, NotImplemented{})
	return madmin.HealResultItem{}, NotImplemented{}
}
/*
* This private listObjects() is called by KineticObject.Walk().  It will retrieves
* all objects with the spcecified prefix and in specified bucket.
*/
func (ko *KineticObjects) listObjects(ctx context.Context, bucket, prefix, delimiter string, resChannel chan<- ObjectInfo) (e error) {
    defer common.KUntrace(common.KTrace("Enter"))
    defer close(resChannel)
	kopts := Opts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}

	startKey := bucket + "/" + prefix
	endKey := bucket + "0"
	var lastKey []byte
	var kc *Client
    var maxKeyRange uint32
    maxKeyRange = 800
    for true {
        kineticMutex.Lock()
        kc = GetKineticConnection()
        keys, err := kc.CGetKeyRange(startKey, endKey, false, true, maxKeyRange, false, kopts)
        ReleaseConnection(kc.Idx)
        kineticMutex.Unlock()
	    if err != nil {
                    debug.FreeOSMemory()
		    return err
	    }

	    for _, key := range keys {
		    lastKey = key
		    var objInfo ObjectInfo
		    if prefix == string(key[len(bucket)+1:len(bucket)+1+len(prefix)]) {
			    objInfo, err = ko.getObjectInfo(ctx, bucket, string(key[(len(bucket)+1):]))
			    if err != nil {
		                    debug.FreeOSMemory()
				    return err
			    }
                if !objInfo.Hidden {
	            if delimiter == SlashSeparator && prefix != "" {
                    if  !HasSuffix(string(prefix), SlashSeparator) {
					    objInfo.IsDir = true
					    objInfo.Name = prefix + SlashSeparator
				    } else {
	                    result := strings.Split(string(key[len(bucket) +1 + len(prefix):]), SlashSeparator)
					    if len(result) == 1 {
					    } else if len(result) > 1 {
                            objInfo.IsDir = true
                            objInfo.Name = prefix + result[0] + SlashSeparator
                        }
				    }
			    } else if delimiter == SlashSeparator && prefix == "" {
				    result := strings.Split(string(key[len(bucket)+1:]), SlashSeparator)
				    if len(result) > 1 {
		                objInfo.IsDir = true
                        objInfo.Name = result[0] + SlashSeparator
				    }
			    }
                resChannel <- objInfo
                }
		    }
	    }
        if len(keys) < int(maxKeyRange) {
            break
        } else {
		    startKey = string(lastKey)
		    endKey = ""
	    }
    }
        debug.FreeOSMemory()
	return nil
}

// Walk a bucket, optionally prefix recursively, until we have returned
// all the content to objectInfo channel, it is callers responsibility
// to allocate a receive channel for ObjectInfo, upon any unhandled
// error walker returns error. Optionally if context.Done() is received
// then Walk() stops the walker.
func (ko *KineticObjects) Walk(ctx context.Context, bucket, prefix string, results chan<- ObjectInfo) error {
    defer common.KUntrace(common.KTrace("Enter"))
    go func() {
        defer common.KUntrace(common.KTrace("Enter"))
        delimiter := ""
        ko.listObjects(ctx, bucket, prefix, delimiter, results)
    } ()
    return nil
}


// HealFormat - no-op for ko, Valid only for XL.
func (ko *KineticObjects) HealFormat(ctx context.Context, dryRun bool) (madmin.HealResultItem, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	logger.LogIf(ctx, NotImplemented{})
	return madmin.HealResultItem{}, NotImplemented{}
}

func (ko *KineticObjects) SetBucketPolicy(ctx context.Context, bucket string, policy *policy.Policy) error {
        defer common.KUntrace(common.KTrace("Enter"))
	return savePolicyConfig(ctx, ko, bucket, policy)
}

// GetBucketPolicy will get policy on bucket
func (ko *KineticObjects) GetBucketPolicy(ctx context.Context, bucket string) (*policy.Policy, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	return getPolicyConfig(ko, bucket)
}

// DeleteBucketPolicy deletes all policies on bucket
func (ko *KineticObjects) DeleteBucketPolicy(ctx context.Context, bucket string) error {
    defer common.KUntrace(common.KTrace("Enter"))
    return removePolicyConfig(ctx, ko, bucket)
}

// SetBucketLifecycle sets lifecycle on bucket
func (ko *KineticObjects) SetBucketLifecycle(ctx context.Context, bucket string, lifecycle *lifecycle.Lifecycle) error {
    defer common.KUntrace(common.KTrace("Enter"))
	return saveLifecycleConfig(ctx, ko, bucket, lifecycle)
}

// GetBucketLifecycle will get lifecycle on bucket
func (ko *KineticObjects) GetBucketLifecycle(ctx context.Context, bucket string) (*lifecycle.Lifecycle, error) {
        defer common.KUntrace(common.KTrace("Enter"))
    lc, err := getLifecycleConfig(ko, bucket)
	return lc, err
}

// DeleteBucketLifecycle deletes all lifecycle on bucket
func (ko *KineticObjects) DeleteBucketLifecycle(ctx context.Context, bucket string) error {
        defer common.KUntrace(common.KTrace("Enter"))
	return removeLifecycleConfig(ctx, ko, bucket)
}

// GetBucketSSEConfig returns bucket encryption config on given bucket
func (ko *KineticObjects) GetBucketSSEConfig(ctx context.Context, bucket string) (*bucketsse.BucketSSEConfig, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        return getBucketSSEConfig(ko, bucket)
}

// SetBucketSSEConfig sets bucket encryption config on given bucket
func (ko *KineticObjects) SetBucketSSEConfig(ctx context.Context, bucket string, config *bucketsse.BucketSSEConfig) error {
        defer common.KUntrace(common.KTrace("Enter"))
        return saveBucketSSEConfig(ctx, ko, bucket, config)
}

// DeleteBucketSSEConfig deletes bucket encryption config on given bucket
func (ko *KineticObjects) DeleteBucketSSEConfig(ctx context.Context, bucket string) error {
        defer common.KUntrace(common.KTrace("Enter"))
        return removeBucketSSEConfig(ctx, ko, bucket)
}


func (ko *KineticObjects) ListObjectsV2(ctx context.Context, bucket, prefix, continuationToken, delimiter string, maxKeys int, fetchOwner bool, startAfter string) (result ListObjectsV2Info, err error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println(" KINETIC: LIST OBJS V2 ", continuationToken, " ", startAfter)
	marker := continuationToken
	if marker == "" {
		marker = startAfter
	}

	loi, err := ko.ListObjects(ctx, bucket, prefix, marker, delimiter, maxKeys)
	if err != nil {
		return result, err
	}

	listObjectsV2Info := ListObjectsV2Info{
		IsTruncated:           loi.IsTruncated,
		ContinuationToken:     continuationToken,
		NextContinuationToken: loi.NextMarker,
		Objects:               loi.Objects,
		Prefixes:              loi.Prefixes,
	}

	return listObjectsV2Info, nil
}

// IsNotificationSupported returns whether bucket notification is applicable for this layer.
func (ko *KineticObjects) IsNotificationSupported() bool {
	return true
}

// IsListenBucketSupported returns whether listen bucket notification is applicable for this layer.
func (ko *KineticObjects) IsListenBucketSupported() bool {
	return true
}

// IsEncryptionSupported returns whether server side encryption is implemented for this layer.
func (ko *KineticObjects) IsEncryptionSupported() bool {
	return true
}

// IsCompressionSupported returns whether compression is applicable for this layer.
func (ko *KineticObjects) IsCompressionSupported() bool {
	return true
}

// IsReady - Check if the backend disk is ready to accept traffic.
func (ko *KineticObjects) IsReady(_ context.Context) bool {
        defer common.KUntrace(common.KTrace("Enter"))
        _, err := os.Stat(ko.fsPath)
        return err == nil
}

func (ko *KineticObjects) makeKey(bucket, obj string) string {
    key := bucket + SlashSeparator + obj
    return key
}

func (ko *KineticObjects) option() Opts {
    return Opts {
            ClusterVersion:  0,
            Force:           true,
            Tag:             []byte{},
            Algorithm:       kinetic_proto.Command_SHA1,
            Synchronization: kinetic_proto.Command_WRITEBACK,
            Timeout:         60000, //60 sec
            Priority:        kinetic_proto.Command_NORMAL,
        }
}

func (ko *KineticObjects) currentVersion(key string) (string, error) {
    defer common.KUntrace(common.KTrace("Enter"))
    common.KTrace(fmt.Sprintf("key: %s", key))
    var curVer string = ""
    option := ko.option()
    kineticMutex.Lock()
    kc := GetKineticConnection()
    cvalue, size, err := kc.CGetMeta(key, option)
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    if err == nil && cvalue != nil {
        //value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        // Work: value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size]
        value := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))
        meta := fsMetaV1{}
        err = json.Unmarshal(value[:size], &meta)
        common.KTrace("Free meta")
        C.free(unsafe.Pointer(cvalue))
        common.KTrace(fmt.Sprintf("err = %+v, Meta: %+v", err, meta))
        if err == nil {
            curVer = meta.Meta["version"]
        }
    }
    common.KTrace(fmt.Sprintf("curVersion: %s", curVer))
    return curVer, err
}
// newVersion
// Version is in the range [1..99999]
// return:  next version string with length of 5 chars
func (ko* KineticObjects) newVersion(aCurVersion string) string {
    defer common.KUntrace(common.KTrace("Enter"))
    var nxtVer string
    if (aCurVersion == "") {
        nxtVer = fmt.Sprintf("%.5d", 1)
    } else {
        curVer, _ := strconv.Atoi(aCurVersion)
        nNxtVer := (curVer + 1) % 100000
        if (nNxtVer == 0) {
            nNxtVer = 1
        }
        nxtVer = fmt.Sprintf("%.5d", nNxtVer)
    }
    common.KTrace(fmt.Sprintf("nxtVer: %s", nxtVer))
    return nxtVer
}

func (ko *KineticObjects) deleteKeys(keys [][]byte) error {
    defer common.KUntrace(common.KTrace("Enter"))
    var err error = nil
    opt := ko.option()
    kineticMutex.Lock()
    kc := GetKineticConnection()
    defer ReleaseConnection(kc.Idx)
    defer kineticMutex.Unlock()

    for _, key := range keys {
        err = kc.Delete(string(key), opt)
        if err != nil {
            break
        }
    }
    return err
}

func (ko *KineticObjects) deleteParts(objKey, version string) error {
    // Get the new version multipart keys to delete
    startKey := objKey + "." + version + "."
    endKey := common.IncStr(startKey)
    kineticMutex.Lock()
    kc := GetKineticConnection()
    objKeys, err := kc.CGetKeyRange(startKey, endKey, true, false, 800, false, ko.option())
    ReleaseConnection(kc.Idx)
    kineticMutex.Unlock()
    if err == nil {
        err = ko.deleteKeys(objKeys)
        if err == nil {
            // Get the new version multipart meta keys to delete
            startKey = startKey
            endKey := common.IncStr(startKey)
            kineticMutex.Lock()
            kc = GetKineticConnection()
            metaKeys, err := kc.CGetKeyRange(startKey, endKey, true, false, 800, false, ko.option())
            ReleaseConnection(kc.Idx)
            kineticMutex.Unlock()
            if err == nil {
                err = ko.deleteKeys(metaKeys)
            }
        }
    }
    return err
}

