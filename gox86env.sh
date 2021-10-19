go env -w CC=gcc
go env -w CXX=g++
go env -w AR=ar
[ ! -d "./lib" ] && mkdir ./lib; \
cp ./libx86/*.a ./lib/
