// +build go1.13

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

/*
 * Below main package has canonical imports for 'go get' and 'go build'
 * to work with all other clones of github.com/minio/minio repository. For
 * more information refer https://golang.org/doc/go1.4#canonicalimports
 */

package main // import "github.com/minio/minio"


import (
// #cgo CXXFLAGS: --std=c++0x  -DNDEBUG -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-parameter -DGTEST_USE_OWN_TR1_TUPLE=1 -fPIC -Wno-unused-local-typedefs -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64 -DSMR_ENABLED -DLEVELDB_PLATFORM_POSIX -DBUILD_FOR_ARM=0 -Wno-psabi -Wno-enum-compare -Wno-shift-count-overflow 
// #cgo LDFLAGS: libkinetic.a libseapubcmds.a kernel_mem_mgr.a libssl.a libcrypto.a libgmock.a libgtest.a libsmrenv.a libleveldb.a libmemenv.a libkinetic_client.a zac_kin.a libprotobuf.a libgflags.a -lpthread -ldl -lrt libglog.a

	"C"
	"os"
	"log"
	minio "github.com/minio/minio/cmd"
	// Import gateway
	_ "github.com/minio/minio/cmd/gateway"
        "github.com/minio/minio/common"
        //DO NOT DELETE the following lines:
	//"github.com/pkg/profile"
	"net/http"
        _ "net/http/pprof"
	"runtime/debug"
)

func main() {
// DO NOT DELETE THE FOLLOWING LINES. WE CAN USE THEM FOR PROFILING
//    defer profile.Start(profile.Cpurofile, profile.ProfilePath(".")).Stop()
      //defer profile.Start(profile.MemProfile, profile.ProfilePath(".")).Stop()
      //defer profile.Start(profile.TraceProfile, profile.ProfilePath(".")).Stop()
      //http.ListenAndServe("localhost:8080", nil)
    common.EnableTrace()
    defer common.KUntrace(common.KTrace("Enter"))
    log.Println("******MAIN******")
    debug.SetMemoryLimit(1<<30)
    debug.SetGCPercent(25)
    go func() {
        log.Println(http.ListenAndServe("localhost:8082", nil))
    }()
    log.Println("*****111 *MAIN******")
    minio.Main(os.Args)
}
