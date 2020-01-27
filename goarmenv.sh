go env -w CC=arm-marvell-linux-gnueabi-gcc
go env -w CXX=arm-marvell-linux-gnueabi-g++
go env -w AR=arm-marvell-linux-gnueabi-ar
cp libarm/* .
cp libarm/* cmd/
cp libarm/* pkg/kinetic/

