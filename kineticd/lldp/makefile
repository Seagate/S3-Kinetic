OPT ?= -g -DNDEBUG
LFLAGS ?= -Wall -std=c++0x -pthread $(OPT)
CFLAGS ?= -Wall -fPIC -std=c++0x -c $(OPT)
CXX ?= g++
ARM ?= y

all : lldp_kin.a lldp_local
.PHONY : all

LIBOBJS = lldp_socket.o lldp_tlv.o lldp_listener.o
LOCALOBJS = $(LIBOBJS) main.o

lldp_local: $(LOCALOBJS)
	$(CXX) $(LFLAGS) $(LOCALOBJS) -o lldp_local

main.o: main.cc includes/lldp_socket.h
	$(CXX) $(CFLAGS) main.cc

lldp_socket.o: lldp_socket.cc includes/lldp_socket.h
	$(CXX) $(CFLAGS) lldp_socket.cc

lldp_tlv.o: lldp_tlv.cc includes/lldp_tlv.h
	$(CXX) $(CFLAGS) lldp_tlv.cc

lldp_listener.o: lldp_listener.cc includes/lldp_listener.h includes/lldp_socket.h includes/lldp_tlv.h
	$(CXX) $(CFLAGS) lldp_listener.cc

lldp_kin.a: $(LIBOBJS)
	ar -rc lldp_kin.a $(LIBOBJS)
	ranlib lldp_kin.a

clean:
	rm -f *.o *.a
	rm -f lldp_local
