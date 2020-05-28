#!/bin/bash
#Command:
# s3kinetic ARM LAMARRKV
# s3kinetic X86 SMR
# s3kinetic X86 NONSMR
git clean -xdf -e bin
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
     ./cplibx86.sh ../lib
     cd ../
     make -f Makefile.x86
     ;;

  "ARM" )
    echo "ARM"
    source ~/uboot-linux/envsetup.sh
    cmake -DPRODUCT="$2"
    make
    ./cplib.sh ../lib
    cd ../
    make -f Makefile.arm
    ;;
esac

if [ -f "minio" ]; then
    [ ! -d "./bin" ] && mkdir ./bin
    cp minio ./bin/s3kinetic.$1.$2
    mv minio ./bin/
    echo "=== New executable minio was created in ./bin directory ===" 
else
    echo "=== New executable minio was not created ==="
fi
