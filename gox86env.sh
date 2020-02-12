go env -w CC=gcc
go env -w CXX=g++
go env -w AR=ar
rm *.a
rm cmd/*.a
cp libx86/* .
cp libx86/* cmd/

