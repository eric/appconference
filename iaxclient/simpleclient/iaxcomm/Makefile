PROVIDER=asterisk
VERSION=1.0rc1
CFLAGS=-I../../lib 

#debug
CFLAGS:= $(CFLAGS) -g -Wall -DVERSION='"$(VERSION)"'

OBJS = main.o prefs.o calls.o directory.o dial.o frame.o devices.o xrc_res.o accounts.o ringer.o
XRCS = rc/devices.xrc rc/directory.xrc rc/frame.xrc rc/menubar.xrc rc/panel.xrc rc/prefs.xrc
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
PROG=iaxcomm.exe
TARGET=iaxcomm.exe
CFLAGS:= $(CFLAGS) -I$(WXWIN)/include -I$(WXWIN)/contrib/include -I$(WXWIN)/lib/mswd -fno-rtti -fno-exceptions -fno-pcc-struct-return -fstrict-aliasing -Wall -D__WXMSW__ -D__GNUWIN32__ -DWINVER=0x400 -D__WIN95__ -DSTRICT -D__WXDEBUG__
WINSYSFLAGS=-Wl,--subsystem,windows -mwindows
#WINSYSFLAGS=
EXTRALIBS = -lwxxrc
SYSLIBS=$(EXTRALIBS) $(WINSYSFLAGS) -lwinmm -lwsock32 -lwxmswd -lcomdlg32 -luser32 -lgdi32 -lole32 -lwsock32 -lcomctl32 -lctl3d32 -lgcc -lstdc++ -lshell32 -loleaut32 -ladvapi32 -luuid -L$(WXWIN)/lib
OBJS:=$(OBJS) iaxcomm_resources.o
endif

ifeq ($(OSTYPE),LINUX)
PROG=iaxcomm
TARGET=iaxcomm
CFLAGS:= $(CFLAGS) `wx-config --cflags` `gtk-config --cflags`
EXTRALIBS = /usr/local/lib/libwx_gtk_xrc-2.4.a
SYSLIBS=$(EXTRALIBS) -lpthread -lm `wx-config --static --libs`
endif

ifeq ($(OSTYPE),MACOSX)
PROG=iaxcomm
TARGET=iaxcomm.app
CFLAGS:= $(CFLAGS) `wx-config --cflags`
EXTRALIBS = `wx-config --prefix`/lib/libwx_mac_xrc-2.4.a
SYSLIBS=$(EXTRALIBS) -framework CoreAudio -framework AudioToolbox `wx-config --libs`
CXX=`wx-config --cxx`
REZ=`wx-config --rezflags`
endif

CPPFLAGS = $(CFLAGS)

all: makelib $(TARGET)

clean: 
	rm -f $(OBJS) $(PROG)
	rm -f xrc_res.cc

makelib:
	(cd ../../lib; make libiaxclient.a)

../../lib/libiaxclient.a: 
	(cd ../../lib; make libiaxclient.a)

iaxcomm_resources.o: iaxcomm.rc
	windres --use-temp-file -i iaxcomm.rc -o iaxcomm_resources.o --include-dir $(WXWIN)/include --define __WIN32__ --define __WIN95__ --define __GNUWIN32__

xrc_res.cc: $(XRCS)
	wxrc -c rc/*xrc -o xrc_res.cc

compact: all
	strip $(TARGET)
	upx -9 $(TARGET)

$(PROG): $(OBJS) $(LIBS)
	$(CXX) $(OBJS) $(LIBS) $(SYSLIBS) -o $(PROG)

$(PROG).app: $(OBJS) $(LIBS)
	mkdir -p $(PROG).app/Contents/MacOS $(PROG).app/Contents/Resources
	cp `wx-config --prefix`/lib/libwx_mac-2.4*.rsrc $(PROG).app/Contents/Resources/$(PROG).rsrc
	echo -n "APPL????" > $(PROG).app/Contents/PkgInfo
	sed "s/@VERSION@/$(VERSION)/g" < Info.plist > $(PROG).app/Contents/Info.plist 
	cp iaxcomm.icns  $(PROG).app/Contents/Resources
	cp -r rc $(PROG).app/Contents/Resources
	$(CXX) $(OBJS) $(LIBS) $(SYSLIBS) -o $(PROG).app/Contents/MacOS/$(PROG)
