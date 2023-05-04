
if [ -n "$ZSH_VERSION" ]; then
    # this is zsh
    DIR=${${0:a}:h}
else
    # it's probably bash
    DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
fi

export TARGET_PLATFORM=envoy1
export CROSS_COMPILE=aarch64-linux-gnu-
export CROSS_COMPILE_BH=$CROSS_COMPILE
export CROSS_COMPILE_LE=$CROSS_COMPILE
export C_COMPILER=aarch64-linux-gnu-gcc
export CXX_COMPILER=aarch64-linux-gnu-g++
export C_RANLIB=aarch64-linux-gnu-ranlib
export GCC=aarch64-linux-gnu-gcc
export CC=aarch64-linux-gnu-gcc
export LD=aarch64-linux-gnu-ld
export AR=aarch64-linux-gnu-ar


export ARCH=arm64

export C_ARCH_LIB_PATH=$DIR/buildroot-2020.02.3/output/per-package/kinetic/target/usr/
export C_ARCH_BIN_PATH=$DIR/buildroot-2020.02.3/output/per-package/kinetic/host/usr/


export PATH=$PATH:$PWD/toolchain/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin
export BL33=$PWD/u-boot-marvell/u-boot.bin
