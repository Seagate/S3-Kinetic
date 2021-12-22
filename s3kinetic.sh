#!/bin/bash
#Command:
# s3kinetic ARG1 ARG2 ARG3 ARG4
# ARG1 =  X86 or ARM
# ARG2 =  kineticd directory (if ARG1==X86) or LAMARRKV (if ARG1==ARM)
# ARG3 =  uboot-linux directoty (if ARG1==ARM)
# ARG4 =  kinetic directory (if ARG1==ARM)
# Ex:  ./s3kinetic.sh X86 /home/thai/kineticd
#      ./s3kinetic.sh ARM LAMARRKV ~/uboot_linux ~/kineticd
git clean -xdf -e bin
albanydir=$(pwd)
if [ $1 == "X86" ]
then
	cd $2
	git clean -xdf
	source ./x86_envsetup.sh
	numberoffiles=$(ls -l ./x86_package_install | wc -l)
	echo $numberoffiles
	if [ $numberoffiles != "9" ]
	then
		sudo rm -r x86_package_install
		./x86_package_installation.sh
	fi
	cmake -DPRODUCT=X86
	make
	./cplibx86.sh $albanydir/lib
	cd $albanydir
	make -f Makefile.x86
else
	echo "ARM"
	cd $4
	git clean -xdf
	source $3/envsetup.sh
	cmake -DPRODUCT="$2"
	make
	./cplibarm.sh $albanydir/lib
	cd $albanydir
	make -f Makefile.arm
fi
