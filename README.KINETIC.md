# MinIO-Kinetic

Kinetic is a key-value database able to run in the drive. [MinIO](https://github.com/minio/minio) is a regular key-value database with an API compatible with [Amazon S3](https://aws.amazon.com/s3/) cloud storage service. MinIO-Kinetic is the project to have a key-value database running in the drive with an API compatible with S3.

This program uses the frontend of MinIO to send/receive S3 objects and, as the backend, the program uses kinetic to interact with the drive.

---

## Requirements                                                                                      
    Golang                                                                                           
    Kernel Memory Module -- See kineticd README.md, section Loading MemMgr Module, for instruction.  

## Installation

**Notes for version 1.7**: 
- This procedure will work for ubuntu 16.04, 18.04.
- Also works for g++ gcc compiler 5 and 7
- This is a manual method for quick development.
- Normal instructions are via installer similar to kineticd installer.

1. Install Golang: 
   Go to this web site to install golang:
      https://tecadmin.net/install-go-on-ubuntu

2. Checkout MinIO-Kinetic: 

      git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/in/albany-minio.git

---

## Compilation:

The steps to compile MinIO (s3kinetic.x.y) depends on the target architecture. 
assuming that the following directories are under user's home directory:
   - albany-minio
   - uboot-linux (this is uboot for ARM)
   - kineticd

### X86
If the directory "./lib" has X86 libraries (after compiling using method b below),
it is enough to type the following command in the main folder (`albany-minio`) if there is no change
in kineticd:

    make -f Makefile.x86
	
Otherwise use the following command (assuming that the current directory is albany-minio):
         
    ./s3kinetic.sh ARG1 ARG2
	   
    ARG1 = X86
    ARG2 = kineticd directory

    ex: My kineticd is under /home/myhomedir/kineticd

    ./s3kinetic.sh X86 /home/myhomedir/kineticd
    
    or

    ./s3kinetic.sh X86 ~/kineticd

There will be an executable 's3kinetic.X86' generated and stored in directory './bin'.
  
### ARM
If this is the first time, do the following:
    - Go to `uboot-linux` directory and call this command:

       ./build_embedded_image.sh -t ramdef

    - Go to the `albany-minio` folder and do the following:
       ./s3kinetic.sh ARG1 ARG2 ARG3 ARG4

       ARG1 = ARM
       ARG2 = LAMARRKV
       ARG3 = uboot-linux directory
       ARG4 = kineticd directory

       ex: My uboot-linux, kineticd are under /home/myhomedir/uboot-linux and /home/myhomedir/kineticd

       ./s3kinetic.sh ARM LAMARRKV  ~/uboot-linux ~/kineticd

There will be an executable 's3kinetic.arm' generated and stored in directory'./bin'.

        Notes:
          After the first time, if there is no change in uboot or kineticd, quick way to compile is just typing:
                make -f Makefile.arm
        
---

## Running s3kinetic.arm:

### Under LAMARRKV or Interposer ARM:

- Copy s3kinetic.arm from the `bin` folder to the Lamarrkvdrive under directory `/mnt/util`:

         scp ./bin/s3kinetic.arm root@ip_address:/mnt/util/

- ssh into Lamarrkv drive. If you encounter any issues, please, read the section `Conecting to a Serial Port` from [Kinetic Notes]( https://seagatetechnology.sharepoint.com/%3Ab%3A/r/teams/dataflowsoftware/Shared%20Documents/Projects/Kinetic/KineticInteractions_Includes_stepsforfw_update_locationof_slod.pdf?csf=1&web=1&e=eNloKP).

         ssh root@ip_address

- Go to directory `/mnt/util` (for kineticd version 09.xx.yy) or /kinetic/util (for kineticd version 10.xx.yy):

         cd /mnt/util      or cd /kinetic/util

- Kill the running kineticd:

         killkv

- Make sure certificate.pem and private.pem are in this directory:

         cp certificates/*.pem .

- Make sure there is directory metadata.db that has users.json.
- To save some memory, disable web browser by:

         export MINIO_BROWSER=off

- Start minio by typing:

          ./s3kinetic.arm server kinetic:skinny:sda kineticd --store_device=/dev/sda

   to turn on  `TRACE`:

         ./s3kinetic.arm --trace server kinetic:skinny:sdx kineticd --store_device=/dev/sda

- Wait till these messages appear:

          ********3. OBJECT LAYER
          Endpoint:  http://172.16.1.59:9000  http://127.0.0.1:9000      
          AccessKey: minioadmin 
          SecretKey: minioadmin 

          Command-line Access: https://docs.min.io/docs/minio-client-quickstart-guide
          $ mc config host add myminio http://172.16.1.59:9000 minioadmin minioadmin

          Object API (Amazon S3 compatible):
          Go:         https://docs.min.io/docs/golang-client-quickstart-guide
          Java:       https://docs.min.io/docs/java-client-quickstart-guide
          Python:     https://docs.min.io/docs/python-client-quickstart-guide
          JavaScript: https://docs.min.io/docs/javascript-client-quickstart-guide
          .NET:       https://docs.min.io/docs/dotnet-client-quickstart-guide
          Detected default credentials 'minioadmin:minioadmin', please change the credentials immediately using 'MINIO_ACCESS_KEY' and 'MINIO_SECRET_KEY'

Before trying any command, make sure the date/time of the client is the same as the server.
It is ready to accept commands.
If those messages are not shown, using interactive python to do instant_secure_erase, then restart 'minio'.
            
### Under X86:
Make sure there is a spare disk drive available. Ex /dev/sdb

Type the following line:

          sudo ./s3kinetic.X86 server kinetic:skinny:sdx kineticd --store_device=/dev/sdxn

Notes: sdxn is a partition number. Ex: /dev/sdb1 
       Under X86, a partition can be used for storage instead of the whole drive.            

To initialize the drive (ISE), the ISE command under directory ~/albany-minio/pkg/kinetic_client/ can be used to ISE the drive (or using ISE.py in shenzi, or under interactive python)

         ./ISE IPaddress 

 Ex: 
* ./ISE 127.0.0.1    
* ./ISE 172.16.1.59

Notes:
If any openssl error, copy the below files to albany-minio:
* certifcate.pem, private_key.pem from kineticd
* copy the contents below and save to ./metadata.db/users.json:

         [{"id":1,"key":"asdfasdf","maxPriority":9,"scopes":[{"offset":0,"permissions":4294967295,"tls_required":false,"value":""}]}]

If any Store Corrupt Error:
* ISE the drive using s3test/ISE or python instant_secure_erase and restart MinIO:

         ./ISE 127.0.0.1

---

## Testing

Using s3cmd commands to test.
1. S3CMD Installation
   Refer: https://tecadmin.net/install-s3cmd-manage-amazon-s3-buckets/
   Compile the source code and install instead of `sudo apt-get install s3cmd` as we need 2.0 version.
2. S3 Config
   Copy the following lines to `~/.s3cfg`:

         host_base = 127.0.0.1:9000    //replace the IP address of LAMARRKV disk drive.
         host_bucket = 127.0.0.1:9000  //replace the IP address of LAMARRKV disk drive.
         use_https = False
         # Setup access keys
         access_key =  minioadmin
         secret_key = minioadmin
         # Enable S3 v4 signature APIs
         signature_v2 = False

3. Test
* Make a bucket:

         >>s3cmd mb s3://bucket_name

* Put a file to the bucket:

        >>s3cmd put hello_world.txt s3://bucket_name

* To list the file inside the bucket:

         >>s3cmd ls s3://bucket_name

* To delete file from bucket:

         >>s3cmd del s3://bucket_name/hello_world.txt

Try others commands, and report bugs in Jira (thanks).

4. Performance Test
There is s3-benchmark test.

---

## Running MinIO with file system (ext4 or xfs)

This command will allow users to run minio on a storage filesystem such as ext4 or xfs or other file systems:

         ./minio server ./datadir

where `datadir` is a data directory. This directory can be aregular directory or a directory that is mounted to a storagepartition.


(to be continued)






