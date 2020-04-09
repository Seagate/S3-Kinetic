#!/bin/bash
git clean -xdf
cd kineticd
git clean -xdf
if [ "$1" = "X86" ]
then
  source ./x86_envsetup.sh
  cmake -DPRODUCT=X86
  make
  ./cplibx86.sh ../libx86
  cd ../
  make -f Makefile.x86
else
  echo "ARM"
  source ~/uboot-linux/envsetup.sh
  cmake -DPRODUCT="$1"
  make
  ./cplib.sh ../libarm
  cd ../
  make -f Makefile.arm
fi

