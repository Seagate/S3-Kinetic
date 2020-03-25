		HOW TO INSTALL AND RUN MINIO-KINETIC

I. INSTALL MINIO:
   A. INSTALL GOLANG
      Go to this web site to install golang:
      https://tecadmin.net/install-go-on-ubuntu

   B. INSTALL MINIO
      git clone ssh://git@lco-esd-cm01.colo.seagate.com:7999/in/albany-minio.git

II. COMPILE AND RUN MINIO-KINETIC:
   Before compiling, checkout this branch by typing:

      git checkout features/for-kinetic-5MBvalue

   A. COMPILE:
      1. To Compile for ARM:
         - source the cross compile environment.
 	 - Make sure that the directory libarm has the correct version of the kineticd for arm.
	   If new version of the kineticd needed:
		* Generate new kineticd libraries by doing cmake then make under kineticd directory.
		* use cplib.sh to copy all libraries to libarm under minio:
		  (Assume your minio is under your home directory)
			./cplib.sh ~/minio/libarm/
	 - do :
		./goarmenv.sh
		make -f Makefile.arm

      2. To Compile for X86:
         - Make sure that the directory libx86 has the correct version of the kineticd for x86.
           If new version of the kineticd needed:
		* Go to kineticd direcory.
                * Generate new kineticd libraries by doing cmake then make under kineticd directory.
                * use cplibx86.sh to copy all libraries to libx86 under minio:
                  (Assume your minio is under your home directory)
                        ./cplibx86.sh ~/minio/libx86/

	 - do:
		./gox86env.sh
		make -f Makefile.x86

    B. RUN MINIO-KINETIC:
       1. UNDER INTERPOSER ARM:

       2. UNDER X86:
	  - Make sure there is a spare disk drive available. Ex /dev/sdb
          - Partition the drive:
	    Say after partition, there is one partition /dev/sdb1

	  Type the following line:

	  ./minio server kinetic:skinny:sdb1

    C. USING s3cmd commands to test.






