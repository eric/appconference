
CFLAGS=-I../../lib -I../../lib/libiax/src -I../../lib/gsm/inc -I../../lib/portaudio/pa_common -I../../lib/portaudio/pablio -Iinclude

#debug
CFLAGS:= $(CFLAGS) -g
OBJS= wx.o testcall.o	
REZ=echo


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
#include $(WXWIN)/src/makeg95.env
#include $(WXWIN)/src/makeprog.g95
EXEEXT=.exe
CFLAGS:= $(CFLAGS) -I$(WXWIN)/include -I$(WXWIN)/lib/mswd -fno-rtti -fno-exceptions -fno-pcc-struct-return -fstrict-aliasing -Wall -D__WXMSW__ -D__GNUWIN32__ -DWINVER=0x400 -D__WIN95__ -DSTRICT -D__WXDEBUG__
SYSLIBS=-Wl,--subsystem,windows -mwindows -lwinmm -lwsock32 -lwxmswd -lcomdlg32 -luser32 -lgdi32 -lole32 -lwsock32 -lcomctl32 -lctl3d32 -lgcc -lstdc++ -lshell32 -loleaut32 -ladvapi32 -luuid -L$(WXWIN)/lib
endif

ifeq ($(OSTYPE),LINUX)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP `wx-config --cflags` `gtk-config --cflags`
SYSLIBS=-lpthread -lm `wx-config --static --libs`
endif

ifeq ($(OSTYPE),MACOSX)
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP `wx-config --cflags`
SYSLIBS=-framework CoreAudio -framework AudioToolbox `wx-config --libs`
CXX=`wx-config --cxx`
REZ=`wx-config --rezflags`
endif

CPPFLAGS = $(CFLAGS)

PROG= wx$(EXEEXT)

all: makelib $(PROG)

clean: 
	rm -f $(OBJS) $(PROG)

makelib:
	(cd ../../lib; make libiaxclient.a)

../../lib/libiaxclient.a: 
	(cd ../../lib; make libiaxclient.a)

wx$(EXEEXT): $(OBJS) $(LIBS)
	c++ $(OBJS) $(LIBS) $(SYSLIBS) -o wx$(EXEEXT)
#	$(REZ) wx$(EXEEXT)

$(PROG).app: $(PROG)
	mkdir -p $(PROG).app/Contents/MacOS wx.app/Contents/Resources
	mv $(PROG) $(PROG).app/Contents/MacOS
	cp `wx-config --prefix`/lib/libwx_mac*.rsrc $(PROG).app/Contents/Resources/$(PROG).rsrc
	echo -n "APPL????" > $(PROG).app/Contents/PkgInfo
	cp Info.plist $(PROG).app/Contents/Info.plist 

