#
# CiC
#
# Channel independent Conferencing
#
# Makefile, based on the Asterisk Makefile, Coypright (C) 1999, Mark Spencer
#
# Copyright (C) 2002,2003 Junghanns.NET GmbH
#
# Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
#
# This program is free software and may be modified and 
# distributed under the terms of the GNU Public License.
#

.EXPORT_ALL_VARIABLES:

INSTALL_PREFIX=
ASTERISK_SOURCE_DIR=/root/octobri/asterisk

MODULES_DIR=$(INSTALL_PREFIX)/usr/lib/asterisk/modules

PROC=$(shell uname -m)
#PROC=i486

DEBUG=-g #-pg
INCLUDE=-I$(ASTERISK_SOURCE_DIR)/include -I$(ASTERISK_SOURCE_DIR)
CFLAGS=-pipe  -Wall -Wmissing-prototypes -Wmissing-declarations $(DEBUG) $(INCLUDE) -D_REENTRANT -D_GNU_SOURCE
#CFLAGS+=-O6
CFLAGS+=$(shell if $(CC) -march=$(PROC) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-march=$(PROC)"; fi)
CFLAGS+=$(shell if uname -m | grep -q ppc; then echo "-fsigned-char"; fi)

LIBS=-ldl -lpthread -lm
CC=gcc
INSTALL=install

SHAREDOS=app_conference.so

CFLAGS+=-Wno-missing-prototypes -Wno-missing-declarations

CFLAGS+=-DCRYPTO

# to allow # and *
#CFLAGS+=-DCONF_HANDLE_DTMF
all: $(SHAREDOS) 

clean:
	rm -f *.so *.o

app_conference.so : app_conference.o ringbuffer.o
	$(CC) -shared -Xlinker -x -o $@ app_conference.o ringbuffer.o

app_onering.so : app_onering.o onering.o
	$(CC) -shared -Xlinker -x -o $@ app_onering.o onering.o

onering:
	$(CC) -o onering onering.c

install: all
	for x in $(SHAREDOS); do $(INSTALL) -m 755 $$x $(MODULES_DIR) ; done

config: all
	cp conf.conf /etc/asterisk/
	