
# set to 2 for IAX2
IAXVER=2

SPEEX_PREPROCESS=1
SPAN_EC=0
SPEEX_EC=1
MEC2_EC=0

CFLAGS=-Igsm/inc -Iportaudio/pa_common -Iportaudio/pablio -Iportmixer/px_common

CFLAGS:= $(CFLAGS) -g -Wall -Wpointer-arith
#CFLAGS:= $(CFLAGS) -g -O2

OBJS=\
	gsm/src/add.o \
	gsm/src/code.o \
	gsm/src/debug.o \
	gsm/src/decode.o \
	gsm/src/gsm_create.o \
	gsm/src/gsm_decode.o \
	gsm/src/gsm_destroy.o \
	gsm/src/gsm_encode.o \
	gsm/src/gsm_explode.o \
	gsm/src/gsm_implode.o \
	gsm/src/gsm_option.o \
	gsm/src/gsm_print.o \
	gsm/src/long_term.o \
	gsm/src/lpc.o \
	gsm/src/preprocess.o \
	gsm/src/rpe.o \
	gsm/src/short_term.o \
	gsm/src/table.o \
	portaudio/pa_common/pa_lib.o \
	portaudio/pablio/pablio.o \
	portaudio/pablio/ringbuffer.o \
	audio_encode.o \
	audio_portaudio.o \
	audio_file.o \
	codec_gsm.o \
	codec_ulaw.o \
	iaxclient_lib.o 

OBJS_WIN32=\
	winfuncs.o \
	portmixer/px_win_wmme/px_win_wmme.o \
	portaudio/pa_win_wmme/pa_win_wmme.o


OBJS_LINUX=\
	unixfuncs.o \
	portmixer/px_unix_oss/px_unix_oss.o \
	portaudio/pa_unix_oss/pa_unix_oss.o \
	portaudio/pa_unix_oss/pa_unix.o 

OBJS_SOLARIS=\
	unixfuncs.o \
	portmixer/px_solaris/px_solaris.o \
	portaudio/pa_unix_oss/pa_unix_solaris.o \
	portaudio/pa_unix_oss/pa_unix.o 

OBJS_MACOSX=\
	unixfuncs.o \
	portmixer/px_mac_core/px_mac_core.o \
	portaudio/pa_common/pa_convert.o \
	portaudio/pa_mac_core/pa_mac_core.o 

OBJS_IAX1=\
	libiax/src/md5.o \
	libiax/src/iax.o \

OBJS_IAX2=\
        libiax2/src/md5.o \
        libiax2/src/iax.o \
        libiax2/src/iax2-parser.o \



LIB=libiaxclient.a

RANLIB= echo

all: $(LIB)

# glean the platform
# We will define this as either Linux, Win32, SOLARIS, or MacOSX
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
$(warning OSTYPE cannot be detected, assuming Linux)
OSTYPE=LINUX
endif
endif
endif
endif
endif

# Setup platform-specific stuff based on gleaning
ifeq ($(OSTYPE),WIN32)
OBJS:= $(OBJS) $(OBJS_WIN32) 
endif

ifeq ($(OSTYPE),LINUX)
OBJS:= $(OBJS) $(OBJS_LINUX) 
endif

ifeq ($(OSTYPE),SOLARIS)
OBJS:= $(OBJS) $(OBJS_SOLARIS) 
CFLAGS:= $(CFLAGS) -DALIGN32 
CC=gcc
endif

ifeq ($(OSTYPE),MACOSX)
OBJS:= $(OBJS) $(OBJS_MACOSX) 
RANLIB= ranlib
# comment these lines to build against current OS, and not 10.1.5.
#CC=MACOSX_DEPLOYMENT_TARGET=10.1 NEXT_ROOT=/Developer/SDKs/MacOSX10.1.5.sdk cc
#CFLAGS:= $(CFLAGS) -isystem /Developer/SDKs/MacOSX10.1.5.sdk/usr/include
endif


ifeq ($(IAXVER),1)
OBJS:= $(OBJS) $(OBJS_IAX1)
CFLAGS:= $(CFLAGS) -Ilibiax/src
else
OBJS:= $(OBJS) $(OBJS_IAX2)
#CFLAGS:= $(CFLAGS) -Ilibiax2/src -DIAXC_IAX2 -DLIBIAX -DDEBUG_SUPPORT 
CFLAGS:= $(CFLAGS) -Ilibiax2/src -DIAXC_IAX2 -DLIBIAX 
endif

ifeq ($(SPEEX_PREPROCESS),1)
OBJS:= $(OBJS) libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o libspeex/mdf.o
CFLAGS:= $(CFLAGS) -DSPEEX_PREPROCESS=1
endif

ifeq ($(SPAN_EC),1)
OBJS:= $(OBJS) ec/echo.o
CFLAGS:= $(CFLAGS) -DSPAN_EC=1
endif

ifeq ($(SPEEX_EC),1)
CFLAGS:= $(CFLAGS) -DSPEEX_EC=1
endif

ifeq ($(MEC2_EC),1)
CFLAGS:= $(CFLAGS) -DUSE_MEC2=1
endif

CFLAGS:= $(CFLAGS) -D$(OSTYPE)


$(LIB): $(OBJS)
	rm -f $(LIB)
	ar cru $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

MSLIB=iaxclient.lib
MGLIBDIR=c:/mingw/lib
GCCLIBDIR=$(MGLIBDIR)/gcc-lib/mingw32/3.2/

GCCLIB=$(GCCLIBDIR)/libgcc.a 


$(MSLIB): $(OBJS)
#	rm -f $(MSLIB)
	cp $(GCCLIB) $(MSLIB)
	ar rs $(MSLIB) $(OBJS) 

plat:
	echo Platform is $(OSTYPE)
	echo OBJS are $(OBJS)
	echo OBJS_LINUX are $(OBJS_LINUX)

clean:
	rm -f $(OBJS) $(LIB)
