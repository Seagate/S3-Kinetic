		HOW TO INSTALL AND RUN MINIO-KINETIC
			Ver 1.5

NOTE: - This procedures will work for ubuntu 16.04, 18.04.
      - Also work for g++ gcc compiler 5 and 7

I. INSTALL MINIO:
   A. INSTALL GOLANG
      Go to this web site to install golang:
      https://tecadmin.net/install-go-on-ubuntu

   B. INSTALL MINIO-KINETIC:
      The Minio and kineticd are now under one roof for compiling.
         Clone albany-minio
           - git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/in/albany-minio.git

II. COMPILE AND RUN  S3kinetic:
    A. COMPILE
      Under albany-minio, there is kineticd directory.
      Any change in kineticd can be edited normally..
      Any change in albany-minio and/or kineticd will be commited normally using git commands (add, commit...).

      To compile s3kinetic, assume that the following directories are under user's home directory:
        - albany-minio
        - uboot-linux (this is uboot for ARM)

      If this is the 1st time do this (assume the current directory is ~/albany-minio)
        - go to uboot-linux do: './build_embedded_image.sh -t ramdef'
        - go to 'kineticd' directory under 'albany-minio' directoty:  cd kineticd
        - install x86 pacakages                                    :  ./x86_package_installation.sh

      To compile, do the following:
        - Back to albany-minio directory:
          cd ~/albany-minio
        - Compils:
          ./s3kinetic ARM LAMARRKV  (for LAMARRKV using ARM processors).
                      X86 NONSMR    (for non-smr drive using SATA interface (like standard SATA drive), X86 processor).
                      X86 SMR       (for smr drive using SATA interface, X86 processor).

      An exectuable 'minio' will be created and is ready to run. (Should have different names later, like : s3kinetic-x86-srm or s3kinetic-x86-nonsmr 
                                              or s3kinetic-arm-lammarrkv).
      
    B. RUN s3kinetic:
       1. UNDER INTERPOSER ARM:
          - Copy minio (ARM version) to the Lamarrkv drive under directory /mnt/util:
            scp minio root@ip_address:/mnt/util/
          - ssh into Lamarrkv drive
            ssh root@ip_address
          - Go to directory /mnt/util
            cd /mnt/util
          - Make sure certificate.pem and private.pem are in this directory
            cp certificates/*.pem .
          - Make sure there is directory metadata.db that has users.json.

          - To save some memory, disable web browser by:
            export MINIO_BROWSER=off

          - start minio by typing:
             ./minio server kinetic:skinny:sda
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

	  sudo ./minio server kinetic:skinny:sdb

          To initialize the drive (ISE), the ISE command under directory ~/albany-minio/pkg/kinetic_client/ can be used to ISE the drive:
	       ./ISE IPaddress 
          Ex:  ./ISE 127.0.0.1    
               ./ISE 172.16.1.59

       Notes:
       If any openssl error, copy the below files to albany-minio:
         * certifcate.pem, private_key.pem from kineticd
         * contents inside ./metadata.db/users.json
       If any Store Corrupt Error
         * ISE the drive using s3test/ISE or python instant_secure_erase  and restart minio
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






