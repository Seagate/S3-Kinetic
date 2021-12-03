#!/bin/bash
#Command:
# s3kinetic ARM LAMARRKV
# s3kinetic X86 SMR
# s3kinetic X86 NONSMR
# $1 uboot directory
# $2 kineticd directory
# $3 X86 or ARM
# $4 LAMARRKV or NONSMR or SMR
git clean -xdf -e bin
albanydir=$(pwd)
cd $2
git clean -xdf
case "$3" in 
  "X86" )
     source ./x86_envsetup.sh
     [ ! -d "x86_package_install" ] && ./x86_package_installation.sh
     case "$4" in
       "NONSMR" )
          cmake -DPRODUCT=X86NONSMR
       ;;
 
       "SMR" )
          cmake -DPRODUCT=X86SMR
       ;;
     esac
     make
     ./cplibx86.sh $albanydir/lib
     cd $albanydir
     make -f Makefile.x86
     ;;

  "ARM" )
    echo "ARM"
    source $1/envsetup.sh
    cmake -DPRODUCT="$4"
    make
    ./cplibarm.sh $albanydir/lib
    cd $albanydir
    make -f Makefile.arm
    ;;
esac

