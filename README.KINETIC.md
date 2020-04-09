		HOW TO INSTALL AND RUN MINIO-KINETIC
			Ver 1.4

NOTE: - This procedures will work for ubuntu 16.04, 18.04.
      - Also work for g++ gcc compiler 5 and 7

I. INSTALL MINIO:
   A. INSTALL GOLANG
      Go to this web site to install golang:
      https://tecadmin.net/install-go-on-ubuntu

   B. INSTALL MINIO:
      a. For S3 Minio:
         Clone S3 minio:
           - git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/in/albany-minio.git
         Then checkout:
           - git checkout features/for-kinetic-5MBvalue
      b. For Kineticd:
         Clone the kineticd master:
           - git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kin/kineticd.git
         Then checkout:
           - git checkout features/kinetic-minio-skinny-5MBvalue

II. COMPILE AND RUN MINIO-KINETIC:
    Normally, all libraries for kineticd are included in the albany-minio under directories libx86 and libarm.
    There is no need to compile Kineticd unless there is a need to change something in Kineticd.
    If there is any failure when compiling albany-minio, recompiling kineticd is recommended.

    A. COMPILE:
       1. To Compile for ARM:
          If there is a need to change anything in Kineticd, follow steps (a), (b) before go to step(c).
	  Otherwise go to step (c).
          a. source the cross compile environment.
 	  b. Make sure that the directory libarm has the correct version of the kineticd for arm.
	     If new version of the kineticd needed:
		* Generate new kineticd libraries by doing cmake then make under kineticd directory.
		* use cplib.sh to copy all libraries to libarm under minio:
		  (Assume your minio is under your home directory)
			./cplib.sh ~/minio/libarm/
	  c. do :
		./goarmenv.sh
		make -f Makefile.arm

       2. To Compile for X86:
          If there is a need to change anything in Kineticd, follow steps (a), (b), (c) before go to step(d).
          Otherwise go to step (c).
	  a. Clone developer-enviroment (for the 1st time only) and run install_kv_dev.sh:
	     - git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/kt/developer-environment.git
             - cd developer-environment
             - sudo ./install_kv_dev.sh
             
	  b. Run x86_package_installation.sh (for the 1st time only).
	     ./x86_package_installation.sh

          c. Make sure that the directory libx86 has the correct version of the kineticd for x86.
             If new version of the kineticd needed:
		* Go to kineticd direcory.
                * Generate new kineticd libraries by doing:
                  - git clean -xdf
                  - source ./x86_envsetup.sh
                  - cmake -DPRODUCT=X86
                  - make
                * use cplibx86.sh to copy all libraries to libx86 under ~/albany-minio:
                  (Assume your albany-minio is under your home directory)
                        ./cplibx86.sh ~/albany-minio/libx86/

	  d. do:
		./gox86env.sh
		make -f Makefile.x86

    B. RUN MINIO-KINETIC:
       1. UNDER INTERPOSER ARM:
	  DO NOT TRY ARM YET. WE DO NOT HAVE ENOUGH MEMORY. IT RUNS AND FAILS
       2. UNDER X86:
	  - Make sure there is a spare disk drive available. Ex /dev/sdb
          - Partition the drive:
	    Say after partition, there is one partition /dev/sdb1

	  Type the following line:

	  sudo  ./minio server kinetic:skinny:sdb1

       If any openssl error copy the below files to albany-minio:
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
         vi ~/.s3cfg
         set both the access_key and the secret_key to minioadmin
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








