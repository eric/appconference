
CFLAGS=-I../../lib -I../../lib/libiax/src -I../../lib/gsm/inc -I../../lib/portaudio/pa_common -I../../lib/portaudio/pablio -Iinclude

#debug
CFLAGS:= $(CFLAGS) -g

OBJS=	testcall.o

LIBS=	../../lib/libiaxclient.a

EXEEXT=

# glean the platform
# We will define this as either Linux, Win32, or MacOSX
ifneq (,$(findstring Linux,$(shell uname)))
OSTYPE=Linux
else
ifneq (,$(findstring Darwin,$(shell uname)))
OSTYPE=MACOSX
else
# CYGWIN reports CYGWIN_NT-5.0 under Win2K
ifneq (,$(findstring WIN,$(shell uname)))
OSTYPE=WIN32
else
$(warning OSTYPE cannot be detected, assuming Linux)
OSTYPE=LINUX
endif
endif
endif

ifeq ($(OSTYPE),WIN32)
EXEEXT=.exe
SYSLIBS=-lwinmm -lwsock32 
endif

ifeq ($(OSTYPE),LINUX)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP
SYSLIBS=-lpthread
endif

ifeq ($(OSTYPE),MACOSX)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP
SYSLIBS=-framework CoreAudio -framework AudioToolbox
endif

PROGS= testcall$(EXEEXT)

all: makelib $(PROGS)

clean: 
	rm -f $(OBJS) $(PROGS)

makelib:
	(cd ../../lib; make libiaxclient.a)

../../lib/libiaxclient.a: 
	(cd ../../lib; make libiaxclient.a)

testcall$(EXEEXT): $(OBJS) $(LIBS)
	gcc $(OBJS) $(LIBS) $(SYSLIBS) -o testcall$(EXEEXT)


