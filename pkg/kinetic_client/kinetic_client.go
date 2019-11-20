//package kinetic_client

package main

import (
	//"errors"
	"fmt"
	"github.com/minio/minio/pkg/kinetic"
	"github.com/minio/minio/pkg/kinetic_proto"
)

var c *kinetic.Client
var identity int64 = 1
var hmac_key string = "asdfasdf"

func Init(tls bool) (*kinetic.Client, error) {
	fmt.Println("Init client")
	c = new(kinetic.Client)
	c.ConnectionID = 0
	c.Identity = identity
	c.Hmac_key = hmac_key
	if !tls {
		c.Connect("127.0.0.1:8123")
	} else {
		c.TlsConnect("127.0.0.1:8443")
	}
	err := c.GetSignOnMessageFor()
	if err != nil {
		return nil, err
	}
	return c, err
}

//* Test
func main() {
	tlsc, err := Init(true)
	if err != nil {
		return
	}
	var value3 = make([]byte, 1024*1024)

	cmdOpts := kinetic.CmdOpts{
		ClusterVersion: 0,
		Timeout:        60000, //60 sec
		Priority:       kinetic_proto.Command_NORMAL,
		//cmd_keyvalue := &kinetic_proto.Command_KeyValue
		NewVersion:      []byte{'1', '.', '0', '.', '0'},
		Force:           true,
		DbVersion:       []byte{'2', '.', '0', '.', '0'},
		Tag:             []byte{},
		Algorithm:       kinetic_proto.Command_SHA1,
		MetaDataOnly:    false,
		Synchronization: kinetic_proto.Command_WRITETHROUGH,
	}
	oldpin := []byte{}
	newpin := []byte{1, 2, 3}

	fmt.Println("\n*** ERASE WITH oldpin *** ")
	err = tlsc.InstantSecureErase(oldpin, cmdOpts)
	if err != nil {
		fmt.Println("FAILED: CMD ERASE WITH oldpin")
		return
	}
	fmt.Println("   PASSED: ")
	fmt.Println("\n*** SET ERASE PIN TO newpin ***")
	tlsc.SetErasePin(oldpin, newpin, cmdOpts)
	if err != nil {
		fmt.Println("FAILED: CMD SET ERASE PIN =  '123'")
		return
	}
	fmt.Println("   PASSED: ")

	fmt.Println("\n*** ERASE WITH oldpin ***")
	err = tlsc.InstantSecureErase(oldpin, cmdOpts)
	if err != nil {
		fmt.Println("FAILED: CMD ERASE WITH oldpin = ''")
		return
	}
	fmt.Println("   PASSED: ")

	fmt.Println("\n*** ERASE WITH newpin ***")
	err = tlsc.InstantSecureErase(newpin, cmdOpts)
	if err != nil {
		fmt.Println("FAILED: CMD ERASE WITH newpin = '123'")
		return
	}
	fmt.Println("   PASSED: ")

	fmt.Println("\n*** SET ERASE PIN TO oldpin ***")
	err = tlsc.SetErasePin(newpin, oldpin, cmdOpts)
	if err != nil {
		fmt.Println("\nFAILED: SET ERASE PIN TO oldpin\n")
		return
	}
	fmt.Println("   PASSED: ")

	//return
	c, err = Init(false)
	//return
	//c.GetLog([]kinetic_proto.Command_GetLog_Type{kinetic_proto.Command_GetLog_UTILIZATIONS}, value3, cmdOpts)
	//return
	//c.GetLog([]kinetic_proto.Command_GetLog_Type{kinetic_proto.Command_GetLog_TEMPERATURES}, value3, cmdOpts)
	//c.GetLog([]kinetic_proto.Command_GetLog_Type{kinetic_proto.Command_GetLog_CAPACITIES}, value3, cmdOpts)
	//c.GetLog([]kinetic_proto.Command_GetLog_Type{kinetic_proto.Command_GetLog_LIMITS}, value3, cmdOpts)
	//c.GetLog([]kinetic.Command_GetLog_Type{kinetic.Command_GetLog_CAPACITIES,
	//	kinetic.Command_GetLog_MESSAGES}, value3, cmd)
	//PUT Test
	fmt.Println("****PUT TEST******")
	key := "123"
	value := "abc"
	_, err = c.Put(key, value, cmdOpts)
	//c.GetStatus()
	if err != nil {
		fmt.Println(" PUT FAILED: ", err)
		return
	}
	fmt.Printf("   PASSED: key %s  value %s\n ", key, value)
	key1 := "1234"
	value1 := "abcd"
	_, err = c.Put(key1, value1, cmdOpts)
	//c.GetStatus()
	if err != nil {
		fmt.Println(" 1. PUT FAILED: ", err)
		return
	}
	fmt.Printf("   PASSED: key %s  value %s\n ", key1, value1)

	key2 := "12345"
	value2 := "abcde"
	_, err = c.Put(key2, value2, cmdOpts)
	//c.GetStatus()
	if err != nil {
		fmt.Println(" 2. PUT FAILED: ", err)
		return
	}
	fmt.Printf("   PASSED: key %s  value %s\n ", key2, value2)

	//GET KEY RANGE test
	fmt.Println(" *** GET KEY RANGE TEST ***")
	c.GetKeyRange("", "", true, true, 200, false, value3, cmdOpts)
	//kinetic_cmds.GetStatus(&c)
	fmt.Println("    PASSED: GET KEY RANGE")

	fmt.Println(" *** GET TEST ***")
	//GET Test
	value_size, err := c.Get(key, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		fmt.Printf(" EXPECTED %s\n ", value)
		return
	}
	fmt.Printf("  PASSED GET TEST: key %s value %s\n", key, string(value3[:value_size]))

	//GET test
	value_size, err = c.Get(key1, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		fmt.Printf(" EXPECTED %s\n", value1)
		return
	}
	fmt.Printf("  PASSED GET TEST: key %s value %s\n", key1, string(value3[:value_size]))

	//GETNEXTTEST
	fmt.Println(" *** GET NEXT TEST ***")
	value_size, err = c.GetNext(key, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		fmt.Printf(" EXPECTED %s\n", value1)
		return
	}
	fmt.Printf("  PASSED: key %s value %s\n", key, string(value3[:value_size]))
	//GETPREVIOUS test
	fmt.Println(" *** GER PREVIOUS TEST ***")
	value_size, err = c.GetPrevious(key2, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		fmt.Printf(" EXPECTED %s\n", value1)
		return
	}
	fmt.Printf("  PASSED: key %s value %s\n", key2, string(value3[:value_size]))

	//GETNEXT test
	fmt.Println(" *** GET NEXT TEST ***")
	value_size, err = c.GetNext(key1, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		fmt.Printf(" EXPECTED %s\n", value2)
		return
	}
	fmt.Printf("  PASSED: key %s value %s\n", key1, string(value3[:value_size]))

	//GET key1
	fmt.Println(" **** GET TEST ***")
	value_size, err = c.Get(key1, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		return
	}
	//GET KEY2
	value_size, err = c.Get(key2, value3, cmdOpts)
	if err != nil {
		fmt.Printf(" VALUE RETURNED TO CALLER:\n %s\n", string(value3[:value_size]))
		return
	}
	fmt.Println(" PASSED: GET TEST")
	//BATCH TEST
	fmt.Println(" *** BATCH TEST ***")
	cmdOpts.BatchID = 0
	c.StartBatch(cmdOpts)
	c.GetStatus()

	c.AbortBatch(cmdOpts)
	c.GetStatus()

	c.StartBatch(cmdOpts)
	c.GetStatus()

	key4 := "4123"
	value4 := "4abc"
	c.PutB(key4, value4, cmdOpts)
	key5 := "5123"
	value5 := "5abc"
	c.PutB(key5, value5, cmdOpts)
	c.EndBatch(2, cmdOpts)
	c.GetStatus()

	c.FlushAllData(cmdOpts)
	c.GetStatus()
	c.NoOp(cmdOpts)
	c.GetStatus()
	//tlsc.SetLockPin(oldPin, newPin, cmd)
	//tlsc.GetStatus()
	start := []byte{}
	end := []byte{}
	c.MediaScan(start, end, false, false, cmdOpts)
	c.GetStatus()
	//c.MediaOptimize(cmdOpts)
	//c.GetStatus()
	//c.LockPin(newPin, cmdOpts)
	//.GetStatus()
	//c.UnlockPin(oldPin, cmdOpts)
	//c.GetStatus()
	//c.ErasePin(oldPin, cmdOpts)
	//c.GetStatus()
}
