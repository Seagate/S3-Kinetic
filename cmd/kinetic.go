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
	"bytes"
	"context"
	"encoding/gob"
	//"errors"
	"io"
	"net/http"
	"path"
	"time"
	"sync"
	"github.com/minio/minio/pkg/kinetic"
	"github.com/minio/minio/pkg/kinetic_proto"
	"github.com/minio/minio/pkg/mimedb"

	"github.com/minio/minio-go/pkg/s3utils"
	"github.com/minio/minio/cmd/logger"

	//"log"
	"github.com/minio/minio/pkg/lock"
	"github.com/minio/minio/pkg/madmin"

	"encoding/json"
	"github.com/minio/minio/pkg/lifecycle"
	"github.com/minio/minio/pkg/policy"
	"strconv"
)

var numberOfKinConns int = 20 
var maxQueue int = 200

type KConnsPool struct {
	idx    int
	kcs    map[int]*kinetic.Client
	inUsed map[int]int
	totalInUsed int
	max    int
	sync.Mutex
	cond *sync.Cond
}

var kConnsPool KConnsPool
var identity int64 = 1
var hmac_key string = "asdfasdf"

var wg = sync.WaitGroup{}
//var kineticMutex = &sync.Mutex{}

func InitKineticd(storePartition []byte) {
        var cPtr *C.char
        cPtr = (*C.char)(unsafe.Pointer(&storePartition[0]))

	go C.CInitMain(cPtr)
        time.Sleep(5 * time.Second)
}

func InitKineticConnection(IP string, tls bool, kc *kinetic.Client) error {
        var err error = nil
	kc.ConnectionID = 0
	kc.Identity = identity
	kc.Hmac_key = hmac_key
        //return err
	if !tls {
		err = kc.Connect(IP + ":8123")

	} else {
		err = kc.TlsConnect(IP + ":8443")
	}

	if err != nil {
		return err
	}
	err = kc.GetSignOnMessageFor()
	return err
}

type KineticObjects struct {
	//totalUsed    uint64 //Total usage
	fsPath       string
	metaJSONFile string
	rwPool       *fsIOPool
	listPool     *TreeWalkPool
	nsMutex      *nsLockMap
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
	size    int64    `json:"SiZe"`   // length in bytes for regular files; system-dependent for others
	mode    FileMode  `json:"Mode"`  // file mode bits
	createdTime time.Time `json:"CreatedTime"`
	modTime time.Time `json:"ModTime"`  // modification time
}

func ReleaseConnection(ix int) {
	kConnsPool.Lock()
	kConnsPool.inUsed[ix]--
	kConnsPool.totalInUsed--
	kConnsPool.cond.Broadcast()
	kConnsPool.Unlock()
}

func GetKineticConnection() *kinetic.Client {
	kConnsPool.Lock()
	var kc *kinetic.Client
        //for true {
        //        if kConnsPool.inUsed[kConnsPool.idx] == 0 {
	for true {
		if kConnsPool.totalInUsed < maxQueue {
                        kc = kConnsPool.kcs[kConnsPool.idx]
                        kc.Idx = kConnsPool.idx
                        kc.ReleaseConn = func(x int) {
                                ReleaseConnection(x)
                        }
                        kConnsPool.inUsed[kConnsPool.idx]++
                        kConnsPool.idx++
			kConnsPool.totalInUsed++;
                        if kConnsPool.idx > (numberOfKinConns - 1) {
                                kConnsPool.idx = 0
                        }
                        break;
                }
		kConnsPool.cond.Wait()
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
	buf := C.allocate_pvalue_buffer(C.int(bufSize))
        goBuf := (*[1 << 30 ]byte)(unsafe.Pointer(buf))[:bufSize:bufSize]
	return goBuf
}

func initKineticMeta(kc *kinetic.Client) error {
	value := allocateValBuf(0)
	bucketKey := "bucket." + minioMetaBucket
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
	kc.Put(bucketKey, value, 0, kopts)
	var bucketInfo BucketInfo
	bucketInfo.Name = bucketKey
	bucketInfo.Created = time.Now()
	metaKey := "meta." + bucketKey
	var buf bytes.Buffer
	//buf :=  bytes.NewBuffer(gbuf)
	enc := gob.NewEncoder(&buf)
	enc.Encode(bucketInfo)
        gbuf := allocateValBuf(buf.Len())
	copy(gbuf, buf.Bytes())
	//log.Println(" META BUCKET ", metaKey, buf.Cap(), buf.Len())
	//log.Println(" DAT ", string(buf.Bytes()))
	//kc.Put(metaKey, buf.Bytes(), buf.Cap(), kopts)
        kc.Put(metaKey, gbuf, buf.Len(), kopts)

	bucketKey = "bucket." + minioMetaTmpBucket
	kc.Put(bucketKey, value, 0, kopts)

	bucketInfo.Name = bucketKey
	bucketInfo.Created = time.Now()
	metaKey = "meta." + bucketKey
        var buf1 bytes.Buffer
        //buf1 :=  bytes.NewBuffer(gbuf1)

	enc = gob.NewEncoder(&buf1)
	enc.Encode(bucketInfo)
	//log.Println(" META BUCKET ", metaKey, buf1.Len())
        gbuf1 := allocateValBuf(buf1.Len())
        copy(gbuf1, buf1.Bytes())

	kc.Put(metaKey, gbuf1, buf1.Len(), kopts)

	bucketKey = "bucket." + minioMetaMultipartBucket

	kc.Put(bucketKey, value, 0, kopts)
	bucketInfo.Name = bucketKey
	bucketInfo.Created = time.Now()
	metaKey = "meta." + bucketKey
        var buf2 bytes.Buffer
        //buf2 :=  bytes.NewBuffer(gbuf2)

	enc = gob.NewEncoder(&buf2)
	enc.Encode(bucketInfo)
        gbuf2 := allocateValBuf(buf2.Len())
        copy(gbuf2, buf2.Bytes())

	//log.Println(" META BUCKET ", metaKey, buf2.Len())
	kc.Put(metaKey, gbuf2, buf2.Len(), kopts)

	return nil
}

func NewKineticObjectLayer(IP string) (ObjectLayer, error) {
	//fsUUID := mustGetUUID()
	var err error = nil
	//log.SetFlags(log.LstdFlags)
	if IP[:6] == "skinny" {
		kinetic.SkinnyWaistIF =  true
		storePartition := []byte(IP[7:])
		InitKineticd(storePartition)
	}
	if err != nil {
		return nil, err
	}
	//kcsMutex = &sync.Mutex{}
	//ch = make(chan struct{})
	//c = sync.NewCond(kcsMutex)
	kConnsPool.idx = 0
	kConnsPool.kcs = make(map[int]*kinetic.Client, numberOfKinConns)
	kConnsPool.inUsed = make(map[int]int)
        kConnsPool.max = maxQueue
	kConnsPool.totalInUsed = 0
	kConnsPool.cond = sync.NewCond(&kConnsPool)
	for i := 0; i < numberOfKinConns; i++ {
		kc := new(kinetic.Client)
		kConnsPool.kcs[i] = kc
		kConnsPool.inUsed[i] = 0
		if !kinetic.SkinnyWaistIF {
			err = InitKineticConnection(IP, false, kConnsPool.kcs[i])
		} else {
                        err = InitKineticConnection("127.0.0.1", false, kConnsPool.kcs[i])
		}

		if err != nil {
			return nil, err
		}
	}
	initKineticMeta(kConnsPool.kcs[kConnsPool.idx])
	Ko := &KineticObjects{
		metaJSONFile: fsMetaJSONFile,
		rwPool: &fsIOPool{
			readersMap: make(map[string]*lock.RLockedFile),
		},
		nsMutex:  newNSLock(false),
		listPool: NewTreeWalkPool(globalLookupTimeout),
		//kcsMutex: kcsMutex,
		//kcsIdx:   0,
		//kcs:      kcs,
	}
	return Ko, nil
}

func (ko *KineticObjects) NewNSLock(ctx context.Context, bucket string, object string) RWLocker {
    // lockers are explicitly 'nil' for FS mode since there are only local lockers
    return ko.nsMutex.NewNSLock(ctx, nil, bucket, object)
}

func (ko *KineticObjects) Shutdown(ctx context.Context) error {
	return nil
}

func (ko *KineticObjects) StorageInfo(ctx context.Context) StorageInfo {
	return StorageInfo{}
}

/// Bucket operations

// getBucketDir - will convert incoming bucket names to
// corresponding valid bucket names on the backend in a platform
// compatible way for all operating systems.
func (ko *KineticObjects) getBucketDir(ctx context.Context, bucket string) (string, error) {
	//log.Println(" GET BUCKET DIR ", bucket)
	if bucket == "" || bucket == "." || bucket == ".." {
		return "", errVolumeNotFound
	}
	bucketDir := bucket + ".bucket"
	return bucketDir, nil
}

func (ko *KineticObjects) statBucketDir(ctx context.Context, bucket string) (*KVInfo, error) {
	//log.Println(" statBucketDir: ", bucket)

	//bucketDir, err := ko.getBucketDir(ctx, bucket)
	//if err != nil {
	//	return nil, err
	//}
	//st, err := fsStatVolume(ctx, bucketDir)
	st := &KVInfo{}
	//	if err != nil {
	//		return nil, err
	//	}
	return st, nil
}

//THAI:/ MakeBucketWithLocation - create a new bucket, returns if it
// already exists.
func (ko *KineticObjects) MakeBucketWithLocation(ctx context.Context, bucket, location string) error {
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
	//TODO: Check if Bucket exist before create new one
        kopts := kinetic.CmdOpts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
       // var bucketInfo BucketInfo
        metaKey := "meta.bucket." + bucket
        //kineticMutex.Lock()
        kc := GetKineticConnection()
        _, ptr, _, err := kc.CGet(metaKey, kopts)
        ReleaseConnection(kc.Idx)
        C.deallocate_gvalue_buffer((*C.char)(ptr))
        //kineticMutex.Unlock()
	if err == nil {
		return  toObjectErr(errVolumeExists, bucket)
	}  

	//var value []byte //string
        value := allocateValBuf(0)
	bucketKey := string(make([]byte, 1024))
	bucketKey = "bucket." + bucket

	//kineticMutex.Lock()
	kc = GetKineticConnection()
	kc.Put(bucketKey, value, 0, kopts)

	var bucketInfo BucketInfo
	bucketInfo.Name = bucketKey
	bucketInfo.Created = time.Now()
	//metaKey = "meta." + bucketKey
	var buf bytes.Buffer
        //buf :=  bytes.NewBuffer(gbuf)
	enc := gob.NewEncoder(&buf)
	enc.Encode(bucketInfo)
        gbuf := allocateValBuf(buf.Len())
        copy(gbuf, buf.Bytes())
	//log.Println(" BUCKET META", bucketInfo)
	kc.Put(metaKey, gbuf, buf.Len(), kopts)
	ReleaseConnection(kc.Idx)
	//kineticMutex.Unlock()
	return nil
}

//THAI:

func (ko *KineticObjects) GetBucketInfo(ctx context.Context, bucket string) (bi BucketInfo, err error) {
	//log.Println(" GET BUCKET INFO ", bucket)
        bucketLock := ko.NewNSLock(ctx, bucket, "")
        if e := bucketLock.GetRLock(globalObjectTimeout); e != nil {
                return bi, e
        }
        defer bucketLock.RUnlock()
	err = nil;
        kopts := kinetic.CmdOpts{
                ClusterVersion:  0,
                Force:           true,
                Tag:             []byte{},
                Algorithm:       kinetic_proto.Command_SHA1,
                Synchronization: kinetic_proto.Command_WRITEBACK,
                Timeout:         60000, //60 sec
                Priority:        kinetic_proto.Command_NORMAL,
        }
       // var bucketInfo BucketInfo
        metaKey := "meta.bucket." + bucket
        //kineticMutex.Lock()
        kc := GetKineticConnection()
        cvalue, ptr, size, err := kc.CGet(metaKey, kopts)
        var value []byte
        if (cvalue != nil && err == nil) {
            value = (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        } else if err != nil {
	        C.deallocate_gvalue_buffer((*C.char)(ptr))
		ReleaseConnection(kc.Idx)
		//kineticMutex.Unlock()
		return bi, toObjectErr(errVolumeNotFound, bucket)
	}
        buf := bytes.NewBuffer(value[:size])
        dec := gob.NewDecoder(buf)
        dec.Decode(&bi)

        //name := []byte(bi.Name)
        //log.Println(" BUCKET INFO NAME: ", bi.Created, string(name),  err)
        C.deallocate_gvalue_buffer((*C.char)(ptr))
        ReleaseConnection(kc.Idx)
        //kineticMutex.Unlock()

	return bi, nil
}

//THAI:
func (ko *KineticObjects) ListBuckets(ctx context.Context) ([]BucketInfo, error) {
	//log.Println("LIST BUCKET")
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}

	startKey := "meta.bucket."
	endKey := "meta.bucket/"
	//log.Println("START/END keys", startKey, endKey)
	kc := GetKineticConnection()
	keys, err := kc.GetKeyRange(startKey, endKey, true, true, 800, false, kopts)
	if err != nil {
		ReleaseConnection(kc.Idx)
		return nil, err
	}

	var bucketInfos []BucketInfo
	var value []byte
        //var ptr **C.char
	for _, key := range keys {
		var bucketInfo BucketInfo
		//log.Println(" BUCKET RANGE ", string(key), " ", string(key[5:]))
		//var size uint32
		if string(key[:12]) == "meta.bucket." && (string(key[:13]) != "meta.bucket..") {
			cvalue, ptr, size, err := kc.CGet(string(key), kopts)
        		if (cvalue != nil) {
            			value = (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        		}
			if err != nil {
				ReleaseConnection(kc.Idx)
				//kineticMutex.Unlock()
				return nil, err
			}
			buf := bytes.NewBuffer(value[:size])
			dec := gob.NewDecoder(buf)
			dec.Decode(&bucketInfo)
			name := []byte(bucketInfo.Name)
			//log.Println(" BUCKET INFO NAME: ", bucketInfo)
			bucketInfo.Name = string(name[7:])
			//nextMarker = objInfo.Name
			bucketInfos = append(bucketInfos, bucketInfo)
			C.deallocate_gvalue_buffer((*C.char)(ptr))
		}
	}
	ReleaseConnection(kc.Idx)
	//kineticMutex.Unlock()
	//log.Println(" END LIST BUCKET")
	return bucketInfos, nil
}

//THAI:
// DeleteBucket - delete a bucket and all the metadata associated
// with the bucket including pending multipart, object metadata.
func (ko *KineticObjects) DeleteBucket(ctx context.Context, bucket string) error {
	////log.Println("*** DELETE BUCKET *** ", bucket)
	bucketLock := ko.NewNSLock(ctx, bucket, "")
	if err := bucketLock.GetLock(globalObjectTimeout); err != nil {
		logger.LogIf(ctx, err)
		return err
	}
	defer bucketLock.Unlock()
	bucketKey := string(make([]byte, 1024))
	bucketKey = "bucket." + bucket
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           false,
		Tag:             []byte{}, //(fsMeta.Meta),
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
	//kineticMutex.Lock()
	kc := GetKineticConnection()
	kc.Delete(bucketKey, kopts)
	metaKey := "meta." + bucketKey
	kc.Delete(metaKey, kopts)
	ReleaseConnection(kc.Idx)
	//kineticMutex.Unlock()

	return nil
}

/// Object Operations

//THAI:
// CopyObject - copy object source object to destination object.
// if source object and destination object are same we only
// update metadata.
func (ko *KineticObjects) CopyObject(ctx context.Context, srcBucket, srcObject, dstBucket, dstObject string, srcInfo ObjectInfo, srcOpts, dstOpts ObjectOptions) (oi ObjectInfo, e error) {
	////log.Println(" COPY OBJECTS")
	return oi, nil
}

//THAI:
func (ko *KineticObjects) GetObjectNInfo(ctx context.Context, bucket, object string, rs *HTTPRangeSpec, h http.Header, lockType LockType, opts ObjectOptions) (gr *GetObjectReader, err error) {
        //log.Println("***GetObjectNInfo***", object)
	if err = checkGetObjArgs(ctx, bucket, object); err != nil {
		return nil, err
	}
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
	//log.Println(" ++++SIZE ", objInfo.Size)
	// For a directory, we need to send an reader that returns no bytes.
	if hasSuffix(object, SlashSeparator) {
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
	size := length
	closeFn := func() {
	}
	// Check if range is valid
	if off > size || off+length > size {
		err = InvalidRange{off, length, size}
		logger.LogIf(ctx, err)
		//closeFn()
		//rwPoolUnlocker()
		//nsUnlocer()
		return nil, err
	}
	//kc := kinetic.Client{}
	//kineticMutex.Lock()
	kc := GetKineticConnection()
	kc.Key = []byte(bucket + "/" + object)
	var reader1 io.Reader = kc
        //kineticMutex.Unlock()
        //log.Println("IO LIMIT READER")
	reader := io.LimitReader(reader1, length)
//        kineticMutex.Unlock()
        //log.Println("***END: GetObjectNInfo***", object)
	return objReaderFn(reader, h, opts.CheckCopyPrecondFn, closeFn)
}

// Used to return default etag values when a pre-existing object's meta data is queried.
func (ko *KineticObjects) defaultFsJSON(object string) fsMetaV1 {
	fsMeta := newFSMetaV1()
	fsMeta.Meta = make(map[string]string)
	fsMeta.Meta["etag"] = defaultEtag
	contentType := mimedb.TypeByExtension(path.Ext(object))
	fsMeta.Meta["content-type"] = contentType
	return fsMeta
}

// Create a new fs.json file, if the existing one is corrupt. Should happen very rarely.
func (ko *KineticObjects) createFsJSON(object, fsMetaPath string) error {
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
	//log.Println(" GETOBJECTINFO META", bucket, " ", object)
	fsMeta := fsMetaV1{}
//	fsMeta := koMeta{}
/*
	if hasSuffix(object, SlashSeparator) {
		key := bucket + "/" + object
		metakey := "meta." + key
		//value := make([]byte, 1024*1024)
		//log.Println(" ***GET META OBJECT ", metakey)
		kopts := kinetic.CmdOpts{
			ClusterVersion:  0,
			Force:           true,
			Tag:             []byte{}, //(fsMeta.Meta),
			Algorithm:       kinetic_proto.Command_SHA1,
			Synchronization: kinetic_proto.Command_WRITEBACK,
			Timeout:         60000, //60 sec
			Priority:        kinetic_proto.Command_NORMAL,
		}
 	        //log.Println(" 1. KineticObject getObjectInfo ", bucket, " ", object)
		kineticMutex.Lock()
        	//log.Println(" 2. KineticObject getObjectInfo ", bucket, " ", object)

		kc := GetKineticConnection()
		//_, err := kc.Get(metakey, value, kopts)
                cvalue, size, err := kc.CGet(metakey, kopts)
                value := (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]

		ReleaseConnection(kc.Idx)
		kineticMutex.Unlock()
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
	metakey := "meta." + key
//	value := make([]byte, 1024*1024)
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{}, //(fsMeta.Meta),
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
	//kineticMutex.Lock()
	kc := GetKineticConnection()
        //var ptr *C.char
	//size, err := kc.Get(metakey, value, kopts)
        cvalue, ptr, size, err := kc.CGet(metakey, kopts)
        var value []byte
        if (cvalue != nil) {
            value = (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
        }
	ReleaseConnection(kc.Idx)
	//kineticMutex.Unlock()
	if err != nil {
		err = errFileNotFound 
                C.deallocate_gvalue_buffer((*C.char)(ptr))
		return oi, err
	} else {
		//fsMeta = newFSMetaV1()
		fsMeta.Meta = make(map[string]string)
		err = json.Unmarshal(value[:size], &fsMeta)
		if err != nil {
			return oi, err
		}
                C.deallocate_gvalue_buffer((*C.char)(ptr))
		//n, err = strconv.ParseInt(fsMeta.Meta["size"], 10, 64)
		//if err != nil {
		//	return oi, err
		//}
	}
        //log.Println(" SIZE ", fsMeta.KoInfo.Name, fsMeta.KoInfo.Size)

	fi := &KVInfo{
		name:    fsMeta.KoInfo.Name,
		size:    fsMeta.KoInfo.Size, //int64(n),
//		ModTime: time.Now(),
                modTime: fsMeta.KoInfo.CreatedTime, //time.Now(),

	}

	//log.Println(" END: getObjectInfo ", object)
	return fsMeta.ToKVObjectInfo(bucket, object, fi), err
}

// getObjectInfoWithLock - reads object metadata and replies back ObjectInfo.
func (ko *KineticObjects) getObjectInfoWithLock(ctx context.Context, bucket, object string) (oi ObjectInfo, e error) {
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
        //log.Println(" END GET OBJ INFO WITH LOCK ", object)

	return ko.getObjectInfo(ctx, bucket, object)
}

// GetObjectInfo - reads object metadata and replies back ObjectInfo.
func (ko *KineticObjects) GetObjectInfo(ctx context.Context, bucket, object string, opts ObjectOptions) (oi ObjectInfo, e error) {
	//log.Println(" GET OBJ INFO: bucket ", bucket, " object ",object)
	oi, err := ko.getObjectInfoWithLock(ctx, bucket, object)
	if err == errCorruptedFormat || err == io.EOF {
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
	//log.Println(" GET OBJECT FROM BUCKET ", bucket, " ", object, " ", offset, " ", length)
	if err = checkGetObjArgs(ctx, bucket, object); err != nil {
		return err
	}
	objectLock := ko.NewNSLock(ctx, bucket, object)
	if err := objectLock.GetRLock(globalObjectTimeout); err != nil {
		logger.LogIf(ctx, err)
		return err
	}
	defer objectLock.RUnlock()
        //log.Println(" END: GET KINETIC OBJECT FROM BUCKET ", bucket, " ", object, " ", offset, " ", length)

	return ko.getObject(ctx, bucket, object, offset, length, writer, etag, true)
}

//getObject - wrapper for GetObject
func (ko *KineticObjects) getObject(ctx context.Context, bucket, object string, offset int64, length int64, writer io.Writer, etag string, lock bool) (err error) {
	//log.Printf(" 1. GET OBJ %s %s %d\n", bucket, object, length)
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
	if hasSuffix(object, SlashSeparator) {
		_, err = writer.Write([]byte(""))
		logger.LogIf(ctx, err)
		return toObjectErr(err, bucket, object)
	}

	key := bucket + "/" + object
//	value := make([]byte, 1024*1024)
	kopts := kinetic.CmdOpts{
		ClusterVersion: 0,
		Force:          true,
		Tag:            []byte{},
		Algorithm:      kinetic_proto.Command_SHA1,
		Timeout:        60000, //60 sec
		Priority:       kinetic_proto.Command_NORMAL,
	}
	//kineticMutex.Lock()
        //var ptr *C.char
	kc := GetKineticConnection()
        cvalue, ptr, size, err := kc.CGet(key, kopts)
        //var value []byte
        //if (err == nil && cvalue != nil) {
 	  //  value := (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
            //log.Println(" VALUE ", string(value))
        //}
//	size, err := kc.Get(key, value, kopts)
	ReleaseConnection(kc.Idx)
        //log.Println(" GET OBJECT DONE ", key, err, size)
	//kineticMutex.Unlock()

	if err != nil {
		err = errFileNotFound
                C.deallocate_gvalue_buffer((*C.char)(ptr))
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
	        C.deallocate_gvalue_buffer((*C.char)(ptr))
		return err
	}
        value := (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
	//_, err = io.Copy(value[:size], writer)
	//log.Println(" WRITE VALUE")
	writer.Write(value[:size])
        C.deallocate_gvalue_buffer((*C.char)(ptr))

	// Allocate a staging buffer.
	//buf := make([]byte, int(bufSize))
	//_, err = io.CopyBuffer(writer, io.LimitReader(reader, length), buf)
	// The writer will be closed incase of range queries, which will emit ErrClosedPipe.
	//if err == io.ErrClosedPipe {
	//	err = nil
	//}
	//return toObjectErr(err, bucket, object)
        //log.Printf(" END: 1. GET OBJ %s %s %d\n", bucket, object, length)

	return err
}

//THAI
// PutObject - creates an object upon reading from the input stream
// until EOF, writes data directly to configured filesystem path.
// Additionally writes `ko.json` which carries the necessary metadata
// for future object operations.
func (ko *KineticObjects) PutObject(ctx context.Context, bucket string, object string, r *PutObjReader, opts ObjectOptions) (objInfo ObjectInfo, retErr error) {
	//log.Printf(" PutObject %s in Bucket %s\n", object, bucket)
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

	return ko.putObject(ctx, bucket, object, r, opts)
}

// putObject - wrapper for PutObject
func (ko *KineticObjects) putObject(ctx context.Context, bucket string, object string, r *PutObjReader, opts ObjectOptions) (objInfo ObjectInfo, retErr error) {
	//log.Printf(" putObject  %s in %s\n", object, bucket)
	data := r.Reader

	// No metadata is set, allocate a new one.
	//var metaBytes []byte
	meta := make(map[string]string)
	//log.Printf("    OPTS USERDEFINED %+v\n", opts.UserDefined)
	for k, v := range opts.UserDefined {
		meta[k] = v
	}
	var err error

	// Validate if bucket name is valid and exists.
	// TODO: Kinetic Get bucket to check if it exists.
	/*
		key := bucket + ".bucket"
		value := make([]byte, 1024*1024)

		kopts = kinetic.CmdOpts{
			ClusterVersion: 0,
			Force:          true,
			Tag:            []byte{},
			Algorithm:      kinetic_proto.Command_SHA1,
			Timeout:        60000, //60 sec
			Priority:       kinetic_proto.Command_NORMAL,
		}

		_, err = kcs[idx].Get(key, value, kopts)
		if err != nil {
			return ObjectInfo{}, toObjectErr(err, bucket, object)
		}
	*/
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
	//bufSize := int64(blockSizeV1)
	//if size := data.Size(); size > 0 && bufSize > size {
	//	bufSize = size
	//}
	var bufSize int64
	if data.Size() <=  blockSizeV1 {
		bufSize = data.Size()
	} else {
		//log.Println("DAT SIZE TOO BIG")
		return ObjectInfo{}, errInvalidArgument
	}

        //kineticMutex.Lock()
        goBuf := allocateValBuf(int(bufSize))
	//Read data to buf
	_, err = readToBuffer(r, goBuf)
	//kineticMutex.Unlock()
	if err != nil {
		//kineticMutex.Unlock()
		return ObjectInfo{}, err
	}
	fsMeta.Meta["etag"] = r.MD5CurrentHexString()
	fsMeta.Meta["size"] = strconv.FormatInt(data.Size(), 10)
	fsMeta.KoInfo = KOInfo{Name: object, Size: data.Size(), CreatedTime: time.Now()}
	//fsMeta.KoInfo.size  = data.Size()
	//fsMeta.KoInfo.createdTime = time.Now()
	//fsMeta.Meta["createdTime"] = time.Now().String()
	//wg.Add(1)
	//go func() {
	// Write to kinetic
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{}, //(fsMeta.Meta),
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
	key := bucket + "/" + object

	// metadata file
        metaKey := "meta." + key
        //log.Println(" FSMETA ", fsMeta)

        bytes, _ := json.Marshal(&fsMeta)
	//log.Println(" FSMETA ", string(bytes))
        buf := allocateValBuf(len(bytes))
        copy(buf, bytes)
        //kineticMutex.Lock()
        kc := GetKineticConnection()
	kc.Put(key, goBuf, int(bufSize), kopts)
	kc.Put(metaKey, buf, len(buf), kopts) 
	ReleaseConnection(kc.Idx)

        //kineticMutex.Unlock()
	//}()
	objectInfo := ObjectInfo{
		Bucket:  bucket,
		Name:    object,
		ModTime: time.Now(),
		Size:    bufSize,
		ETag:    r.MD5CurrentHexString(),
	}

	// Stat the file to fetch timestamp, size.
	//fi, err := fsStatFile(ctx, pathJoin(ko.fsPath, bucket, object))
	//if err != nil {
	//	return ObjectInfo{}, toObjectErr(err, bucket, object)
	//}

	// Success.
	return objectInfo, nil
	//return ObjectInfo{}, toObjectErr(err, bucket, object)

}

//THAI:
// DeleteObjects - deletes an object from a bucket, this operation is destructive
// and there are no rollbacks supported.
func (ko *KineticObjects) DeleteObjects(ctx context.Context, bucket string, objects []string) ([]error, error) {
	errs := make([]error, len(objects))
	for idx, object := range objects {
		errs[idx] = ko.DeleteObject(ctx, bucket, object)
	}
	return errs, nil
}

//THAI:
// DeleteObject - deletes an object from a bucket, this operation is destructive
// and there are no rollbacks supported.
func (ko *KineticObjects) DeleteObject(ctx context.Context, bucket, object string) error {
	// Acquire a write lock before deleting the object.
	objectLock := ko.NewNSLock(ctx, bucket, object)
	if err := objectLock.GetLock(globalOperationTimeout); err != nil {
		return err
	}
	defer objectLock.Unlock()

	if err := checkDelObjArgs(ctx, bucket, object); err != nil {
		return err
	}

	//minioMetaBucketDir := pathJoin(fs.fsPath, minioMetaBucket)
	//fsMetaPath := pathJoin(minioMetaBucketDir, bucketMetaPrefix, bucket, object, fs.metaJSONFile)
	/*
		if bucket != minioMetaBucket {
			rwlk, lerr := fs.rwPool.Write(fsMetaPath)
			if lerr == nil {
				// This close will allow for fs locks to be synchronized on `fs.json`.
				defer rwlk.Close()
			}
			if lerr != nil && lerr != errFileNotFound {
				logger.LogIf(ctx, lerr)
				return toObjectErr(lerr, bucket, object)
			}
		}

		// Delete the object.
		if err := fsDeleteFile(ctx, pathJoin(fs.fsPath, bucket), pathJoin(fs.fsPath, bucket, object)); err != nil {
			return toObjectErr(err, bucket, object)
		}
	*/
	key := bucket + SlashSeparator + object
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}
	//kineticMutex.Lock()
	kc := GetKineticConnection()
	kc.Delete(key, kopts)
	metakey := "meta." + key
	kc.Delete(metakey, kopts)
	ReleaseConnection(kc.Idx)
	//kineticMutex.Unlock()
	//kc.GetStatus()
	/*
		if bucket != minioMetaBucket {
			// Delete the metadata object.
			err := fsDeleteFile(ctx, minioMetaBucketDir, fsMetaPath)
			if err != nil && err != errFileNotFound {
				return toObjectErr(err, bucket, object)
			}
		} */
	return nil
}

//THAI:
func (ko *KineticObjects) ListObjects(ctx context.Context, bucket, prefix, marker, delimiter string, maxKeys int) (loi ListObjectsInfo, e error) {
	//log.Println("LIST OBJECTS in Bucket ", bucket,  prefix,  marker, delimiter, " ", maxKeys)

	var objInfos []ObjectInfo
	//var eof bool
	//var nextMarker string
	kopts := kinetic.CmdOpts{
		ClusterVersion:  0,
		Force:           true,
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		Synchronization: kinetic_proto.Command_WRITEBACK,
		Timeout:         60000, //60 sec
		Priority:        kinetic_proto.Command_NORMAL,
	}

	startKey := "meta." + bucket + "/" + prefix
	endKey := "meta." + bucket + "0"
	//log.Println(" LIST OBJS START/END", startKey, endKey)
	//kineticMutex.Lock() 
	kc := GetKineticConnection()
	keys, err := kc.GetKeyRange(startKey, endKey, true, true, 800, false, kopts)
        ReleaseConnection(kc.Idx)
	//kineticMutex.Unlock()
	if err != nil {
		return loi, err
	}
	for _, key := range keys {
		var objInfo ObjectInfo
		if string(key[:5]) == "meta." {
			//log.Println("KEY ", string(key[5:]))
			objInfo, err = ko.getObjectInfo(ctx, bucket, string(key[(5+len(bucket)+1):]))
                        //log.Println("1. KEY ", string(key[5:]), len(bucket))

			//nextMarker = objInfo.Name
			if err != nil {
				return loi, err
			}
			//objInfo.Name = string([]byte(objInfo.Name)[len(bucket)+1:])
			objInfos = append(objInfos, objInfo)
                        //log.Println("2. KEY ", objInfo.Name)

		}
	}
	result := ListObjectsInfo{}
	for _, objInfo := range objInfos {
		if objInfo.IsDir && delimiter == SlashSeparator {
			result.Prefixes = append(result.Prefixes, objInfo.Name)
			continue
		}
		result.Objects = append(result.Objects, objInfo)
	}
	return result, nil
}

//THAI:
func (ko *KineticObjects) ReloadFormat(ctx context.Context, dryRun bool) error {
	logger.LogIf(ctx, NotImplemented{})
	return NotImplemented{}
}

func (ko *KineticObjects) HealObjects(ctx context.Context, bucket, prefix string, fn func(string, string) error) (e error) {
	logger.LogIf(ctx, NotImplemented{})
	return NotImplemented{}
}

// ListBucketsHeal - list all buckets to be healed. Valid only for XL
func (ko *KineticObjects) ListBucketsHeal(ctx context.Context) ([]BucketInfo, error) {
	logger.LogIf(ctx, NotImplemented{})
	return []BucketInfo{}, NotImplemented{}
}

// ListObjectsHeal - list all objects to be healed. Valid only for XL
func (ko *KineticObjects) ListObjectsHeal(ctx context.Context, bucket, prefix, marker, delimiter string, maxKeys int) (result ListObjectsInfo, err error) {
	logger.LogIf(ctx, NotImplemented{})
	return ListObjectsInfo{}, NotImplemented{}
}

// HealObject - no-op for ko. Valid only for XL.
func (ko *KineticObjects) HealObject(ctx context.Context, bucket, object string, dryRun, remove bool, scanMode madmin.HealScanMode) (
	res madmin.HealResultItem, err error) {
	logger.LogIf(ctx, NotImplemented{})
	return res, NotImplemented{}
}

// HealBucket - no-op for ko, Valid only for XL.
func (ko *KineticObjects) HealBucket(ctx context.Context, bucket string, dryRun, remove bool) (madmin.HealResultItem,
	error) {
	logger.LogIf(ctx, NotImplemented{})
	return madmin.HealResultItem{}, NotImplemented{}
}

// HealFormat - no-op for ko, Valid only for XL.
func (ko *KineticObjects) HealFormat(ctx context.Context, dryRun bool) (madmin.HealResultItem, error) {
	logger.LogIf(ctx, NotImplemented{})
	return madmin.HealResultItem{}, NotImplemented{}
}

func (ko *KineticObjects) SetBucketPolicy(ctx context.Context, bucket string, policy *policy.Policy) error {
	return savePolicyConfig(ctx, ko, bucket, policy)
}

// GetBucketPolicy will get policy on bucket
func (ko *KineticObjects) GetBucketPolicy(ctx context.Context, bucket string) (*policy.Policy, error) {
	return getPolicyConfig(ko, bucket)
}

// DeleteBucketPolicy deletes all policies on bucket
func (ko *KineticObjects) DeleteBucketPolicy(ctx context.Context, bucket string) error {
	return nil
}

// SetBucketLifecycle sets lifecycle on bucket
func (ko *KineticObjects) SetBucketLifecycle(ctx context.Context, bucket string, lifecycle *lifecycle.Lifecycle) error {
	return saveLifecycleConfig(ctx, ko, bucket, lifecycle)
}

// GetBucketLifecycle will get lifecycle on bucket
func (ko *KineticObjects) GetBucketLifecycle(ctx context.Context, bucket string) (*lifecycle.Lifecycle, error) {
	return getLifecycleConfig(ko, bucket)
}

// DeleteBucketLifecycle deletes all lifecycle on bucket
func (ko *KineticObjects) DeleteBucketLifecycle(ctx context.Context, bucket string) error {
	return removeLifecycleConfig(ctx, ko, bucket)
}

//THAI:
func (ko *KineticObjects) ListObjectsV2(ctx context.Context, bucket, prefix, continuationToken, delimiter string, maxKeys int, fetchOwner bool, startAfter string) (result ListObjectsV2Info, err error) {
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
