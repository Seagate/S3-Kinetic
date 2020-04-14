.PHONY: all clean pkg

DELIVERABLE=tcg_api_`date +%Y_%m_%d`.tgz
REFMAN=refman.pdf

all:
	cd api && make all
	cd transports && make all
	cd tools && make all

kinetic:
	cd api && make all
	cd transports && make all

clean:
	rm -f *.tgz $(REFMAN)
	cd api && make clean
	cd transports && make clean
	cd tools && make clean

kclean:
	cd api && make clean
	cd transports && make clean


pkg:
	make clean
	cd api && make docs && cp latex/$(REFMAN) .. && make clean
	tar czf $(DELIVERABLE) makefile refman.pdf readme.txt api transports tools
