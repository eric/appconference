
# set to 2 for IAX2
IAXVER=2

SPEEX_PREPROCESS=1
SPAN_EC=0
SPEEX_EC=0

CFLAGS=-Igsm/inc -Iportaudio/pa_common -Iportaudio/pablio

CFLAGS:= $(CFLAGS) -g

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
	sox/compand.o \
	sox/soxcompat.o \
	iaxclient_lib.o 

OBJS_WIN32=\
	winfuncs.o \
	portaudio/pa_win_wmme/pa_win_wmme.o


OBJS_LINUX=\
	unixfuncs.o \
	portaudio/pa_unix_oss/pa_unix_oss.o \
	portaudio/pa_unix_oss/pa_unix.o 

OBJS_MACOSX=\
	unixfuncs.o \
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
$(warning OSTYPE cannot be detected, assuming Linux)
OSTYPE=LINUX
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

ifeq ($(OSTYPE),MACOSX)
OBJS:= $(OBJS) $(OBJS_MACOSX) 
RANLIB= ranlib
endif


ifeq ($(IAXVER),1)
OBJS:= $(OBJS) $(OBJS_IAX1)
CFLAGS:= $(CFLAGS) -Ilibiax/src
else
OBJS:= $(OBJS) $(OBJS_IAX2)
CFLAGS:= $(CFLAGS) -Ilibiax2/src -DIAXC_IAX2 -DLIBIAX -DDEBUG_SUPPORT 
endif

ifeq ($(SPEEX_PREPROCESS),1)
OBJS:= $(OBJS) libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o libspeex/mdf.o
CFLAGS:= $(CFLAGS) -DSPEEX_PREPROCESS=1
endif

ifeq ($(SPAN_EC),1)
OBJS:= $(OBJS) ec/echo.o
CFLAGS:= $(CFLAGS) -DSPAN_EC=1
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
