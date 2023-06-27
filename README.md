# S3-Kinetic
S3-Kinetic is a fork of MinIO, specifically [this release](https://github.com/minio/minio/tree/RELEASE.2019-10-12T01-39-57Z). Much of this repository is identical to its MinIO counterpart; however, several files have been added and/or modified(link). 

These modifications allow MinIO's S3 server to  interface with [kineticd](https://gitlab.com/kinetic-storage/kineticd), an on-disk key-value storage application developed by Seagate. **This code depends heavily on libraries from kineticd; it will not compile without kineticd**. 

When compiled, this code produces an executable, **s3kinetic**, that runs an S3 server largely similar to that provided by a local MinIO executable. However, **s3kinetic** will store incoming data (e.g., from an S3 PutObject request) in a key-value database derived from **kineticd**, rather than employing a traditional filesystem as a standard MinIO server would. 

This repository was developed as part of an effort to explore additional functionalities for **kineticd**. It is an **experimental** repository and will NOT function as a drop-in replacement for MinIO.

# Prerequisites
* [kineticd](https://gitlab.com/kinetic-storage/kineticd) and [its dependencies](https://gitlab.com/kinetic-storage/kineticd#requirements-for-getting-started)
* Go v1.20 or higher ([download here](https://go.dev/dl/))
* [libkinetic "develop" branch](https://gitlab.com/kinetic-storage/libkinetic/-/tree/develop/) - required to interact with server. See [notes on libkinetic](#installing-libkinetic)
* A spare hard disk drive, 512k or 512e (4kn drives are NOT supported)

 **Optional, but recommended**: [s3cmd](https://s3tools.org/s3cmd), simple command line tool for S3 testing

# Compiling
1. Compile kineticd for [x86](https://gitlab.com/kinetic-storage/kineticd#building-for-x86) or [ARM](https://gitlab.com/kinetic-storage/kineticd#building-for-ARM) according to your system architecture
2. Clone (or move) the S3-Kinetic repository into the the same parent directory as kineticd
3. Compile s3kinetic with `make clean x86`

# Installing the memMgr kernel module
S3-Kinetic depends on memMgr, a kernel memory management module included in kineticd's source code. To install this module:
```
# Compile and insert
cd kineticd/driver/memMgr
make
sudo insmod memMgr_drv.ko

# Create device node
cat /proc/devices | grep mem   # retrieve major device number for memMgr
sudo mknod /dev/memMgr c <MAJOR_NUMBER> 0
```

# Installing libkinetic
At the moment, the only way to perform certain backend operations on S3-Kinetic - i.e. erasing and rebuilding a database for the server - is to use the **kctl** module provided by **libkinetic**. 

* Download the **develop** branch of libkinetic - only this branch will work for S3-Kinetic:
```
 git clone --recurse-submodules -b develop https://gitlab.com/kinetic-storage/libkinetic.git   
```
* Configure protobuf-c:
```
cd libkinetic/vendor/protobuf-c
./autogen.sh
```
* Compile libkinetic and kctl:
```
cd libkinetic
make
```
**Note**: libkinetic depends on protobuf-2.6.1. This library should already be installed by kineticd. If libkinetic is unable to locate this library, refer to the libkinetic [README](https://gitlab.com/kinetic-storage/libkinetic.git) for installation instructions

# Operating the Server
## Starting the server
Start the S3-Kinetic server with
```
sudo ./s3kinetic server[.x86 | .arm] kinetic:skinny:<SHORT_DEVICE_PATH> kineticd --store_device=<DEVICE_PATH>
```
where `<DEVICE_PATH>` represents the full path to your spare drive's device file, e.g. `/dev/sdb`, and `<SHORT_DEVICE_PATH>` represents the specific device file within `/dev`, e.g. `sdb`.

For example, to run the S3-Kinetic server on an x86 platform, using `/dev/sdb` as S3-Kinetic's backing store:

```
sudo ./skinetic.x86 server kinetic:skinny:sdb kineticd --store_device=/dev/sdb
```

## Opening or clearing a database
S3-Kinetic will attempt to reopen a Kinetic database if one exists on the target device. However, if the database is corrupt or nonexistent, S3-Kinetic will not create a new database by default. To erase any existing data and establish a new database, perform the following commands ***while S3-Kinetic or kineticd are running***:

```
cd libkinetic/toolbox/kctl
./kctl -h 127.0.0.1 -s device erase
```
This command will attempt to establish a connection with a kineticd/s3kinetic server on the same machine, erase the server's database, and create a new database.

A large amount of console output will be generated upon issuing the erase command; the server will indicate that the process is complete with the following lines:
```
[I0626 15:16:25.299952 478545 server.cc:821] ========== In Restore Drive State
[I0626 15:16:25.300041 478545 server.cc:821] ========== In Ready State
```

## Testing the server with s3cmd
The S3-Kinetic server will accept properly formatted S3 requests from any source. s3cmd is a lightweight tool that can easily be used to test the server's functionality. After [downloading and installing s3cmd](https://s3tools.org/download), modify or add the following lines to your ~/.s3cfg file:

```
host_base = 127.0.0.1:9000      # default S3-Kinetic server IP:port
host_bucket = 127.0.0.1:9000
use_https = False
access_key =  minioadmin
secret_key = minioadmin  
signature_v2 = False
```
After configuring s3cmd, test out the server with the following commands:

```
# Create a bucket
$> s3cmd mb s3://testbucket

# Store (PUT) an object (<5MB)
$> s3cmd put your_file.txt s3://testbucket

# Store (PUT) a multipart object (>5MB)
#$> s3cmd put --multipart-chunk-size=5MB your_big_file.txt s3://testbucket

# Retrieve (GET) an object
$> s3cmd get s3://testbucket/your_file.txt

# Delete an object
$> s3cmd del s3://testbucket/your_file.txt

# Delete a bucket
$> s3cmd rb [--recursive for non-empty buckets] s3://testbucket
```

See [s3cmd usage](https://s3tools.org/usage) for a complete list of operations supported by s3cmd.


# Features and Limitations
S3-Kinetic supports a limited set of fundamental S3 operations:

* Creating and deleting buckets
* Uploading, downloading, and deleting objects from buckets
* Renaming buckets and objects
* Copying objects between buckets
* Multipart uploads (with 5MB chunk sizes)

However, S3-Kinetic does *not* currently support:
* S3 SELECT queries
* Object/bucket versioning
* Access Control Lists (ACLs)
* MinIO Client (mc) 
* MinIO Browser

S3-Kinetic does not have a maximum object size; however, any objects larger than 5MB must be uploaded as multipart objects in <=5MB chunks.

# License
This repository is licensed under the **Apache 2.0 License**, as it was forked from an APL-licensed release of MinIO (RELEASE.2019-10-12T01-39-57Z). This code is NOT subject to the AGPL-3.0 license used by newer releases of MinIO. A copy of this repository's license can be found in the [LICENSE](https://github.com/Seagate/S3-Kinetic/blob/master/LICENSE) file or at https://www.apache.org/licenses/LICENSE-2.0.     
