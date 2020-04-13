HA_ZAC = ../ha_zac_cmds
INCLUDES = -I$(HA_ZAC)/includes
LFLAGS ?= -Wall -std=c++0x -pthread $(HA_ZAC)/zac_kin.a
CFLAGS ?= -Wall -fPIC -std=c++0x -c $(INCLUDES) -D_FILE_OFFSET_BITS=64
CXX ?= g++
ARM ?= y

all: qual_kin.a qual_local
.PHONY: all

LIBOBJS = drive_helper.o base_test.o ati_ste_test.o sequential_write_test.o butterfly_seek_test.o rdt_test.o id_od_test.o
LOCALOBJS = $(LIBOBJS) main.o

qual_local: $(LOCALOBJS)
	$(CXX) $(LOCALOBJS) $(LFLAGS) -o qual_local

main.o: main.cc includes/drive_helper.h includes/base_test.h includes/ati_ste_test.h
	$(CXX) $(CFLAGS) main.cc

drive_helper.o: includes/drive_helper.h drive_helper.cc $(HA_ZAC)/includes/zac_mediator.h
	$(CXX) $(CFLAGS) drive_helper.cc

base_test.o: includes/base_test.h base_test.cc includes/drive_helper.h $(HA_ZAC)/includes/zac_mediator.h
	$(CXX) $(CFLAGS) base_test.cc

ati_ste_test.o: includes/ati_ste_test.h ati_ste_test.cc includes/drive_helper.h includes/base_test.h
	$(CXX) $(CFLAGS) ati_ste_test.cc

sequential_write_test.o: includes/sequential_write_test.h sequential_write_test.cc includes/drive_helper.h includes/base_test.h
	$(CXX) $(CFLAGS) sequential_write_test.cc

butterfly_seek_test.o: includes/butterfly_seek_test.h butterfly_seek_test.cc includes/drive_helper.h includes/base_test.h
	$(CXX) $(CFLAGS) butterfly_seek_test.cc

rdt_test.o: includes/rdt_test.h rdt_test.cc includes/drive_helper.h includes/butterfly_seek_test.h
	$(CXX) $(CFLAGS) rdt_test.cc

id_od_test.o: includes/id_od_test.h id_od_test.cc includes/drive_helper.h includes/rdt_test.h
	$(CXX) $(CFLAGS) id_od_test.cc

qual_kin.a: $(LIBOBJS)
	ar -rc qual_kin.a $(LIBOBJS)
	ranlib qual_kin.a

clean:
	rm -f *.o *.a
	rm -f qual_local
