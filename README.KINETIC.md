1. To Compile for ARM:
        - source the cross compile environment.
	- Make sure that the directory libarm has the correct version of the kineticd for arm.
	  If new version of the kineticd needed:
		* Generate new kineticd libraries by doing cmake then make under kineticd directory.
		* use cplib.sh to copy all libraries to libarm under minio:
		  (Assume your minio is under your home directory)
			./cplib.sh ~/minio/libarm/
	- do :
		make -f Makefile.arm
2. To Compile for X86:
        - Make sure that the directory libx86 has the correct version of the kineticd for arm.
          If new version of the kineticd needed:
		* Go to kineticd direcory.
                * Generate new kineticd libraries by doing cmake then make under kineticd directory.
                * use cplibx86.sh to copy all libraries to libx86 under minio:
                  (Assume your minio is under your home directory)
                        ./cplibx86.sh ~/minio/libx86/

	- do:
		make -f Makefile.x86

