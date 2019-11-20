//package kinetic_client

package main

import (
	"fmt"
	"github.com/minio/minio/pkg/kinetic"
	"github.com/minio/minio/pkg/kinetic_proto"
	"os"
)

var c *kinetic.Client
var identity int64 = 1
var hmac_key string = "asdfasdf"

func Init(tls bool, ip string) (*kinetic.Client, error) {
	c = new(kinetic.Client)
	c.ConnectionID = 0
	c.Identity = identity
	c.Hmac_key = hmac_key
	if !tls {
		c.Connect(ip + ":8123")
	} else {
		c.TlsConnect(ip + ":8443")
	}
	err := c.GetSignOnMessageFor()
	if err != nil {
		return nil, err
	}
	return c, err
}

//* Test
func main() {

	ip := os.Args[1]
	fmt.Println(" ISE ip ", ip)
	tlsc, err := Init(true, ip)
	if err != nil {
		return
	}

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

	err = tlsc.InstantSecureErase(oldpin, cmdOpts)
	if err != nil {
		fmt.Println("FAILED: CMD ERASE WITH oldpin")
		return
	}
	fmt.Println("*** DONE ***")
}
