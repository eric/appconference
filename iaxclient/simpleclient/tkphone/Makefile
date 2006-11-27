
ROOT=../..
CFLAGS=-I$(ROOT)/lib -I$(ROOT)/lib/libiax/src -I$(ROOT)/lib/gsm/inc -I$(ROOT)/lib/portaudio/pa_common -I$(ROOT)/lib/portaudio/pablio -Iinclude

#debug
CFLAGS:= $(CFLAGS) -g -Wall

OBJS=	iaxcli.o tones.o

LIBS=	$(ROOT)/lib/libiaxclient.a

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
CFLAGS:= $(CFLAGS) -DWIN32 -mno-cygwin
EXEEXT=.exe
SYSLIBS=-mno-cygwin -lwinmm -lwsock32 -lcomdlg32 -luser32 -lgdi32 -lole32 -lcomctl32 -lctl3d32 \
   -lstdc++ -lshell32 -loleaut32 -ladvapi32 -luuid -Wl,--output-def,iaxclient.def,--add-stdcall-alias,--enable-stdcall-fixup,--out-implib,libiaxclient.a 
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

# This allows us to build for multiple architectures and keep
# the executables from being confused.  The TK gui uses these names
# to idetify the proper command line binary to use.  All non-windows
# platforms seem to support the "-s" and "-m" flags to uname
ifneq ($(OSTYPE),WIN32)
EXEEXT=-$(shell uname -s)-$(shell uname -m)
endif

PROGS= iaxcli$(EXEEXT)

all: makelib $(PROGS)

clean: 
	rm -f $(OBJS) $(PROGS)

makelib:
	(cd $(ROOT)/lib; $(MAKE) libiaxclient.a)

$(ROOT)/lib/libiaxclient.a: 
	(cd $(ROOT)/lib; $(MAKE) libiaxclient.a)

iaxcli$(EXEEXT): $(OBJS) $(LIBS)
	gcc $(OBJS) $(LIBS) $(SYSLIBS) -o iaxcli$(EXEEXT)
