package cmd

import (
// #cgo CXXFLAGS: --std=c++0x  -DNDEBUG -DNDEBUGW -DSMR_ENABLED
// #cgo LDFLAGS: -L../lib -lkinetic -lseapubcmds -l:kernel_mem_mgr.a -lssl -lcrypto -lgmock -lgtest -lsmrenv -lleveldb -lmemenv -lkinetic_client -l:zac_kin.a -lprotobuf -lgflags -lpthread -ldl -lrt -lglog

// #include "minio_skinny_waist.h"
       "C"
	"unsafe"
	"crypto/hmac"
	"crypto/sha1"
	"crypto/tls"
	"errors"
	"encoding/json"
	"fmt"
	//"runtime"
        "runtime/debug"
	"strings"
	"encoding/binary"
	"github.com/golang/protobuf/proto"
	"github.com/minio/minio/pkg/kinetic_proto"
	"log" //"github.com/sirupsen/logrus"
	"net"
	//"sync"
	"github.com/minio/minio/common"
)

//var mutexCmd = &sync.Mutex{}
var SkinnyWaistIF bool = false
var MetaSize int = 8*1024

type Opts struct {
	//Command         kinetic_proto.Command_MessageType
	ClusterVersion int64
	Timeout        uint64
	Priority       kinetic_proto.Command_Priority
	BatchID        uint32
	//cmdKeyValue := &kinetic_proto.Command_KeyValue
	NewVersion     []byte
	Force           bool
	//Key             []byte
	DbVersion       []byte
	Tag             []byte
	Algorithm       kinetic_proto.Command_Algorithm
	MetaDataOnly    bool
	Synchronization kinetic_proto.Command_Synchronization
}


type Client struct {
	socket       net.Conn
	Idx          int
	ConnectionID int64
	Sequence     uint64
	UserID      int
	Identity     int64
	HmacKey     string
	ReadSize     int32
	WriteSize    int32
	Key          []byte
	Opts         Opts
	ReleaseConn  func(int)
	NextPartNumber *int
	LastPartNumber int
}

func (c *Client) Read(value []byte) (int, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        //log.Println(" ****READ****", string(c.Key), c.LastPartNumber)
	//runtime.GC()
	debug.FreeOSMemory()
	//PrintMemUsage()
	fsMeta := fsMetaV1{}
        cvalue, size, err := c.CGetMeta(string(c.Key), c.Opts)
        if err != nil {
                err = errFileNotFound
                c.ReleaseConn(c.Idx)
                return 0, err
        }
        if (cvalue != nil) {
		fsMetaBytes := (*[1 << 16 ]byte)(unsafe.Pointer(cvalue))[:size:size]
		err = json.Unmarshal(fsMetaBytes[:size], &fsMeta)
	}
	c.LastPartNumber =  len(fsMeta.Parts)
	//log.Println(" READ SIZE", fsMeta.KoInfo.Size)
	if len(fsMeta.Parts) == 0 {
		cvalue, size, err := c.CGet(string(c.Key), int(fsMeta.KoInfo.Size), c.Opts)
                if err != nil {
                        c.ReleaseConn(c.Idx)
                        return 0, err
                }
		if cvalue  != nil {
			value1 := (*[1 << 30 ]byte)(unsafe.Pointer(cvalue))[:size:size]
			copy(value, value1)
                        c.ReleaseConn(c.Idx)
                        return int(size), err
                }
                c.ReleaseConn(c.Idx)
		return 0, err
	}
    partKeyPrefix := string(c.Key) + "." + fsMeta.Version
	for i, part := range  fsMeta.Parts {
		if i == *(c.NextPartNumber) {
			key := partKeyPrefix + "." +  fmt.Sprintf("%.5d.%s.%d", part.Number, part.ETag, part.ActualSize)
			cvalue, size, err := c.CGet(key, 0, c.Opts)
			if err != nil {
				c.ReleaseConn(c.Idx)
				return 0, err
			}
			if cvalue != nil {
				value1 := (*[1 << 30 ]byte)(unsafe.Pointer(cvalue))[:size:size]
				copy(value, value1)
				if i ==  len(fsMeta.Parts) -1 {
					*(c.NextPartNumber) = 0
				 c.ReleaseConn(c.Idx)
				} else {
				        *(c.NextPartNumber)++
				}
				return int(size), err
			}
		}
	}
        c.ReleaseConn(c.Idx)
	return 0, err
}


func (c *Client) Write(p []byte, size int) (int, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	n, err := c.Put(string(c.Key), p, size, c.Opts)
	return int(n), err
}

//Close: close socket
func (c *Client) Close() {
        defer common.KUntrace(common.KTrace("Enter"))
	c.socket.Close()
}

//ComputeHMAC: 
func ComputeHMAC(msg, HmacKey []byte) []byte {
        defer common.KUntrace(common.KTrace("Enter"))
	var msgSize uint32 = uint32(len(msg))
	size := make([]byte, 4, 5)
	binary.BigEndian.PutUint32(size[1:5], msgSize)
	h := hmac.New(sha1.New, []byte(HmacKey))
	h.Write(size[1:5])
	h.Write(msg)
	khmac := h.Sum(nil)
	return khmac
}

//Send: 
func (c *Client) Send(msg *kinetic_proto.Message, value []byte, size int) error {
        defer common.KUntrace(common.KTrace("Enter"))
	if *msg.AuthType == kinetic_proto.Message_HMACAUTH {
		khmac := ComputeHMAC(msg.CommandBytes, []byte(c.HmacKey))
		messageHMACauth := &kinetic_proto.Message_HMACauth{
			Identity: &c.Identity,
			Hmac:     khmac,
		}
		msg.HmacAuth = messageHMACauth
	}
	msgMarshal, _ := proto.Marshal(msg)
	var msgSize uint32 = uint32(len(msgMarshal))
	var valueSize uint32 = uint32(size)
	txHeader := make([]byte, 9)
	txHeader[0] = 'F'
	binary.BigEndian.PutUint32(txHeader[1:5], msgSize)
	binary.BigEndian.PutUint32(txHeader[5:9], valueSize)
	//log.Println(" WRITE SOCK HEADER")
	err := Write(c.socket, txHeader, 9)
	if err != nil {
		return err
	}
	err = Write(c.socket, msgMarshal, msgSize)
	if err != nil {
		return err
	}
	err = Write(c.socket, value, valueSize)
	return err
}

func Read(socket net.Conn, buffer []byte, size uint32) error {
        defer common.KUntrace(common.KTrace("Enter"))
	var bytesRead uint32
	var err error
	for bytesRead < size {
		n, err := socket.Read(buffer)
		if err != nil {
			//Connection may be closed by Peer
			socket.Close()
			//log.Println(" Connection Closed by Peer ", err)
			return err
		}
		if n > 0 {
			bytesRead += uint32(n)
		}
	}
	return err
}

func Write(socket net.Conn, buffer []byte, size uint32) error {
        defer common.KUntrace(common.KTrace("Enter"))
	var bytesWritten uint32
	var err error
	for bytesWritten < size {
		n, err := socket.Write(buffer)
		if err != nil {
			///log.Println(" TX Error: ", err)
			return err
		}
		if n > 0 {
			bytesWritten += uint32(n)
		}
	}
	return err
}

func (c *Client) Connect(address string) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("Starting client...")
	var err error
	conn, err := net.Dial("tcp", address)
	if err != nil {
		//log.Println("FAILED TO CONNECT", err)
		return err
	}
	c.socket = conn
	return err
}

func (c *Client) TLSConnect(address string) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println(" TLS connection")
	var err error
	cert, err := tls.LoadX509KeyPair("selfsigned.crt", "selfsigned.key")
	if err != nil {
		log.Fatalf("server: loadkeys: %s", err)
	}
	//roots := x509.NewCertPool()
	//ok := roots.AppendCertsFromPEM([]byte(rootCert))
	//if !ok {
	//	log.Fatal("failed to parse root certificate")
	//	return err, false
	//}
	config := tls.Config{Certificates: []tls.Certificate{cert}, InsecureSkipVerify: true}
	conn, err := tls.Dial("tcp", address, &config)
	if err != nil {
		log.Fatalf("Dial: %s", err)
		return err
	}
	c.socket = conn
	return err
}

func SetCmdInHeader(c *Client, header *kinetic_proto.Command_Header, cmdtype kinetic_proto.Command_MessageType, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//cmdHeader := &kinetic_proto.Command_Header{
	//              ClusterVersion: 0,
	//              ConnectionID: &c.ConnectionID,
	//              Sequence:     &c.Sequence,
	//		MessageType:  new(kinetic_proto.Command_MessageType),
	//}
	var err error
	header.MessageType = new(kinetic_proto.Command_MessageType)
	switch cmdtype {
	case kinetic_proto.Command_GET:
	case kinetic_proto.Command_PUT:
	case kinetic_proto.Command_DELETE:
	case kinetic_proto.Command_GETNEXT:
	case kinetic_proto.Command_GETPREVIOUS:
	case kinetic_proto.Command_GETKEYRANGE:
	case kinetic_proto.Command_GETVERSION:
	case kinetic_proto.Command_SETUP:
	case kinetic_proto.Command_GETLOG:
	case kinetic_proto.Command_SECURITY:
	case kinetic_proto.Command_PEER2PEERPUSH:
	case kinetic_proto.Command_NOOP:
	case kinetic_proto.Command_FLUSHALLDATA:
	case kinetic_proto.Command_PINOP:
	case kinetic_proto.Command_MEDIASCAN:
	case kinetic_proto.Command_MEDIAOPTIMIZE:
	case kinetic_proto.Command_START_BATCH:
	case kinetic_proto.Command_END_BATCH:
	case kinetic_proto.Command_ABORT_BATCH:
	case kinetic_proto.Command_SET_POWER_LEVEL:
	default:
		//log.Println("INVALID COMMAND")
		return errors.New("INVALID COMMAND")
	}
	header.ClusterVersion = new(int64)
	*header.ClusterVersion = 0
	header.ConnectionID = &c.ConnectionID
	header.Sequence = new(uint64)
	*header.Sequence = c.Sequence
	c.Sequence++
	header.MessageType = new(kinetic_proto.Command_MessageType)
	*header.MessageType = cmdtype
	header.Timeout = new(uint64)
	*header.Timeout = cmd.Timeout
	header.Priority = new(kinetic_proto.Command_Priority)
	*header.Priority = cmd.Priority
	return err
}

func SetCmdKeyValue(kv *kinetic_proto.Command_KeyValue, key []byte, algorithm kinetic_proto.Command_Algorithm, sync kinetic_proto.Command_Synchronization) error {
        defer common.KUntrace(common.KTrace("Enter"))
	// Keep these comments:
	//cmdKeyValue := &kinetic_proto.Command_KeyValue
	//	NewVersion	[]byte
	//	Force		*bool
	//	Key		[]byte
	//      DbVersion	[]byte
	//	Tag		[]byte
	//	Algorithn	*Command_Algorithm
	//	MetaDataOnly	*bool
	//	Synchronization	*Command_Synchronization
	// }
	kv.Key = []byte(key)
	kv.Tag = make([]byte, 1)
	if algorithm != kinetic_proto.Command_INVALID_ALGORITHM {
		kv.Algorithm = new(kinetic_proto.Command_Algorithm)
		switch algorithm {
		case kinetic_proto.Command_SHA1:
		case kinetic_proto.Command_SHA2:
		case kinetic_proto.Command_SHA3:
		case kinetic_proto.Command_CRC32C:
		case kinetic_proto.Command_CRC64:
		case kinetic_proto.Command_CRC32:
		default:
			//log.Println("INVALID ALGORITHM")
			return errors.New("INVALID ALGORITHM")
		}
		*kv.Algorithm = algorithm
	}
	if sync != kinetic_proto.Command_INVALID_SYNCHRONIZATION {
		kv.Synchronization = new(kinetic_proto.Command_Synchronization)
		switch sync {
		case kinetic_proto.Command_WRITETHROUGH:
		case kinetic_proto.Command_WRITEBACK:
		case kinetic_proto.Command_FLUSH:
		default:
			//log.Println("INVALID SYNCHRONIZATION")
			return errors.New("INVALID SYNCHRONIZATION")
		}
		*kv.Synchronization = sync
	}
	return nil
}

func SetCmdRange(rng *kinetic_proto.Command_Range, startKey string, endKey string, startKeyInclusive bool, endKeyInclusive bool, maxReturned uint32, reverse bool) {
        defer common.KUntrace(common.KTrace("Enter"))
	rng.StartKey = []byte(startKey)
	rng.EndKey = []byte(endKey)
	rng.StartKeyInclusive = new(bool)
	rng.EndKeyInclusive = new(bool)
	rng.MaxReturned = new(uint32)
	rng.Reverse = new(bool)
	*rng.StartKeyInclusive = startKeyInclusive
	*rng.EndKeyInclusive = endKeyInclusive
	*rng.MaxReturned = maxReturned
	*rng.Reverse = reverse
}

func (c *Client) SetLockPin(oldpin, newpin []byte, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD SET LOCK PIN: ")
	var err error
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err = SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_SECURITY, cmd)
	if err != nil {
		return err
	}
	securityoptype := new(kinetic_proto.Command_Security_SecurityOpType)
	*securityoptype = kinetic_proto.Command_Security_LOCK_PIN_SECURITYOP
	cmdSecurity := &kinetic_proto.Command_Security{
		OldLockPIN:     oldpin,
		NewLockPIN:     newpin,
		SecurityOpType: securityoptype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		Security: cmdSecurity,
	}

	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) SetErasePin(oldpin, newpin []byte, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD SET ERASE PIN: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_SECURITY, cmd)
	if err != nil {
		return err
	}
	securityoptype := new(kinetic_proto.Command_Security_SecurityOpType)
	*securityoptype = kinetic_proto.Command_Security_ERASE_PIN_SECURITYOP
	cmdSecurity := &kinetic_proto.Command_Security{
		OldErasePIN:    oldpin,
		NewErasePIN:    newpin,
		SecurityOpType: securityoptype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		Security: cmdSecurity,
	}

	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		return err
	}
	_, _, err = c.GetStatus()
	return err
}

func (c *Client) StartBatch(cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD START BATCH: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_START_BATCH, cmd)
	if err != nil {
		return err
	}
	cmdHeader.BatchID = new(uint32)
	*cmdHeader.BatchID = cmd.BatchID
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) EndBatch(count uint32, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD END BATCH: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_END_BATCH, cmd)
	if err != nil {
		return err
	}
	cmdHeader.BatchID = new(uint32)
	*cmdHeader.BatchID = cmd.BatchID
	cmdBatch := &kinetic_proto.Command_Batch{}
	cmdBatch.Count = new(uint32)
	*cmdBatch.Count = count
	cmdBody := &kinetic_proto.Command_Body{
		Batch: cmdBatch,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) AbortBatch(cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD ABORT BATCH: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_ABORT_BATCH, cmd)
	if err != nil {
		return err
	}
	cmdHeader.BatchID = new(uint32)
	*cmdHeader.BatchID = cmd.BatchID
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) CGetMeta(key string, acmd Opts) (*C.char, uint32, error) {
    defer common.KUntrace(common.KTrace("Enter"))
	metaKey := "meta." + key
	metaSize := MetaSize
	return c.CGet(metaKey, metaSize, acmd)
}


//CGet: Use this for Skinny Waist interface
func (c *Client) CGet(key string, size int, acmd Opts) (*C.char, uint32, error) {
    defer common.KUntrace(common.KTrace("Enter"))
        //log.Println(" CALL CGET ", key, size)
        var psv C._CPrimaryStoreValue
        psv.version = C.CString(string(acmd.NewVersion))
        psv.tag = C.CString(string(acmd.Tag))
        psv.algorithm = C.int(acmd.Algorithm)
        cKey := C.CString(key)
	var size1 int
	var status int
	var cvalue *C.char
	var bvalue []byte
	if size > 0 {
		bvalue = make([]byte, size+4096)
	} else {
		log.Println("ALLOC 5MB", key)
		bvalue = make([]byte, 5*1048576+4096)
	}
        cvalue = C.Get(1, cKey, (*C.char)(unsafe.Pointer(&bvalue[0])), &psv, (*C.int)(unsafe.Pointer(&size1)), (*C.int)(unsafe.Pointer(&status)))
	//log.Println("CVALUE BVALUE", cvalue, &bvalue[0])
	var err error = nil
	if status != 0 || cvalue == nil {
		err =  errKineticNotFound //errors.New("NOT FOUND")
	}
        //log.Println(" CGET DONE ", err, cvalue)
        return cvalue, uint32(size1), err
}


//Use this for Kinetic API 
func (c *Client) Get(key string, value []byte, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        //log.Println(" NORMAL GET")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_GET, cmd)
	if err != nil {
		return 0, err
	}
	cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	SetCmdKeyValue(cmdKeyValue, []byte(key), kinetic_proto.Command_INVALID_ALGORITHM, kinetic_proto.Command_INVALID_SYNCHRONIZATION)
	cmdBody := &kinetic_proto.Command_Body{
		KeyValue: cmdKeyValue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		return 0, err
	}
	_, valueSize, err := c.GetStatus()
	if err != nil {
		//log.Println("GET STATUS FAILED")
		return 0, err
	}
	err = Read(c.socket, value, valueSize)
	if err != nil {
		return 0, err
	}
	return valueSize, err
}

//CDelete: 
func (c *Client) cDelete(key string, cmd Opts)  error {
        defer common.KUntrace(common.KTrace("Enter"))
        currentVersion := "1"
        cKey := C.CString(key)
	var status C.int
        status = C.Delete(1, cKey, C.CString(currentVersion), false, 1, 1)
	//log.Println(" STATUS DEL ", int(status))
        return  toKineticError(KineticError(int(status)))
}

func (c *Client) Delete(key string, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
        common.KTrace(fmt.Sprint("key: %s", key))
        if SkinnyWaistIF {
                return c.cDelete(key, cmd)
        }
	//log.Println("CMD DELETE: ", key)
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_DELETE, cmd)
	if err != nil {
		return err
	}
	var force = cmd.Force
	cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	cmdKeyValue.Force = &force
	SetCmdKeyValue(cmdKeyValue, []byte(key), cmd.Algorithm, cmd.Synchronization)
	cmdBody := &kinetic_proto.Command_Body{
		KeyValue: cmdKeyValue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value []byte
	err = c.Send(message, value, 0)

	if err != nil {
		//log.Println("SENT FAILED")
		return err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("GET STTAUS FAILED")
		return err
	}
	return err
}

func (c *Client) CPutMeta(key string, value []byte, size int, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	metaKey := "meta." + key
	return c.CPut(metaKey, value, size, cmd)

}

func (c *Client) CPut(key string, value []byte, size int, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//start := time.Now()
	var psv C._CPrimaryStoreValue
	psv.version = C.CString(string(cmd.NewVersion))
	psv.tag = C.CString(string(cmd.Tag))
	psv.algorithm = C.int(cmd.Algorithm)
        currentVersion := "1"
	cKey := C.CString(key)
	var cValue *C.char
	if  size  > 0 {
		cValue = (*C.char)(unsafe.Pointer(&value[0]))
	} else {
		cValue  = (*C.char)(nil)
	}
	var status C.int 
	status = C.Put(1, cKey, C.CString(currentVersion), &psv, cValue, C.size_t(size), false, 1, 1)
        //log.Println("MINIO CPUT DONE ", toKineticError(KineticError(int(status))), time.Since(start))
        return uint32(size),  toKineticError(KineticError(int(status)))
}


func (c *Client) Put(key string, value []byte, size int, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
        if SkinnyWaistIF {
		return c.CPut(key, value, size, cmd)
	}
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_PUT, cmd)
	if err != nil {
		return 0, err
	}
	var force = cmd.Force
	cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	cmdKeyValue.NewVersion = cmd.NewVersion
	cmdKeyValue.Force = &force
	cmdKeyValue.Algorithm = &cmd.Algorithm
	cmdKeyValue.Synchronization = &cmd.Synchronization
	cmdKeyValue.Key = []byte(key)
	cmdKeyValue.Tag = cmd.Tag

	cmdBody := &kinetic_proto.Command_Body{
		KeyValue: cmdKeyValue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	err = c.Send(message, value, size)
	if err != nil {
		return 0, err
	}

	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("PUT FAILED")
		return 0, err
	}
	return uint32(size), nil
}


func (c *Client) PutB(key string, value []byte, size int, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD PUT BATCH: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_PUT, cmd)
	if err != nil {
		return err
	}
	cmdHeader.BatchID = new(uint32)
	*cmdHeader.BatchID = cmd.BatchID
	var force = cmd.Force
	cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	cmdKeyValue.Force = &force
	SetCmdKeyValue(cmdKeyValue, []byte(key), cmd.Algorithm, cmd.Synchronization)
	cmdBody := &kinetic_proto.Command_Body{
		KeyValue: cmdKeyValue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	err = c.Send(message, value, size )
	return err
}

func (c *Client) GetNext(key string, value []byte, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD GETNEXT: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_GETNEXT, cmd)
	cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	SetCmdKeyValue(cmdKeyValue, []byte(key), kinetic_proto.Command_INVALID_ALGORITHM, kinetic_proto.Command_INVALID_SYNCHRONIZATION)
	cmdBody := &kinetic_proto.Command_Body{
		KeyValue: cmdKeyValue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err := c.Send(message, value1, 0)
	if err != nil {
		return 0, err
	}
	_, valueSize, err := c.GetStatus()
	if err != nil {
		//log.Println("GETNEXT FAILED")
		return 0, err
	}
	err = Read(c.socket, value, valueSize)
	if err != nil {
		return 0, err
	}
	return valueSize, err
}

// TODO: Use this to interface to Skinny Waist
func (c *Client) CGetKeyRange(startKey string, endKey string, startKeyInclusive bool, endKeyInclusive bool, maxReturned uint32, reverse bool, cmd Opts) ([][]byte, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	cStartKey := C.CString(startKey)
	cEndKey := C.CString(endKey)
	Keys  := make([]byte, 1024*1024)
	cKeys :=  (*C.char)(unsafe.Pointer(&Keys[0]))
	var size int
	cSize := (*C.int)(unsafe.Pointer(&size))
	C.GetKeyRange(1, cStartKey, cEndKey, C.bool(startKeyInclusive), C.bool(endKeyInclusive), C.uint32_t(maxReturned), false, cKeys, cSize)
        Keys = (*[1 << 30 ]byte)(unsafe.Pointer(cKeys))[:size:size]
	keyStrings := strings.Split(string(Keys[:size]), ":")
	var keys [][]byte
	for i := range  keyStrings {
		//log.Println("KEY ", keyStrings[i])
		if len(keyStrings[i]) > 0 {
			keys = append(keys, []byte(keyStrings[i]))
		}
	}
	return keys, nil
}



func (c *Client) GetKeyRange(startKey string, endKey string, startKeyInclusive bool, endKeyInclusive bool, maxReturned uint32, reverse bool, cmd Opts) ([][]byte, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("CMD GETKEYRANGE: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_GETKEYRANGE, cmd)
	cmdRange := &kinetic_proto.Command_Range{}
	SetCmdRange(cmdRange, startKey, endKey, startKeyInclusive, endKeyInclusive, maxReturned, reverse)
	cmdBody := &kinetic_proto.Command_Body{
		Range: cmdRange,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err := c.Send(message, value1, 0)
	if err != nil {
                //log.Println("1. ERROR: GETKEYRANGE FAILED")
		return nil, err
	}
	value, _, err := c.GetStatus()

	if err != nil {
		return nil, err
	}
	return value, err
}

func (c *Client) GetLog(ltype []kinetic_proto.Command_GetLog_Type, value []byte, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD GETLOG: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_GETLOG, cmd)
	if err != nil {
		return 0, err
	}
	getLog := &kinetic_proto.Command_GetLog{
		Types: ltype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		GetLog: getLog,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		return 0, err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("GETLOG FAILED")
		return 0, err
	}
	/*
		body := kcmd.GetBody()
		getlog := body.GetGetLog()
		logtype := getlog.GetTypes()
		//log.Println(" LOG:")
		for i := range logtype {
			//log.Println(string(i))
		}
		//log.Println(getlog.String())
	*/
	return 0, err
}

func (c *Client) NoOp(cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD NOOP: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_NOOP, cmd)
	if err != nil {
		return err
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) MediaScan(startkey, endkey []byte, startKeyInclusive,
	endKeyInclusive bool, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD MEDIA SCAN: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_MEDIASCAN, cmd)
	if err != nil {
		return err
	}
	cmdRange := &kinetic_proto.Command_Range{
		StartKey:          startkey,
		EndKey:            endkey,
		StartKeyInclusive: &startKeyInclusive,
		EndKeyInclusive:   &endKeyInclusive,
	}
	cmdBody := &kinetic_proto.Command_Body{
		Range: cmdRange,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) MediaOptimize(cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD MEDIA OPTIMIZE: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_MEDIAOPTIMIZE, cmd)
	if err != nil {
		return err
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) FlushAllData(cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD FLUSH ALL DATA: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_FLUSHALLDATA, cmd)
	if err != nil {
		return err
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) UnlockPin(pin []byte, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD UNLOCK PIN: ")
	authType := kinetic_proto.Message_PINAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_PINOP, cmd)
	//cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_UNLOCK_PINOP
	pinOp := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		PinOp: pinOp,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send  command, value1 is fake and will not be sent
	err := c.Send(message, value1, 0)
	if err != nil {
		return err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("UNLOCK PIN FAILED")
		return err
	}
	return err
}

func (c *Client) LockPin(pin []byte, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD LOCK PIN: ")
	authType := kinetic_proto.Message_PINAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_PINOP, cmd)
	if err != nil {
		return err
	}
	//cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_LOCK_PINOP
	pinOp := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		PinOp: pinOp,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	return err
}

func (c *Client) ErasePin(pin []byte, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD ERASE PIN: ")
	authType := kinetic_proto.Message_PINAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_PINOP, cmd)
	if err != nil {
		return err
	}
	//cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_ERASE_PINOP
	pinOp := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		PinOp: pinOp,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		return err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("ERASE PIN FAILED")
		return err
	}
	return err
}

func (c *Client) InstantSecureErase(pin []byte, cmd Opts) error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD ERASE: ")
	authType := kinetic_proto.Message_PINAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_PINOP, cmd)
	if err != nil {
		return err
	}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_ERASE_PINOP
	pinOp := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmdBody := &kinetic_proto.Command_Body{
		PinOp: pinOp,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	pinauth := new(kinetic_proto.Message_PINauth)
	pinauth.Pin = pin
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		PinAuth:      pinauth,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		return err
	}
	_, _, err = c.GetStatus()
	return err
}

func (c *Client) GetPrevious(key string, value []byte, cmd Opts) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\nCMD GETPREVIOUS: ")
	authType := kinetic_proto.Message_HMACAUTH
	cmdHeader := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmdHeader, kinetic_proto.Command_GETPREVIOUS, cmd)
	if err != nil {
		return 0, err
	}
	cmdKeyValue := &kinetic_proto.Command_KeyValue{}
	err = SetCmdKeyValue(cmdKeyValue, []byte(key), kinetic_proto.Command_INVALID_ALGORITHM, kinetic_proto.Command_INVALID_SYNCHRONIZATION)
	if err != nil {
		return 0, err
	}

	cmdBody := &kinetic_proto.Command_Body{
		KeyValue: cmdKeyValue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmdHeader,
		Body:   cmdBody,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &authType,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		return 0, err
	}
	_, valueSize, err := c.GetStatus()
	if err != nil {
		//log.Println("GETPREVIOUS FAILED")
		return 0, err
	}
	err = Read(c.socket, value, valueSize)
	if err != nil {
		//log.Println("GETPREVIOUS: GET VALUE FAILED")
		return 0, err
	}
	return valueSize, err
}

func (c *Client) GetStatus() ([][]byte, uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("***GetStatus****")
	message := &kinetic_proto.Message{}
	valueSize, err := c.GetMessage(message)
	if err != nil {
		//log.Println(" FAILED GET MESSAGE")
		return nil, 0, err
	}
	authType := message.GetAuthType()
	switch authType {
	case kinetic_proto.Message_HMACAUTH:
		//log.Println(" TYPE: HMACAUTH")
	case kinetic_proto.Message_PINAUTH:
		//log.Println(" TYPE: PINAUTH")
	case kinetic_proto.Message_UNSOLICITEDSTATUS:
		//log.Println(" TYPE: UNSOLICITED")
	default:
		//log.Println(" TYPE: INVALID AUTH")
	}
	commandbytes := message.GetCommandBytes()
	command := &kinetic_proto.Command{}
	err = proto.Unmarshal(commandbytes, command)
	if err != nil {
		//log.Println("FAILED TO UNMARSHAL command: ", err)
		return nil, 0, err
	}
	header := command.GetHeader()
	//Header
	c.ConnectionID = header.GetConnectionID()
	//cluster_version := header.GetClusterVersion()
	messageType := header.GetMessageType()
	// TO DO: for different message type, need to return the value
	// which is in the body of  message.
	//log.Println(" CMD RESPONSE: ", messageType)
	//Status
	status := command.GetStatus()
	statusCode := status.GetCode()
	statusMessage := status.GetStatusMessage()
	//log.Println(" Connection ID:", c.ConnectionID)
	//log.Println(" Status code:  ", statusCode)
	//log.Println(" Status msg;    ", statusMessage)
	if statusCode != kinetic_proto.Command_Status_SUCCESS {
		return nil, 0, errors.New(statusMessage)
	}
	switch messageType {
	case kinetic_proto.Command_GET_RESPONSE:
		return nil, valueSize, err
	case kinetic_proto.Command_PUT_RESPONSE:
	case kinetic_proto.Command_DELETE_RESPONSE:
	case kinetic_proto.Command_GETNEXT_RESPONSE:
	case kinetic_proto.Command_GETPREVIOUS_RESPONSE:
	case kinetic_proto.Command_GETKEYRANGE_RESPONSE:
		body := command.GetBody()
		keyrange := body.GetRange()
		keys := keyrange.GetKeys()
		return keys, 0, nil
	case kinetic_proto.Command_GETVERSION_RESPONSE:
	case kinetic_proto.Command_SETUP_RESPONSE:
	case kinetic_proto.Command_GETLOG_RESPONSE:
		body := command.GetBody()
		getlog := body.GetGetLog()
		logtype := getlog.GetTypes()
		//log.Println(" LOG TYPE:  ")
		for i := range logtype {
			log.Println("  ******* ", logtype[i].String())
			switch logtype[i] {
			case kinetic_proto.Command_GetLog_UTILIZATIONS:
				util := getlog.GetUtilizations()
				for i := range util {
					log.Println("  NAME: ", util[i].GetName())
					log.Println("  VALUE: ", util[i].GetValue())
				}

			case kinetic_proto.Command_GetLog_TEMPERATURES:
			case kinetic_proto.Command_GetLog_CAPACITIES:
				capacity := getlog.GetCapacity()
				//log.Println("  NOMIAL CAPACITY: ", capacity.GetNominalCapacityInBytes())
				log.Println("  PORTION FULL:    ", capacity.GetPortionFull())
			case kinetic_proto.Command_GetLog_CONFIGURATION:
			case kinetic_proto.Command_GetLog_STATISTICS:
			case kinetic_proto.Command_GetLog_MESSAGES:
				msg := getlog.GetMessages()
				log.Println(string(msg))
			case kinetic_proto.Command_GetLog_LIMITS:
				limits := getlog.GetLimits()
				log.Println("  MAX KEY SIZE:   ", limits.GetMaxKeySize())
				log.Println("  MAX VALUE SIZE: ", limits.GetMaxValueSize())
				//TO DO:
			case kinetic_proto.Command_GetLog_DEVICE:
			default:
				//log.Println("GETLOG FAILED")
				return nil, 0, nil
			}
		}
	case kinetic_proto.Command_SECURITY_RESPONSE:
	case kinetic_proto.Command_PEER2PEERPUSH_RESPONSE:
	case kinetic_proto.Command_NOOP_RESPONSE:
	case kinetic_proto.Command_FLUSHALLDATA_RESPONSE:
	case kinetic_proto.Command_PINOP_RESPONSE:
	case kinetic_proto.Command_MEDIASCAN_RESPONSE:
	case kinetic_proto.Command_MEDIAOPTIMIZE_RESPONSE:
	case kinetic_proto.Command_START_BATCH_RESPONSE:
	case kinetic_proto.Command_END_BATCH_RESPONSE:
	case kinetic_proto.Command_ABORT_BATCH_RESPONSE:
	case kinetic_proto.Command_SET_POWER_LEVEL_RESPONSE:
	default:
		//log.Println(" DEFAULT")
		return nil, 0, errors.New("COMMAND NOT FOUND")
	}
	//log.Println("***GetStatus return OK")
	return nil, 0, nil
}

func (c *Client) GetHeader() (uint32, uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	header := make([]byte, 9)
	err := Read(c.socket, header, 9)
	//log.Println(" READ HEADER")
	if (err != nil) || (header[0] != 'F') {
		//log.Println(" Message is NOK ", err, " ", header[0], " ", 'F')
		return  0, 0, err
	}
	var messageSize uint32 = binary.BigEndian.Uint32(header[1:5])
	var valueSize uint32 = binary.BigEndian.Uint32(header[5:9])
	//log.Println(" MSG_SIZE ", messageSize, " VALUE SIZE ", valueSize)
	return messageSize, valueSize, err
}

func (c *Client) GetMessage(message *kinetic_proto.Message) (uint32, error) {
        defer common.KUntrace(common.KTrace("Enter"))
	// IGNORE CALCULATE HMAC TO MAKE SURE MESSAGE is OK
	//log.Println(" GET HEADER")
	messageSize, valueSize, err := c.GetHeader()
	if err != nil {
		log.Fatal(" BAD HEADER")
		return 0, err
	}
	//log.Println(" MESSAGE SIZE ", messageSize)
	buff := make([]byte, messageSize)
	err = Read(c.socket, buff, messageSize)
	if err != nil {
		//log.Println(" FAILED TO GET MESSAGE")
		return 0, err
	}
	err = proto.Unmarshal(buff, message)
	if err != nil {
		//log.Println("FAILED TO MARSHAL MESSAGE")
		return 0, err
	}
	return valueSize, err
}

func (c *Client) GetSignOnMessageFor() error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("GET SIGN ON MESSAGE")
	message := &kinetic_proto.Message{}
	_, err := c.GetMessage(message)
	if err != nil {
		//log.Println(" FAILED GET MESSAGE")
		return err
	}
	authType := message.GetAuthType()
	if authType != kinetic_proto.Message_UNSOLICITEDSTATUS {
		//log.Println("BAD AUTH MESSAGE")
		return errors.New("BAD AUTH MESSAGE")
	}
	commandbytes := message.GetCommandBytes()
	command := &kinetic_proto.Command{}
	err = proto.Unmarshal(commandbytes, command)
	if err != nil {
		//log.Println("FAILED TO UNMARSHAL command: ", err)
		return err
	}
	header := command.GetHeader()
	//Header
	c.ConnectionID = header.GetConnectionID()
	//cluster_version := header.GetClusterVersion()
	//messageType := header.GetMessageType()
	///log.Println(" CMD RESPONSE: ", messageType)
	//Status
	status := command.GetStatus()
	statusCode := status.GetCode()
	statusMessage := status.GetStatusMessage()
	log.Println(" Connection ID ", c.ConnectionID)
	log.Println(" Status code: ", statusCode)
	log.Println(" Status msg;  ", statusMessage)
	return err
}

func (c *Client) GetSignOnMessage() error {
        defer common.KUntrace(common.KTrace("Enter"))
	//log.Println("\n SIGN ON MESSAGE: ")
	messageSize, valueSize, err := c.GetHeader()
	if err != nil {
		//log.Println(" BAD HEADER")
		return err
	}
	message := make([]byte, messageSize)
	value := make([]byte, valueSize)

	err = Read(c.socket, message, messageSize)
	if err != nil {
		return err
	}
	if valueSize > 0 {
		err = Read(c.socket, value, valueSize)
	}
	if err != nil {
		return err
	}
	response := &kinetic_proto.Message{}
	err = proto.Unmarshal(message, response)
	if err != nil {
		return err
	}
	hmac_auth := response.GetHmacAuth()
	c.Identity = hmac_auth.GetIdentity()
	//khmac := hmac_auth.GetHmac()
	commandbytes := response.GetCommandBytes()
	command := &kinetic_proto.Command{}
	err1 := proto.Unmarshal(commandbytes, command)
	if err1 != nil {
		return err1
	}
	header := command.GetHeader()
	//body := command.GetBody()
	status := command.GetStatus()
	//Header
	c.ConnectionID = header.GetConnectionID()
	//cluster_version := header.GetClusterVersion()
	messageType := header.GetMessageType()
	log.Println(" CMD RESPONSE: ", messageType)
	//sequence := header.GetSequence()
	ackSequence := header.GetAckSequence()
	log.Println(" ACK SEQ: ", ackSequence)
	//priority := header.GetPriority()
	//batchID := header.GetBatchID()
	//Body
	//keyValue := body.GetKeyValue()
	//krange := body.GetRange()
	//setup := body.GetSetup()
	//getlog := body.GetGetLog()
	//security := body.GetSecurity()
	//pinop := body.GetPinOp()
	//batch := body.GetBatch()
	//power := body.GetPower()
	//Batch
	//count := batch.GetCount()
	//batch_sequence := batch.GetSequence()
	//batch_failed_sequence := batch.GetFailedSequence()
	//Status
	statusCode := status.GetCode()
	statusMessage := status.GetStatusMessage()
	//detailed_message := status.GetDetailedMessage()
	//log.Println(" Connection ID ", c.ConnectionID)
	log.Println(" Status code: ", statusCode)
	log.Println(" Status msg;  ", statusMessage)
	//log.Println(" Detailed msg ", detailed_message)
	//log.Println(" Identity ", identity, " HMAC ", khmac)
	return err
}
