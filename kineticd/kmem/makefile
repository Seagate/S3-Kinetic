OPT ?= -g -DNDEBUG
# OPT ?= -g
LFLAGS ?= -Wall -std=c++0x $(OPT)
CFLAGS ?= -Wall -fPIC -std=c++0x -c $(OPT)
CXX ?= g++
ARM ?= n

all : kernel_mem_mgr.a 
.PHONY : all

LIBOBJS = kernel_mem_mgr.o 


kernel_mem_mrg.o: kernel_mem_mrg.h
	$(CXX) $(CFLAGS) kernel_mem_mgr.cc

kernel_mem_mgr.a: $(LIBOBJS)
	ar -rc kernel_mem_mgr.a $(LIBOBJS)
	ranlib kernel_mem_mgr.a

clean:
	rm -f *.o *.a
