go env -w CC=gcc
go env -w CXX=g++
go env -w AR=ar
rm lib*.a
rm cmd/lib*.a
rm pkg/kinetic/lib*.a
cp libx86/* .
cp libx86/* cmd/
cp libx86/* pkg/kinetic/

