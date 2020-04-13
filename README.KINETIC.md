		HOW TO INSTALL AND RUN MINIO-KINETIC
			Ver 1.5

NOTE: - This procedures will work for ubuntu 16.04, 18.04.
      - Also work for g++ gcc compiler 5 and 7

I. INSTALL MINIO:
   A. INSTALL GOLANG
      Go to this web site to install golang:
      https://tecadmin.net/install-go-on-ubuntu

   B. INSTALL MINIO-KINETIC:
      The Minio and kineticd are now under one roof for compiling. From now on, it will be called s3kinetic:
         Clone s3kinetic
           - git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/in/albany-minio.git
         (Will fix albany-minio to albany-s3kinetic)

II. COMPILE AND RUN S3kinetic:
    A. COMPILE
      Under albany-minio, there is kineticd directory.
      Any change in kineticd can be done normally like before, like 'git add -u', 'git commit..', except that no 'git push origin ...' is needed
      Any change in albany-minio will be commited normally using git commands and kineticd also be commited (if there is any change in kineticd).

      To compile s3kinetic, assume that the following directories are under user's home directory:
        - albany-minio
        - uboot-linux (this is uboot for ARM)

      do the following:
        cd ~/albany-minio
        ./s3kinetic ARM LAMARRKV  (for LAMARRKV using ARM processors).
                    X86 NONSMR    (for non-smr drive using SATA interface (like standard SATA drive) using X86 processors).
                    X86 SMR       (for smr drive using SATA interface).

      An exectuable 'minio' will be created. (Should have different names later, like : s3kinetic-x86-srm or s3kinetic-x86-nonsmr 
                                              or s3kinetic-arm-lammarrkv).

    B. RUN s3kinetic:
       1. UNDER INTERPOSER ARM:
	  DO NOT TRY ARM YET. WE DO NOT HAVE ENOUGH MEMORY. IT RUNS AND FAILS
       2. UNDER X86:
	  - Make sure there is a spare disk drive available. Ex /dev/sdb

	  Type the following line:

	  sudo ./minio server kinetic:skinny:sdb

          To initialize the drive (ISE), the ISE command under directory ~/albany-minio/pkg/kinetic_client/ can be used to ISE the drive:
	       ./ISE IPaddress 
          Ex:  ./ISE 127.0.0.1    
               ./ISE 172.16.1.59

       Notes:
       If any openssl error, copy the below files to albany-minio:
         * certifcate.pem, private_key.pem from kineticd
         * contents inside /metadata.db/
       If any Store Corrupt Error
         * ISE the drive using s3test/ISE and restart minio
           ./ISE 127.0.0.1

    C. USING s3cmd commands to test.
      1. S3CMD INSTALLATION
         Refer: https://tecadmin.net/install-s3cmd-manage-amazon-s3-buckets/
         Compile the source code and install instead of sudo apt-get install s3cmd as we need 2.0 version.
      2. S3 Config
         Copy the following lines to ~/.s3cfg:

         host_base = 127.0.0.1:9000
         host_bucket = 127.0.0.1:9000
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

      4. Performance Test

(to be continued)






