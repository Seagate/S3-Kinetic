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

.PHONY: clean docs

LDFLAGS=-g

# Turn up all the compiler warnings to try to find platform and compiler issues.
CFLAGS=-Wall -pedantic -Wstrict-prototypes -Wextra -fPIC -std=gnu99 

# TODO: Update dependencies section to support incremental builds.
# As of Jan 2014, if you make any changes to .h files, you need to make clean and then rebuild.
memory_helpers.o: memory_helpers.c memory_helpers.h


# Object files for the Enterprise API Library.
LIBOBJS=parameters.o high_level.o level_0_discovery.o memory_helpers.o \
        low_level.o utilities.o status_codes.o debug.o \
        transport.o transport_locator.o transport_fake.o

libapi.a: $(LIBOBJS)
	ar -rc libapi.a $(LIBOBJS)
	ranlib libapi.a


# Basic self-test of the parameters structure and helpers.
parameters_test: parameters_test.o libapi.a


all: libapi.a

# This will create a PDF document in the latex subdirectory.
# NOTE: The latex build (make.bat) is very noisy.
# The output file is latex/refman.pdf
docs:
	doxygen Doxyfile && cd latex && make

clean:
	rm -f *.o lib*.a
	rm -rf latex
