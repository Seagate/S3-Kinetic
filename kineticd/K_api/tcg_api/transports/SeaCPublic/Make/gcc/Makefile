# Hand Written Makefile (Edit with caution) -Muhammad
# 
# Do NOT modify or remove this copyright and confidentiality notice.
#
# Copyright 2012 - 2014 Seagate Technology LLC.
#
# The code contained herein is CONFIDENTIAL to Seagate Technology LLC 
# and may be covered under one or more Non-Disclosure Agreements. 
# All or portions are also trade secret. 
# Any use, modification, duplication, derivation, distribution or disclosure
# of this code, for any reason, not expressly authorized is prohibited. 
# All other rights are expressly reserved by Seagate Technology LLC.
#

NAME=seapubcmds
MAJOR=0
MINOR=1
VERSION=$(MAJOR).$(MINOR)
SRC_DIR=../../src/
INC_DIR=-I../../include
#CC = cc
CFLAGS = -Wall -c -fPIC -I.
LFLAGS = -Wall 
LIB_SRC_FILES = $(SRC_DIR)cmds.c $(SRC_DIR)sg_helper.c $(SRC_DIR)scsi_helper.c $(SRC_DIR)sat_helper.c $(SRC_DIR)ata_cmds.c $(SRC_DIR)scsi_cmds.c
#Add the tcg_helper.c after adding the support. 

#All of the source files have associated object files
LIB_OBJ_FILES = $(LIB_SRC_FILES:.c=.o)
LIBS = lib$(NAME).a

.PHONY: all 

all: clean $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INC_DIR) $< -o $@

$(LIBS): $(LIB_OBJ_FILES)
	rm -f $@
	$(AR) cq $@ $(LIB_OBJ_FILES)
	$(CC) -shared $(LIB_OBJ_FILES) -o lib$(NAME).so.$(VERSION)
clean:
	rm -f lib$(NAME).a lib$(NAME).so.$(VERSION) *.o ../../src/*.o
