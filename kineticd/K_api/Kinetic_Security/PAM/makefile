
IKSDIR = ../api
IEADIR = ../../tcg_api/api
IPBKDIR = ../external-libs/PBKDFlib

LIBKSAPI = $(IKSDIR)/libksapi.a
LIBEAPI = $(IEADIR)/libapi.a
LIBPBKDF = $(IPBKDIR)/libpbkdf.a
TRANSPORTDIR = ../../tcg_api/transports

TRANSPORTLIBS=$(TRANSPORTDIR)/libtransports.a $(TRANSPORTDIR)/libseapubcmds.a

# Turn up all the compiler warnings to try to find platform and compiler issues.

CFLAGS= -I$(IKSDIR) -I$(IEADIR) -I$(IPBKDIR) -std=gnu99 -Wextra -fPIC

LIBOBJS= seagate.o


all: $(LIBOBJS)
	$(LD) -x --shared -o seagate.so $(LIBKSAPI) $(TRANSPORTLIBS) $(LIBEAPI) $(LIBPBKDF)

clean:
	rm -f *.o *.so

