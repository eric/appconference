
CFLAGS=-I../../lib -I../../lib/libiax/src -I../../lib/gsm/inc -I../../lib/portaudio/pa_common -I../../lib/portaudio/pablio -Iinclude

#debug
CFLAGS:= $(CFLAGS) -g

OBJS= wx.o testcall.o	


LIBS=	../../lib/libiaxclient.a

EXEEXT=

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
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP `wxgtk-2.4-config --cflags`
SYSLIBS=-lpthread -lm `wxgtk-2.4-config --libs`
endif

ifeq ($(OSTYPE),MACOSX)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP `wx-config --cflags`
SYSLIBS=-framework CoreAudio -framework AudioToolbox `wx-config --libs`
endif

CPPFLAGS = $(CFLAGS)

PROGS= wx$(EXEEXT)

all: makelib $(PROGS)

clean: 
	rm -f $(OBJS) $(PROGS)

makelib:
	(cd ../../lib; make libiaxclient.a)

../../lib/libiaxclient.a: 
	(cd ../../lib; make libiaxclient.a)

wx$(EXEEXT): $(OBJS) $(LIBS)
	c++ $(OBJS) $(LIBS) $(SYSLIBS) -o wx$(EXEEXT)


