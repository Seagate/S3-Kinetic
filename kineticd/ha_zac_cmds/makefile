OPT ?= -g -DNDEBUG
# OPT ?= -g
LFLAGS ?= -Wall -std=c++0x -pthread $(OPT)
CFLAGS ?= -Wall -fPIC -std=c++0x -c $(OPT)
CXX ?= g++
ARM ?= n

ifneq ($(ARM),n)
	CXX=$(MARV)
endif

all : zac_kin.a
.PHONY : all

LIBOBJS = zac_mediator.o ata_cmd_handler.o

ata_cmd_handler.o: includes/ata_cmd_handler.h ata_cmd_handler.cc
	$(CXX) $(CFLAGS) ata_cmd_handler.cc

zac_mediator.o: includes/zac_mediator.h zac_mediator.cc includes/ata_cmd_handler.h includes/zoned_ata_standards.h
	$(CXX) $(CFLAGS) zac_mediator.cc

zac_kin.a: $(LIBOBJS)
	ar -rc zac_kin.a $(LIBOBJS)
	ranlib zac_kin.a

clean:
	rm -f *.o *.a
