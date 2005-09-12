
CFLAGS=-I../../lib -I../../lib/libiax/src -I../../lib/gsm/inc -I../../lib/portaudio/pa_common -I../../lib/portaudio/pablio -Iinclude

#debug
CFLAGS:= $(CFLAGS) -g -Wall

OBJS=	iaxcli.o tones.o

LIBS=	../../lib/libiaxclient.a

EXEEXT=

USE_HOTKEY=0

# glean the platform
# We will define this as either Linux, Win32, or MacOSX
ifneq (,$(findstring Linux,$(shell uname)))
OSTYPE=LINUX
else
ifneq (,$(findstring Darwin,$(shell uname)))
OSTYPE=MACOSX
else
# CYGWIN reports CYGWIN_NT-5.0 under Win2K
ifneq (,$(findstring WIN,$(shell uname)))
OSTYPE=WIN32
else
ifneq (,$(findstring MINGW,$(shell uname)))
OSTYPE=WIN32
else
ifneq (,$(findstring SunOS,$(shell uname)))
OSTYPE=SOLARIS
else
ifneq (,$(findstring FreeBSD,$(shell uname)))
OSTYPEREAL=FREEBSD
else
$(warning OSTYPE cannot be detected, assuming Linux)
endif
OSTYPE=LINUX
endif
endif
endif
endif
endif

ifeq ($(OSTYPEREAL),FREEBSD)
MAKE=gmake
else
MAKE=make
endif

ifeq ($(USE_HOTKEY),1)
CFLAGS:= $(CFLAGS) -DUSE_HOTKEY
endif

ifeq ($(OSTYPE),WIN32)
CFLAGS:= $(CFLAGS) -DWIN32
EXEEXT=.exe
SYSLIBS=-lwinmm -lwsock32 
endif

ifeq ($(OSTYPE),LINUX)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP
ifeq ($(OSTYPEREAL),FREEBSD)
SYSLIBS=-lc_r -lm
else
SYSLIBS=-lpthread -lm
endif
ifeq ($(USE_HOTKEY),1)
CFLAGS:= $(CFLAGS) `gtk-config --cflags`
SYSLIBS:= $(SYSLIBS) `gtk-config --libs` 
endif
endif

ifeq ($(OSTYPE),MACOSX)
CFLAGS:= $(CFLAGS) -DMACOSX -DPOSIXSLEEP
SYSLIBS=-framework CoreAudio -framework AudioToolbox
ifeq ($(USE_HOTKEY),1)
SYSLIBS:= $(SYSLIBS) -framework Carbon
endif
endif

ifeq ($(OSTYPE),SOLARIS)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP
SYSLIBS= -lsocket -lnsl -lpthread -lrt -lresolv -lm 
MAKE=gmake
ifeq ($(USE_HOTKEY),1)
CFLAGS:= $(CFLAGS) `gtk-config --cflags`
SYSLIBS:= $(SYSLIBS) `gtk-config --libs` 
endif
endif

PROGS= iaxcli$(EXEEXT)

all: makelib $(PROGS)

clean: 
	rm -f $(OBJS) $(PROGS)

makelib:
	(cd ../../lib; $(MAKE) libiaxclient.a)

../../lib/libiaxclient.a: 
	(cd ../../lib; $(MAKE) libiaxclient.a)

iaxcli$(EXEEXT): $(OBJS) $(LIBS)
	gcc $(OBJS) $(LIBS) $(SYSLIBS) -o iaxcli$(EXEEXT)
