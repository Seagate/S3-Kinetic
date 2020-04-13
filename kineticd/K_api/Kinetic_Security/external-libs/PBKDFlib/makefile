.PHONY: clean docs

LDFLAGS=-g 

CRYPTOFLAGS = -fPIC -DCRYPTO_MATH_PBKDF -DCRYPTO_MATH_AES -DCRYPTO_MATH_AES_GCM -DCRYPTO_MATH_SHA256 -DCRYPTO_MATH_HMAC -DAES_ENCRYPTION 

# Turn on all flags for the crypto functions we use.
CFLAGS= $(CRYPTOFLAGS)

# CFLAGS=-Wall -pedantic -Wstrict-prototypes -Wextra 


# Object files for the Enterprise API Library.
LIBOBJS=sc_aes.o sc_hmac.o sc_pbkdf.o sc_sha2.o aes_gcm.o debug.o sc_aescrypt.o sc_aeskey.o sc_aestab.o 

libpbkdf.a: $(LIBOBJS)
	$(AR) -rc libpbkdf.a $(LIBOBJS)
	ranlib libpbkdf.a

all: libpbkdf.a 

# This will create a PDF document in the latex subdirectory.
# NOTE: The latex build (make.bat) is very noisy.
# The output file is latex/refman.pdf
docs:
	doxygen Doxyfile && cd latex && make

clean:
	rm -f *.o *.exe lib*.a
	rm -rf latex
