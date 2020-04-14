#!/bin/bash
set -e  #stop on error
set -x  #print the cmds getting executed
function install_package {
	cmds=("$@")
	cmds_len=${#cmds[@]}
	for (( i=0; i<${cmds_len}; i++ ));
	do
		cmd=${cmds[i]}
		${cmd}
	done
}

FOLDER_NAME="x86_package_install"
mkdir -p ${FOLDER_NAME}
SRC="${PWD}/${FOLDER_NAME}"

glog_cmds=("cd ${SRC}" "wget https://github.com/google/glog/archive/v0.3.5.tar.gz" "tar zxvf v0.3.5.tar.gz" "cd glog-0.3.5" "./configure" "sudo make" "sudo make install")
gflags_cmds=("cd ${SRC}" "wget https://github.com/gflags/gflags/archive/v2.2.0.tar.gz" "tar zxvf v2.2.0.tar.gz" "cd gflags-2.2.0" "mkdir -p build" "cd build" "export CXXFLAGS="-fPIC"" "cmake .." "make VERBOSE=1" "sudo make" "sudo make install")
gtest_gmock_cmds=("cd ${SRC}" "wget https://github.com/google/googletest/archive/release-1.8.0.tar.gz" "tar xzf release-1.8.0.tar.gz" "cd googletest-release-1.8.0" "mkdir -p build" "cd build" "cmake .." "make -j" "sudo cp -r ../googletest/include/gtest /usr/local/include" "sudo cp -r ../googlemock/include/gmock /usr/local/include" "sudo cp -r ./googlemock/gtest/libgtest*.a /usr/local/lib/" "sudo cp -r ./googlemock/libgmock*.a /usr/local/lib/")
protobuf_cmds=("cd ${SRC}" "wget https://github.com/google/protobuf/releases/download/v2.6.1/protobuf-2.6.1.tar.gz" "tar xzf protobuf-2.6.1.tar.gz" "cd protobuf-2.6.1" "sudo ./configure" "sudo make" "sudo make check" "sudo make install " "sudo ldconfig")
ssl_cmd=("sudo apt-get install libssl-dev")

install_package "${glog_cmds[@]}"
install_package "${gflags_cmds[@]}"
install_package "${gtest_gmock_cmds[@]}"
install_package "${protobuf_cmds[@]}"
install_package "${ssl_cmd[@]}"