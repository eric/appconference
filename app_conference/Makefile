# $Id$

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

#
# app_conference defines which can be passed on the command-line
#

INSTALL_PREFIX := /opt/horizon
INSTALL_MODULES_DIR := $(INSTALL_PREFIX)/lib/asterisk/modules

ASTERISK_INCLUDE_DIR := $(HOME)/local/asterisk/asterisk/include

# turn app_conference debugging on or off ( 0 == OFF, 1 == ON )
APP_CONFERENCE_DEBUG := 1

# 0 = OFF 1 = astdsp 2 = speex
SILDET := 2

#
# app_conference objects to build
#

OBJS = app_conference.o member.o conference.o frame.o 
SHAREDOS = app_conference.so

#
# standard compile settings
#

PROC = $(shell uname -m)
INSTALL = install
CC = gcc

INCLUDE = -I$(ASTERISK_INCLUDE_DIR) 
LIBS = -ldl -lpthread -lm
DEBUG := -g 

CFLAGS = -pipe -std=c99 -Wall -Wmissing-prototypes -Wmissing-declarations $(DEBUG) $(INCLUDE) -D_REENTRANT -D_GNU_SOURCE
#CFLAGS += -O2
#CFLAGS += -O3 -march=pentium3 -msse -mfpmath=sse,387 -ffast-math 
# PERF: below is 10% faster than -O2 or -O3 alone.
#CFLAGS += -O3 -ffast-math -funroll-loops
# below is another 5% faster or so.
CFLAGS += -O3 -ffast-math -funroll-all-loops -march=pentium3 -fprefetch-loop-arrays 
# adding -msse -mfpmath=sse has little effect.
#CFLAGS += -O3 -msse -mfpmath=sse
#CFLAGS += $(shell if $(CC) -march=$(PROC) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-march=$(PROC)"; fi)
CFLAGS += $(shell if uname -m | grep -q ppc; then echo "-fsigned-char"; fi)
CFLAGS += -DCRYPTO

ifeq ($(APP_CONFERENCE_DEBUG), 1)
CFLAGS += -DAPP_CONFERENCE_DEBUG
endif

#
# additional flag values for silence detection
#

ifeq ($(SILDET), 2)
OBJS += libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o
CFLAGS += -Ilibspeex -DSILDET=2
endif

ifeq ($(SILDET), 1)
CFLAGS += -DSILDET=1
endif

#
# targets
#

all: $(SHAREDOS) 

clean:
	rm -f *.so *.o $(OBJS)

app_conference.so : $(OBJS)
	$(CC) -pg -shared -Xlinker -x -o $@ $(OBJS)

vad_test: vad_test.o libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o
	$(CC) $(PROFILE) -o $@ $^ -lm

loop_test: loop_test.o libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o
	$(CC) $(PROFILE) -o $@ $^ -lm

install: all
	for x in $(SHAREDOS); do $(INSTALL) -m 755 $$x $(INSTALL_MODULES_DIR) ; done
	killall asterisk

# config: all
# 	cp conf.conf /etc/asterisk/
	

