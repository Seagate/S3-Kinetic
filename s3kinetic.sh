#!/bin/bash
#Command:
# s3kinetic ARM LAMARRKV
# s3kinetic X86 SMR
# s3kinetic X86 NONSMR
# $1 X86 or ARM
# $2 kineticd directory (if $1==X86) or LAMARRKV (if $1==ARM)
# $3 uboot-linux directoty (if $1==ARM)
# $4 kinetic directory (if $1==ARM)
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
