.PHONY: clean docs

LDFLAGS=-g

DEPS = ks_globals.h ks_setup.h ks_intiialize.h ks_debug.h ks_erase.h ks_random.h ks_mandiags.h ks_setpin.h ks_utilities.h

IKSDIR = ../api
IEADIR = ../../tcg_api/api
ISCDIR = ../../tcg_api/transports/SeaCPublic/include
ISTDIR = ../../tcg_api/transports
IPBKDIR = ../external-libs/PBKDFlib

CRYPTOFLAGS = -DCRYPTO_MATH_PBKDF -DCRYPTO_MATH_AES -DCRYPTO_MATH_AES_GCM -DCRYPTO_MATH_SHA256 -DCRYPTO_MATH_HMAC 

# Turn up all the compiler warnings to try to find platform and compiler issues.
CFLAGS= -I$(IKSDIR) -I$(IEADIR) -I$(ISCDIR) -I$(IPBKDIR) -I$(ISTDIR) -Wall -std=gnu99 -Wstrict-prototypes -fPIC -Wextra $(CRYPTOFLAGS)

# TODO: Update dependencies section to support incremental builds.
# As of Jan 2014, if you make any changes to .h files, you need to make clean and then rebuild.

LIBOBJS= ks_globals.o ks_initialize.o ks_setup.o ks_debug.o ks_utilities.o ks_random.o ks_mandiags.o ks_setpin.o ks_erase.o ks_manlocking.o

libksapi.a: $(LIBOBJS)
	$(AR) -rc libksapi.a $(LIBOBJS)
	ranlib libksapi.a


all: libksapi.a


# This will create a PDF document in the latex subdirectory.
# NOTE: The latex build (make.bat) is VERY noisy.
# The output file is latex/refman.pdf
docs:
	doxygen Doxyfile && cd latex && make


clean:
	rm -f *.o *.exe lib*.a
	rm -rf latex
