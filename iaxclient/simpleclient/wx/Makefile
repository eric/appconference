
CFLAGS=-I../../lib

#debug
CFLAGS:= $(CFLAGS) -g
OBJS= wx.o 
REZ=echo

CFLAGS:= $(CFLAGS) -DDEFAULT_DESTSERV=\"guest@ast1\" -DDEFAULT_DESTEXT=\"1001\" -DDEFAULT_NUM_CALLS=1

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
PROG=wx.exe
TARGET=wx.exe
CFLAGS:= $(CFLAGS) -I$(WXWIN)/include -I$(WXWIN)/lib/mswd -fno-rtti -fno-exceptions -fno-pcc-struct-return -fstrict-aliasing -Wall -D__WXMSW__ -D__GNUWIN32__ -DWINVER=0x400 -D__WIN95__ -DSTRICT -D__WXDEBUG__
WINSYSFLAGS=-Wl,--subsystem,windows -mwindows
#WINSYSFLAGS=
SYSLIBS=$(WINSYSFLAGS) -lwinmm -lwsock32 -lwxmswd -lcomdlg32 -luser32 -lgdi32 -lole32 -lwsock32 -lcomctl32 -lctl3d32 -lgcc -lstdc++ -lshell32 -loleaut32 -ladvapi32 -luuid -L$(WXWIN)/lib
endif

ifeq ($(OSTYPE),LINUX)
PROG=wx
TARGET=wx
CFLAGS:= $(CFLAGS) `wx-config --cflags` `gtk-config --cflags`
SYSLIBS=-lpthread -lm `wx-config --static --libs`
endif

ifeq ($(OSTYPE),MACOSX)
PROG=wx
TARGET=wx.app
CFLAGS:= $(CFLAGS) -DPOSIXSLEEP `wx-config --cflags`
SYSLIBS=-framework CoreAudio -framework AudioToolbox `wx-config --libs`
CXX=`wx-config --cxx`
REZ=`wx-config --rezflags`
endif

CPPFLAGS = $(CFLAGS)

all: makelib $(TARGET)

clean: 
	rm -f $(OBJS) $(PROG)

makelib:
	(cd ../../lib; make libiaxclient.a)

../../lib/libiaxclient.a: 
	(cd ../../lib; make libiaxclient.a)

$(PROG): $(OBJS) $(LIBS)
	$(CXX) $(OBJS) $(LIBS) $(SYSLIBS) -o $(PROG)

$(PROG).app: $(OBJS) $(LIBS)
	mkdir -p $(PROG).app/Contents/MacOS $(PROG).app/Contents/Resources
	cp `wx-config --prefix`/lib/libwx_mac*.rsrc $(PROG).app/Contents/Resources/$(PROG).rsrc
	echo -n "APPL????" > $(PROG).app/Contents/PkgInfo
	cp Info.plist $(PROG).app/Contents/Info.plist 
	$(CXX) $(OBJS) $(LIBS) $(SYSLIBS) -o $(PROG).app/Contents/MacOS/$(PROG)
