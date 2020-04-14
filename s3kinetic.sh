#!/bin/bash
#Command:
# s3kinetic ARM LAMARRKV
# s3kinetic X86 SMR
# s3kinetic X86 NONSMR
git clean -xdf
cd kineticd
git clean -xdf
case "$1" in 
  "X86" )
     source ./x86_envsetup.sh
     case "$2" in
       "NONSMR" )
          cmake -DPRODUCT=X86NONSMR
       ;;
 
       "SMR" )
          cmake -DPRODUCT=X86SMR
       ;;
     esac
     make
     ./cplibx86.sh ../libx86
     cd ../
     make -f Makefile.x86
     ;;

  "ARM" )
    echo "ARM"
    source ~/uboot-linux/envsetup.sh
    cmake -DPRODUCT="$2"
    make
    ./cplib.sh ../libarm
    cd ../
    make -f Makefile.arm
    ;;
esac
