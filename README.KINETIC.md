		HOW TO INSTALL AND RUN MINIO-KINETIC
			Ver 1.7

NOTE: - This procedure will work for ubuntu 16.04, 18.04.
      - Also works for g++ gcc compiler 5 and 7
      - This is manual method for quick try and test.
      - Other method is via installer similar to kineticd installer.

I. INSTALL MINIO:
   A. INSTALL GOLANG
      Go to this web site to install golang:
      https://tecadmin.net/install-go-on-ubuntu

   B. INSTALL MINIO-KINETIC:
      The Minio and kineticd are now under one roof for compiling.
         Clone albany-minio
           - git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/in/albany-minio.git

II. COMPILE AND RUN minio or s3kinetic.X.Y:
    The script s3kinetic.sh will be used to start the compiling.  The command is following:
      ./s3kinetic.sh arg1 arg2
      arg1 = X86 or ARM
      arg2 = NONSMR or SMR or LAMARRKV
     
      The NONSMR or SMR is the disk drive type. NONSMR is implied that the standard SATA disk drive. SMR is for the standard SMR SATA interface drive with zac media management.

    There will be two executable files exactly the same, but they have different names as shown below:
      - minio
      - s3kinetic.arg1.arg2

    EX: s3kinetic.ARM.LAMARRKV
        s3kinetic.X86.NONSMR
        s3kinetic.X86.SMR
    
    The above examples are the combinations that were tested.
    The other combinations like s3kinetic.ARM.NONSMR, s3kinetic.ARM.SMR are not in the CMakeList.txt configuration, therefore they may not compiled.  These combinations may be used by INTERPOSER.
           
    A. COMPILE:
      Under "albany-minio" directory, there is "kineticd" directory.
      Any change in "kineticd" directory can be committed normally using git commands(add, commit...)..
      Any change in "albany-minio" directory  can be committed normally using git commands (add, commit...).

      To compile minio (s3kinetic.x.y), assume that the following directories are under user's home directory:
        - albany-minio
        - uboot-linux (this is uboot for ARM)

      If this is the 1st time, do the followings (assume the current directory is ~/albany-minio):
        - go to "uboot-linux" directory,  do:
          ./build_embedded_image.sh -t ramdef
        - go to 'kineticd' directory under 'albany-minio' directory, do the following commands to install the required packages::
          cd kineticd
          ./x86_package_installation.sh

      To compile, do the followings:
        - Go back to albany-minio directory:
          cd ~/albany-minio
        - To Compile for ARM, LAMARRKV:
          source ~/uboot-linux/envsetup.h
          ./s3kinetic.sh ARM LAMARRKV  (for LAMARRKV using ARM processors).
        - To Compile for X86:
          source kineticd/x86_envsetup.sh
          ./s3kinetic.sh X86 NONSMR    (for non-smr drive using SATA interface (like standard SATA drive), X86 processor).
          ./s3kinetic.sh X86 SMR       (for smr drive using SATA interface, X86 processor).

      An exectuable 'minio' and s3kinetic.x.y  will be created and are ready to run.
      
    B. RUN minio ors3kinetic.X.Y:
       1. UNDER LAMARRKV or INTERPOSER ARM:
          - Copy minio or s3kinetic.X.Y (ARM version) to the Lamarrkv drive under directory /mnt/util:
            scp minio (or s3kinetic.X.Y) root@ip_address:/mnt/util/
          - ssh into Lamarrkv drive
            ssh root@ip_address
          - Go to directory /mnt/util
            cd /mnt/util
          - Kill the running kineticd:
            killkv
          - Make sure certificate.pem and private.pem are in this directory
            cp certificates/*.pem .
          - Make sure there is directory metadata.db that has users.json.

          - To save some memory, disable web browser by:
            export MINIO_BROWSER=off

          - start minio by typing:
             ./minio server kinetic:skinny:sda kineticd --store_partition=/dev/sda --store_device=sda --metadata_db_path=./metatdata.db
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

          - It is ready to accept commands.
          - If those messages are not shown, using interactive python to do instant_secure_erase, then restart 'minio'
            
       2. UNDER X86:
	  - Make sure there is a spare disk drive available. Ex /dev/sdb

	  Type the following line:

          ./minio server kinetic:skinny:sdx kineticd --store_partition=/dev/sdxn --store_device=sdx --metadata_db_path=./metatdata.db

           Notes: sdxn is a partition number. Ex: /dev/sdb1 
                  under X86, a partition can be used for storage instead of the whole drive.            

          To initialize the drive (ISE), the ISE command under directory ~/albany-minio/pkg/kinetic_client/ can be used to ISE the drive (or using ISE.py in shenzi, or under interactive python)
	       ./ISE IPaddress 
          Ex:  ./ISE 127.0.0.1    
               ./ISE 172.16.1.59

       Notes:
       If any openssl error, copy the below files to albany-minio:
         * certifcate.pem, private_key.pem from kineticd
         * copy the contents below and save to ./metadata.db/users.json:
       [{"id":1,"key":"asdfasdf","maxPriority":9,"scopes":[{"offset":0,"permissions":4294967295,"tls_required":false,"value":""}]}]

       If any Store Corrupt Error
         * ISE the drive using s3test/ISE or python instant_secure_erase and restart minio
           ./ISE 127.0.0.1

    C. USING s3cmd commands to test.
      1. S3CMD INSTALLATION
         Refer: https://tecadmin.net/install-s3cmd-manage-amazon-s3-buckets/
         Compile the source code and install instead of sudo apt-get install s3cmd as we need 2.0 version.
      2. S3 Config
         Copy the following lines to ~/.s3cfg:

         host_base = 127.0.0.1:9000    //replace the IP address of LAMARRKV disk drive.
         host_bucket = 127.0.0.1:9000  //replace the IP address of LAMARRKV disk drive.
         use_https = False

         # Setup access keys
         access_key =  minioadmin
         secret_key = minioadmin

         # Enable S3 v4 signature APIs
         signature_v2 = False

      3. Test
         * Make a bucket  
         >>s3cmd mb s3://bucket_name
         * Put a file to the bucket
         >>s3cmd put hello_world.txt s3://bucket_name
         * To list the file inside the bucket
         >>s3cmd ls s3://bucket_name
         * To delete file from bucket
         >>s3cmd del s3://bucket_name/hello_world.txt
         Try others commands, and report bugs in Jira (thanks).
      4. Performance Test
         There is s3-benchmark test.

(to be continued)






