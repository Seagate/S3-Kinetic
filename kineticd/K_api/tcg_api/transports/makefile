#
# Do NOT modify or remove this copyright and confidentiality notice.
#
# Copyright 2013 - 2014 Seagate Technology LLC.
#
# The code contained herein is CONFIDENTIAL to Seagate Technology LLC
# and may be covered under one or more Non-Disclosure Agreements. All or
# portions are also trade secret. Any use, modification, duplication,
# derivation, distribution or disclosure of this code, for any reason,
# not expressly authorized is prohibited. All other rights are expressly
# reserved by Seagate Technology LLC.
#
.PHONY: clean docs all-nested clean-nested

LDFLAGS=-g

# Turn up all the compiler warnings to try to find platform and compiler issues.
# We cannot use -pendantic because the SeaCPublic code uses C++ style comments
# and -pendantic doesn't like that.
CFLAGS=-Wall -Wstrict-prototypes -fPIC -Wextra -I../api -ISeaCPublic/include -std=gnu99 -DSECURE_MESSAGING


# TODO: Update dependencies section to support incremental builds.

LIBOBJS=transport_SeaCPublic.o transport_locator.o

libtransports.a: $(LIBOBJS)
	$(AR) -rc libtransports.a $(LIBOBJS)
	ranlib libtransports.a

all: all-nested libtransports.a

all-nested:
	cd SeaCPublic/Make/gcc && make all
	cp -p SeaCPublic/Make/gcc/libseapubcmds.a .


clean: clean-local clean-nested

clean-nested:
	cd SeaCPublic/Make/gcc && make clean

clean-local:
	rm -f *.o lib*.a
