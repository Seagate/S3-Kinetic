package cmd

import (
// #cgo CXXFLAGS: --std=c++0x  -DNDEBUG -DNDEBUGW -DSMR_ENABLED
// #cgo LDFLAGS: libkinetic.a kernel_mem_mgr.a libssl.a libcrypto.a libglog.a libgmock.a libgtest.a libsmrenv.a libleveldb.a libmemenv.a libkinetic_client.a zac_kin.a lldp_kin.a  qual_kin.a libprotobuf.a libgflags.a  libgflags_nothreads.a libprotoc.a libksapi.a libpbkdf.a libapi.a libtransports.a  libseapubcmds.a libapi.a -lpthread -ldl -lrt 
// #include "minio_skinny_waist.h"
       "C"
	"unsafe"
	"crypto/hmac"
	"crypto/sha1"
	"crypto/tls"
	"errors"
	"encoding/json"
	"fmt"
	"sync"
	"strings"
	//"time"
	//"crypto/x509"
	"encoding/binary"
	"github.com/golang/protobuf/proto"
	"github.com/minio/minio/pkg/kinetic_proto"
	"log" //"github.com/sirupsen/logrus"
	//"io"
	"net"
)

var mutexCmd = &sync.Mutex{}
var SkinnyWaistIF bool = false
var ptr **C.char

type CmdOpts struct {
	//Command         kinetic_proto.Command_MessageType
	ClusterVersion int64
	Timeout        uint64
	Priority       kinetic_proto.Command_Priority
	BatchID        uint32
	//cmd_keyvalue := &kinetic_proto.Command_KeyValue
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
	User_id      int
	Identity     int64
	Hmac_key     string
	ReadSize     int32
	WriteSize    int32
	Key          []byte
	Opts         CmdOpts
	ReleaseConn  func(int)
	NextPartNumber *int
	LastPartNumber int
}

func (c *Client) Read(value []byte) (int, error) {
        ///log.Println(" ****READ****", string(c.Key), c.LastPartNumber)
	//Key :=  string(value)
        ///log.Println(" ***KEY***", *(c.NextPartNumber))

	fsMeta := fsMetaV1{}
        cvalue, ptr, size, err := c.CGetMeta(string(c.Key), c.Opts)
        if err != nil {
                err = errFileNotFound
		if ptr != nil { 
			C.deallocate_gvalue_buffer((*C.char)(ptr))
		}
                c.ReleaseConn(c.Idx)
                return 0, err
        }
        //var fsMetaBytes []byte
        if (cvalue != nil) {
		fsMetaBytes := (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
		err = json.Unmarshal(fsMetaBytes[:size], &fsMeta)
	        C.deallocate_gvalue_buffer((*C.char)(ptr))
	}
	c.LastPartNumber =  len(fsMeta.Parts)
	if len(fsMeta.Parts) == 0 {
        //log.Println(" ****READ****", string(c.Key), c.LastPartNumber)

		cvalue, ptr, size, err := c.CGet(string(c.Key), c.Opts)
                if err != nil {
                        if ptr != nil {
				C.deallocate_gvalue_buffer((*C.char)(ptr))
			}
                        c.ReleaseConn(c.Idx)
                        return 0, err
                }
		if cvalue  != nil {
			value1 := (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
			copy(value, value1)
	                C.deallocate_gvalue_buffer((*C.char)(ptr))
                        c.ReleaseConn(c.Idx)
                        return int(size), err
                }
                c.ReleaseConn(c.Idx)
		return 0, err
	}

	for i, part := range  fsMeta.Parts {
		///log.Println(" PARTS > 0")
		if i == *(c.NextPartNumber) {
			///log.Println(" PART: ", i, part)
			key := string(c.Key)+ "." +  fmt.Sprintf("%.5d.%s.%d", part.Number, part.ETag, part.ActualSize)
			///log.Println(" KEY: ", key)
			cvalue, ptr1, size, err := c.CGet(key, c.Opts)
			if err != nil {
				///log.Println(" NOT FOUND")
				if ptr1 != nil {
					 C.deallocate_gvalue_buffer((*C.char)(ptr1))
				}
				c.ReleaseConn(c.Idx)
				return 0, err
			}
			if cvalue != nil {
				value1 := (*[1 << 20 ]byte)(unsafe.Pointer(cvalue))[:size:size]
				copy(value, value1)
				///log.Println(" VAL SIZE", len(value))
				C.deallocate_gvalue_buffer((*C.char)(ptr1))
				if i ==  len(fsMeta.Parts) -1 {
					///log.Println(" Release Connection")
					*(c.NextPartNumber) = 0
				 c.ReleaseConn(c.Idx)
				} else {
				        *(c.NextPartNumber) += 1
				}
				return int(size), err
			}
		}
	}
        c.ReleaseConn(c.Idx)
	return 0, err
}


func (c *Client) Write(p []byte, size int) (int, error) {
	n, err := c.Put(string(c.Key), p, size, c.Opts)
	return int(n), err
}

func (c *Client) Close() {
	c.socket.Close()
}

func ComputeHMAC(msg, hmac_key []byte) []byte {
	var msg_size uint32 = uint32(len(msg))
	size := make([]byte, 4, 5)
	binary.BigEndian.PutUint32(size[1:5], msg_size)
	h := hmac.New(sha1.New, []byte(hmac_key))
	h.Write(size[1:5])
	h.Write(msg)
	khmac := h.Sum(nil)
	return khmac
}

func (c *Client) Send(msg *kinetic_proto.Message, value []byte, size int) error {
	if *msg.AuthType == kinetic_proto.Message_HMACAUTH {
		khmac := ComputeHMAC(msg.CommandBytes, []byte(c.Hmac_key))
		message_HMACauth := &kinetic_proto.Message_HMACauth{
			Identity: &c.Identity,
			Hmac:     khmac,
		}
		msg.HmacAuth = message_HMACauth
	}
	msgMarshal, _ := proto.Marshal(msg)
	var msg_size uint32 = uint32(len(msgMarshal))
	var value_size uint32 = uint32(size)
	tx_header := make([]byte, 9)
	tx_header[0] = 'F'
	binary.BigEndian.PutUint32(tx_header[1:5], msg_size)
	binary.BigEndian.PutUint32(tx_header[5:9], value_size)
	//log.Println(" WRITE SOCK HEADER")
	err := Write(c.socket, tx_header, 9)
	if err != nil {
		return err
	}
        //log.Println(" WRITE SOCK MID")

	err = Write(c.socket, msgMarshal, msg_size)
	if err != nil {
		return err
	}
        //log.Println(" WRITE SOCK VALUE")

	err = Write(c.socket, value, value_size)
	return err
}

func Read(socket net.Conn, buffer []byte, size uint32) error {
	var bytesRead uint32 = 0
	var err error = nil
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
	var bytesWritten uint32 = 0
	var err error = nil
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
	mutexCmd.Lock()
	//log.Println("Starting client...")
	var err error = nil
	conn, err := net.Dial("tcp", address)
	if err != nil {
		//log.Println("FAILED TO CONNECT")
		///log.Println(err)
		mutexCmd.Unlock()
		return err
	}
	c.socket = conn
	mutexCmd.Unlock()
	return err
}

func (c *Client) TlsConnect(address string) error {
	mutexCmd.Lock()
	//log.Println(" TLS connection")
	var err error = nil
	cert, err := tls.LoadX509KeyPair("selfsigned.crt", "selfsigned.key")
	if err != nil {
		mutexCmd.Unlock()
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
		mutexCmd.Unlock()
		return err
	}
	c.socket = conn
	mutexCmd.Unlock()
	return err
}

func SetCmdInHeader(c *Client, header *kinetic_proto.Command_Header, cmdtype kinetic_proto.Command_MessageType, cmd CmdOpts) error {
	//cmd_header := &kinetic_proto.Command_Header{
	//              ClusterVersion: 0,
	//              ConnectionID: &c.ConnectionID,
	//              Sequence:     &c.Sequence,
	//		MessageType:  new(kinetic_proto.Command_MessageType),
	//}
	var err error = nil
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
	// Keep these comments:
	//cmd_keyvalue := &kinetic_proto.Command_KeyValue
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

func (c *Client) SetLockPin(oldpin, newpin []byte, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD SET LOCK PIN: ")
	var err error = nil
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err = SetCmdInHeader(c, cmd_header, kinetic_proto.Command_SECURITY, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	securityoptype := new(kinetic_proto.Command_Security_SecurityOpType)
	*securityoptype = kinetic_proto.Command_Security_LOCK_PIN_SECURITYOP
	cmd_security := &kinetic_proto.Command_Security{
		OldLockPIN:     oldpin,
		NewLockPIN:     newpin,
		SecurityOpType: securityoptype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		Security: cmd_security,
	}

	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) SetErasePin(oldpin, newpin []byte, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD SET ERASE PIN: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_SECURITY, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	securityoptype := new(kinetic_proto.Command_Security_SecurityOpType)
	*securityoptype = kinetic_proto.Command_Security_ERASE_PIN_SECURITYOP
	cmd_security := &kinetic_proto.Command_Security{
		OldErasePIN:    oldpin,
		NewErasePIN:    newpin,
		SecurityOpType: securityoptype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		Security: cmd_security,
	}

	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	_, _, err = c.GetStatus()
	mutexCmd.Unlock()
	return err
}

func (c *Client) StartBatch(cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD START BATCH: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_START_BATCH, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	cmd_header.BatchID = new(uint32)
	*cmd_header.BatchID = cmd.BatchID
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) EndBatch(count uint32, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD END BATCH: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_END_BATCH, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	cmd_header.BatchID = new(uint32)
	*cmd_header.BatchID = cmd.BatchID
	cmd_batch := &kinetic_proto.Command_Batch{}
	cmd_batch.Count = new(uint32)
	*cmd_batch.Count = count
	cmd_body := &kinetic_proto.Command_Body{
		Batch: cmd_batch,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) AbortBatch(cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD ABORT BATCH: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_ABORT_BATCH, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	cmd_header.BatchID = new(uint32)
	*cmd_header.BatchID = cmd.BatchID
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) CGetMeta(key string, acmd CmdOpts) (*C.char, *C.char, uint32, error) {
	metaKey := "meta." + key
	return c.CGet(metaKey, acmd)
}


// Use this for Skinny Waist interface
func (c *Client) CGet(key string, acmd CmdOpts) (*C.char, *C.char, uint32, error) {
	mutexCmd.Lock()
        /////log.Println(" CALL CGET ", key)
        var psv C._CPrimaryStoreValue
        psv.version = C.CString(string(acmd.NewVersion))
        psv.tag = C.CString(string(acmd.Tag))
        psv.algorithm = C.int(acmd.Algorithm)
        c_key := C.CString(key)
	var size  int
	var status int
	var cvalue *C.char
        cvalue = C.Get(1, c_key, &psv, &ptr, (*C.int)(unsafe.Pointer(&size)), (*C.int)(unsafe.Pointer(&status)))
	mutexCmd.Unlock()
	var err error = nil
	if status != 0 || cvalue == nil {
		err =  errors.New("NOT FOUND")
	}
        /////log.Println(" CGET DONE ", err, cvalue, *ptr)
        return cvalue, *ptr, uint32(size), err                   
}


//Use this for Kinetic API 
func (c *Client) Get(key string, value []byte, cmd CmdOpts) (uint32, error) {
	mutexCmd.Lock()
        /////log.Println(" NORMAL GET")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_GET, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}
	cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	SetCmdKeyValue(cmd_keyvalue, []byte(key), kinetic_proto.Command_INVALID_ALGORITHM, kinetic_proto.Command_INVALID_SYNCHRONIZATION)
	cmd_body := &kinetic_proto.Command_Body{
		KeyValue: cmd_keyvalue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}
	_, value_size, err := c.GetStatus()
	if err != nil {
		//log.Println("GET STATUS FAILED")
		mutexCmd.Unlock()
		return 0, err
	}
	err = Read(c.socket, value, value_size)
	mutexCmd.Unlock()
	if err != nil {
		return 0, err
	}
	return value_size, err
}

func (c *Client) CDelete(key string, cmd CmdOpts)  error {
        mutexCmd.Lock()
        current_version := "1"
        c_key := C.CString(key)
        C.Delete(1, c_key, C.CString(current_version), false, 1, 1)
        mutexCmd.Unlock()
        return  nil
}

func (c *Client) Delete(key string, cmd CmdOpts) error {
        if SkinnyWaistIF {
                return c.CDelete(key, cmd)
        }
	mutexCmd.Lock()
	/////log.Println("CMD DELETE: ", key)
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_DELETE, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	var force bool = cmd.Force
	cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	cmd_keyvalue.Force = &force
	SetCmdKeyValue(cmd_keyvalue, []byte(key), cmd.Algorithm, cmd.Synchronization)
	cmd_body := &kinetic_proto.Command_Body{
		KeyValue: cmd_keyvalue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value []byte
	err = c.Send(message, value, 0)

	if err != nil {
		//log.Println("SENT FAILED")
		mutexCmd.Unlock()
		return err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("GET STTAUS FAILED")
		mutexCmd.Unlock()
		return err
	}
	mutexCmd.Unlock()
	return err
}

func (c *Client) CPutMeta(key string, value []byte, size int, cmd CmdOpts) (uint32, error) {
	metaKey := "meta." + key
	return c.CPut(metaKey, value, size, cmd)

}

func (c *Client) CPut(key string, value []byte, size int, cmd CmdOpts) (uint32, error) {
	//start := time.Now()
	mutexCmd.Lock()
	var psv C._CPrimaryStoreValue
	psv.version = C.CString(string(cmd.NewVersion))
	psv.tag = C.CString(string(cmd.Tag))
	psv.algorithm = C.int(cmd.Algorithm)
        current_version := "1"
	c_key := C.CString(key)
	var c_value *C.char
	if  size  > 0 {
		c_value = (*C.char)(unsafe.Pointer(&value[0]))
	} else {
		c_value  = (*C.char)(nil)
	}
	C.Put(1, c_key, C.CString(current_version), &psv, c_value, C.size_t(size), false, 1, 1)
	mutexCmd.Unlock()
        ///log.Println("MINIO CPUT DONE ", time.Since(start))
        return uint32(size), nil
}


func (c *Client) Put(key string, value []byte, size int, cmd CmdOpts) (uint32, error) {
        if SkinnyWaistIF {
		return c.CPut(key, value, size, cmd)
	}
	mutexCmd.Lock()
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_PUT, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}
	var force bool = cmd.Force
	cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	cmd_keyvalue.NewVersion = cmd.NewVersion
	cmd_keyvalue.Force = &force
	cmd_keyvalue.Algorithm = &cmd.Algorithm
	cmd_keyvalue.Synchronization = &cmd.Synchronization
	cmd_keyvalue.Key = []byte(key)
	cmd_keyvalue.Tag = cmd.Tag

	cmd_body := &kinetic_proto.Command_Body{
		KeyValue: cmd_keyvalue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	err = c.Send(message, value, size)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}

	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("PUT FAILED")
		mutexCmd.Unlock()
		return 0, err
	}

	mutexCmd.Unlock()
	return uint32(size), nil
}


func (c *Client) PutB(key string, value []byte, size int, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD PUT BATCH: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_PUT, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	cmd_header.BatchID = new(uint32)
	*cmd_header.BatchID = cmd.BatchID
	var force bool = cmd.Force
	cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	cmd_keyvalue.Force = &force
	SetCmdKeyValue(cmd_keyvalue, []byte(key), cmd.Algorithm, cmd.Synchronization)
	cmd_body := &kinetic_proto.Command_Body{
		KeyValue: cmd_keyvalue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	err = c.Send(message, value, size )
	mutexCmd.Unlock()
	return err
}

func (c *Client) GetNext(key string, value []byte, cmd CmdOpts) (uint32, error) {
	mutexCmd.Lock()
	//log.Println("\nCMD GETNEXT: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	SetCmdInHeader(c, cmd_header, kinetic_proto.Command_GETNEXT, cmd)
	cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	SetCmdKeyValue(cmd_keyvalue, []byte(key), kinetic_proto.Command_INVALID_ALGORITHM, kinetic_proto.Command_INVALID_SYNCHRONIZATION)
	cmd_body := &kinetic_proto.Command_Body{
		KeyValue: cmd_keyvalue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err := c.Send(message, value1, 0)
	//mutexCmd.Unlock()
	if err != nil {
		return 0, err
	}
	_, value_size, err := c.GetStatus()
	if err != nil {
		//log.Println("GETNEXT FAILED")
		return 0, err
	}
	//log.Println(" VALUE SIZE ", value_size)
	//mutexCmd.Lock()
	err = Read(c.socket, value, value_size)
	mutexCmd.Unlock()
	if err != nil {
		//log.Println("GETNEXT: READ VALUE FAILED")
		return 0, err
	}
	//log.Println(" VALUE: ", string(value[:value_size]))
	return value_size, err
}

// TODO: Use this to interface to Skinny Waist
func (c *Client) CGetKeyRange(startKey string, endKey string, startKeyInclusive bool, endKeyInclusive bool, maxReturned uint32, reverse bool, cmd CmdOpts) ([][]byte, error) {
	c_startKey := C.CString(startKey)
	c_endKey := C.CString(endKey)
	Keys  := make([]byte, 1024*1024)
	c_Keys :=  (*C.char)(unsafe.Pointer(&Keys[0]))
	var size int = 0
	c_size := (*C.int)(unsafe.Pointer(&size))
	C.GetKeyRange(1, c_startKey, c_endKey, C.bool(startKeyInclusive), C.bool(endKeyInclusive), 800, false, c_Keys, c_size)
        Keys = (*[1 << 20 ]byte)(unsafe.Pointer(c_Keys))[:size:size]
	keyStrings := strings.Split(string(Keys[:size]), ":")
        //log.Println(" KEYS: ", string(Keys[:size]))
	var keys [][]byte
	for i := range  keyStrings {
		//log.Println("KEY ", keyStrings[i])
		if len(keyStrings[i]) > 0 {
			keys = append(keys, []byte(keyStrings[i]))
		}
	}
	return keys, nil
}



func (c *Client) GetKeyRange(startKey string, endKey string, startKeyInclusive bool, endKeyInclusive bool, maxReturned uint32, reverse bool, cmd CmdOpts) ([][]byte, error) {
	mutexCmd.Lock()
	//log.Println("CMD GETKEYRANGE: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	SetCmdInHeader(c, cmd_header, kinetic_proto.Command_GETKEYRANGE, cmd)
	cmd_range := &kinetic_proto.Command_Range{}
	SetCmdRange(cmd_range, startKey, endKey, startKeyInclusive, endKeyInclusive, maxReturned, reverse)
	cmd_body := &kinetic_proto.Command_Body{
		Range: cmd_range,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err := c.Send(message, value1, 0)
        //log.Println("0. GETKEYRANGE", err)

	if err != nil {
                //log.Println("1. ERROR: GETKEYRANGE FAILED")
		mutexCmd.Unlock()
		return nil, err
	}
	value, _, err := c.GetStatus()
        //log.Println("1. GETKEYRANGE", err)

	if err != nil {
		//log.Println("2. ERROR: GETKEYRANGE FAILED")
		mutexCmd.Unlock()
		return nil, err
	}
        //log.Println("END: CMD GETKEYRANGE: ")

	// TO DO: Should return the keys to caller.
	mutexCmd.Unlock()
	return value, err
}

func (c *Client) GetLog(ltype []kinetic_proto.Command_GetLog_Type, value []byte, cmd CmdOpts) (uint32, error) {
	mutexCmd.Lock()
	//log.Println("\nCMD GETLOG: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_GETLOG, cmd)
	if err != nil {
		return 0, err
	}
	get_log := &kinetic_proto.Command_GetLog{
		Types: ltype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		//		KeyValue: cmd_keyvalue,
		GetLog: get_log,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
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
		mutexCmd.Unlock()
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
	mutexCmd.Unlock()

	return 0, err
}

func (c *Client) NoOp(cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD NOOP: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_NOOP, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) MediaScan(startkey, endkey []byte, startKeyInclusive,
	endKeyInclusive bool, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD MEDIA SCAN: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_MEDIASCAN, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	cmd_range := &kinetic_proto.Command_Range{
		StartKey:          startkey,
		EndKey:            endkey,
		StartKeyInclusive: &startKeyInclusive,
		EndKeyInclusive:   &endKeyInclusive,
	}
	cmd_body := &kinetic_proto.Command_Body{
		Range: cmd_range,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) MediaOptimize(cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD MEDIA OPTIMIZE: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_MEDIAOPTIMIZE, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send Get command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) FlushAllData(cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD FLUSH ALL DATA: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_FLUSHALLDATA, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) UnlockPin(pin []byte, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD UNLOCK PIN: ")
	auth_type := kinetic_proto.Message_PINAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	SetCmdInHeader(c, cmd_header, kinetic_proto.Command_PINOP, cmd)
	//cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_UNLOCK_PINOP
	pin_op := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		PinOp: pin_op,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send  command, value1 is fake and will not be sent
	err := c.Send(message, value1, 0)
	// mutexCmd.Unlock()
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("UNLOCK PIN FAILED")
		mutexCmd.Unlock()
		return err
	}
	mutexCmd.Unlock()
	return err
}

func (c *Client) LockPin(pin []byte, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD LOCK PIN: ")
	auth_type := kinetic_proto.Message_PINAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_PINOP, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	//cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_LOCK_PINOP
	pin_op := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		PinOp: pin_op,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	mutexCmd.Unlock()
	return err
}

func (c *Client) ErasePin(pin []byte, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD ERASE PIN: ")
	auth_type := kinetic_proto.Message_PINAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_PINOP, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	//cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_ERASE_PINOP
	pin_op := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		PinOp: pin_op,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	_, _, err = c.GetStatus()
	if err != nil {
		//log.Println("ERASE PIN FAILED")
		mutexCmd.Unlock()
		return err
	}
	mutexCmd.Unlock()
	return err
}

func (c *Client) InstantSecureErase(pin []byte, cmd CmdOpts) error {
	mutexCmd.Lock()
	//log.Println("\nCMD ERASE: ")
	auth_type := kinetic_proto.Message_PINAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_PINOP, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	optype := new(kinetic_proto.Command_PinOperation_PinOpType)
	*optype = kinetic_proto.Command_PinOperation_ERASE_PINOP
	pin_op := &kinetic_proto.Command_PinOperation{
		PinOpType: optype,
	}
	cmd_body := &kinetic_proto.Command_Body{
		PinOp: pin_op,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	pinauth := new(kinetic_proto.Message_PINauth)
	pinauth.Pin = pin
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		PinAuth:      pinauth,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	_, _, err = c.GetStatus()
	mutexCmd.Unlock()
	return err
}

func (c *Client) GetPrevious(key string, value []byte, cmd CmdOpts) (uint32, error) {
	mutexCmd.Lock()
	//log.Println("\nCMD GETPREVIOUS: ")
	auth_type := kinetic_proto.Message_HMACAUTH
	cmd_header := &kinetic_proto.Command_Header{}
	err := SetCmdInHeader(c, cmd_header, kinetic_proto.Command_GETPREVIOUS, cmd)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}
	cmd_keyvalue := &kinetic_proto.Command_KeyValue{}
	err = SetCmdKeyValue(cmd_keyvalue, []byte(key), kinetic_proto.Command_INVALID_ALGORITHM, kinetic_proto.Command_INVALID_SYNCHRONIZATION)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}

	cmd_body := &kinetic_proto.Command_Body{
		KeyValue: cmd_keyvalue,
	}
	kcmd := &kinetic_proto.Command{
		Header: cmd_header,
		Body:   cmd_body,
	}
	commandbytes, _ := proto.Marshal(kcmd)
	message := &kinetic_proto.Message{
		AuthType:     &auth_type,
		CommandBytes: commandbytes,
	}
	var value1 []byte
	// Send command, value1 is fake and will not be sent
	err = c.Send(message, value1, 0)
	if err != nil {
		mutexCmd.Unlock()
		return 0, err
	}
	_, value_size, err := c.GetStatus()
	if err != nil {
		//log.Println("GETPREVIOUS FAILED")
		return 0, err
	}
	//log.Println(" VALUE SIZE ", value_size)
	err = Read(c.socket, value, value_size)
	if err != nil {
		//log.Println("GETPREVIOUS: GET VALUE FAILED")
		mutexCmd.Unlock()
		return 0, err
	}
	//log.Println(" VALUE: ", string(value[:value_size]))
	mutexCmd.Unlock()
	return value_size, err
}

func (c *Client) GetStatus() ([][]byte, uint32, error) {
	//mutexCmd.Lock()
	//log.Println("***GetStatus****")
	message := &kinetic_proto.Message{}
	value_size, err := c.GetMessage(message)
	if err != nil {
		//log.Println(" FAILED GET MESSAGE")
		//mutexCmd.Unlock()
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
		//mutexCmd.Unlock()
		return nil, 0, err
	}
	header := command.GetHeader()
	//Header
	c.ConnectionID = header.GetConnectionID()
	//cluster_version := header.GetClusterVersion()
	message_type := header.GetMessageType()
	// TO DO: for different message type, need to return the value
	// which is in the body of  message.
	//log.Println(" CMD RESPONSE: ", message_type)
	//Status
	status := command.GetStatus()
	status_code := status.GetCode()
	status_message := status.GetStatusMessage()
	//log.Println(" Connection ID:", c.ConnectionID)
	//log.Println(" Status code:  ", status_code)
	//log.Println(" Status msg;    ", status_message)
	if status_code != kinetic_proto.Command_Status_SUCCESS {
		//mutexCmd.Unlock()
		return nil, 0, errors.New(status_message)
	}
	switch message_type {
	case kinetic_proto.Command_GET_RESPONSE:
		return nil, value_size, err
	case kinetic_proto.Command_PUT_RESPONSE:
	case kinetic_proto.Command_DELETE_RESPONSE:
	case kinetic_proto.Command_GETNEXT_RESPONSE:
	case kinetic_proto.Command_GETPREVIOUS_RESPONSE:
	case kinetic_proto.Command_GETKEYRANGE_RESPONSE:
		body := command.GetBody()
		keyrange := body.GetRange()
		keys := keyrange.GetKeys()
		//mutexCmd.Unlock()
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
				//mutexCmd.Unlock()
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
		//mutexCmd.Unlock()
		return nil, 0, errors.New("COMMAND NOT FOUND")
	}
	//mutexCmd.Unlock()
	//log.Println("***GetStatus return OK")
	return nil, 0, nil
}

func (c *Client) GetHeader() (error, uint32, uint32) {
	header := make([]byte, 9)
	err := Read(c.socket, header, 9)
	//log.Println(" READ HEADER")
	if (err != nil) || (header[0] != 'F') {
		//log.Println(" Message is NOK ", err, " ", header[0], " ", 'F')
		return err, 0, 0
	}
	var message_size uint32 = binary.BigEndian.Uint32(header[1:5])
	var value_size uint32 = binary.BigEndian.Uint32(header[5:9])
	//log.Println(" MSG_SIZE ", message_size, " VALUE SIZE ", value_size)
	return err, message_size, value_size
}

func (c *Client) GetMessage(message *kinetic_proto.Message) (uint32, error) {
	// IGNORE CALCULATE HMAC TO MAKE SURE MESSAGE is OK
	//log.Println(" GET HEADER")
	err, message_size, value_size := c.GetHeader()
	if err != nil {
		log.Fatal(" BAD HEADER")
		return 0, err
	}
	//log.Println(" MESSAGE SIZE ", message_size)
	buff := make([]byte, message_size)
	err = Read(c.socket, buff, message_size)
	if err != nil {
		//log.Println(" FAILED TO GET MESSAGE")
		return 0, err
	}
	err = proto.Unmarshal(buff, message)
	if err != nil {
		//log.Println("FAILED TO MARSHAL MESSAGE")
		return 0, err
	}
	return value_size, err
}

func (c *Client) GetSignOnMessageFor() error {
	mutexCmd.Lock()
	//log.Println("GET SIGN ON MESSAGE")
	message := &kinetic_proto.Message{}
	_, err := c.GetMessage(message)
	if err != nil {
		//log.Println(" FAILED GET MESSAGE")
		mutexCmd.Unlock()
		return err
	}
	authType := message.GetAuthType()
	if authType != kinetic_proto.Message_UNSOLICITEDSTATUS {
		//log.Println("BAD AUTH MESSAGE")
		mutexCmd.Unlock()
		return errors.New("BAD AUTH MESSAGE")
	}
	commandbytes := message.GetCommandBytes()
	command := &kinetic_proto.Command{}
	err = proto.Unmarshal(commandbytes, command)
	if err != nil {
		//log.Println("FAILED TO UNMARSHAL command: ", err)
		mutexCmd.Unlock()
		return err
	}
	header := command.GetHeader()
	//Header
	c.ConnectionID = header.GetConnectionID()
	//cluster_version := header.GetClusterVersion()
	//message_type := header.GetMessageType()
	///log.Println(" CMD RESPONSE: ", message_type)
	//Status
	status := command.GetStatus()
	status_code := status.GetCode()
	status_message := status.GetStatusMessage()
	log.Println(" Connection ID ", c.ConnectionID)
	log.Println(" Status code: ", status_code)
	log.Println(" Status msg;  ", status_message)
	mutexCmd.Unlock()
	return err
}

func (c *Client) GetSignOnMessage() error {
	mutexCmd.Lock()
	//log.Println("\n SIGN ON MESSAGE: ")
	err, message_size, value_size := c.GetHeader()
	if err != nil {
		//log.Println(" BAD HEADER")
		mutexCmd.Unlock()
		return err
	}
	message := make([]byte, message_size)
	value := make([]byte, value_size)

	err = Read(c.socket, message, message_size)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	if value_size > 0 {
		err = Read(c.socket, value, value_size)
		//log.Println(" VALUE GET: ", string(value))
	}
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	///log.Println("READ MSG OK")
	response := &kinetic_proto.Message{}
	err = proto.Unmarshal(message, response)
	if err != nil {
		mutexCmd.Unlock()
		return err
	}
	//log.Println("1. READ MSG OK")
	hmac_auth := response.GetHmacAuth()
	c.Identity = hmac_auth.GetIdentity()
	//khmac := hmac_auth.GetHmac()
	commandbytes := response.GetCommandBytes()
	command := &kinetic_proto.Command{}
	err1 := proto.Unmarshal(commandbytes, command)
	if err1 != nil {
		mutexCmd.Unlock()
		return err1
	}
	header := command.GetHeader()
	//body := command.GetBody()
	status := command.GetStatus()
	//Header
	c.ConnectionID = header.GetConnectionID()
	//cluster_version := header.GetClusterVersion()
	message_type := header.GetMessageType()
	log.Println(" CMD RESPONSE: ", message_type)
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
	status_code := status.GetCode()
	status_message := status.GetStatusMessage()
	//detailed_message := status.GetDetailedMessage()
	//log.Println(" Connection ID ", c.ConnectionID)
	log.Println(" Status code: ", status_code)
	log.Println(" Status msg;  ", status_message)
	//log.Println(" Detailed msg ", detailed_message)
	//log.Println(" Identity ", identity, " HMAC ", khmac)
	mutexCmd.Unlock()
	return err
}
