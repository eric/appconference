#
# iaxclient: a cross-platform IAX softphone library
#
# Copyrights:
# Copyright (C) 2003 HorizonLive.com, (c) 2004, Horizon Wimba, Inc.
#
# Contributors <see README for complete list>
# Steve Kann <stevek@stevek.com>
# Michael Van Donselaar <mvand@vandonselaar.org>
# Shawn Lawrence <shawn.lawrence@terracecomm.com>
#
#
# This program is free software, distributed under the terms of
# the GNU Lesser (Library) General Public License
#



CODEC_ILBC=0

SPEEX_PREPROCESS=1
SPAN_EC=0
SPEEX_EC=1
MEC2_EC=0

USE_NEWJB=1

USE_VIDEO=0

CFLAGS= -I. -Igsm/inc \
	-Iportaudio/pa_common -Iportaudio/pablio -Iportmixer/px_common \
	-Ilibspeex/include --std=c99 -Wall -Wmissing-noreturn

#CFLAGS:= $(CFLAGS) -g -Wall -Wpointer-arith
CFLAGS:= $(CFLAGS) -g -O2

CFLAGS:= $(CFLAGS) -DLIBVER='"CVS-$(shell date +%Y/%m/%d-%H:%M)"'

ifeq ($(SPEEX_PREPROCESS),1)
CFLAGS:= $(CFLAGS) -DSPEEX_PREPROCESS=1
endif

ifeq ($(USE_NEWJB),1)
CFLAGS:= $(CFLAGS) -DNEWJB
endif

# debug flags
# CFLAGS := $(CFLAGS) -DDEBUG_SUPPORT -DDEBUG_DEFAULT -DEXTREME_DEBUG -D_DEBUG 
# CFLAGS := $(CFLAGS) -DDEBUG_SUPPORT -DDEBUG_DEFAULT -D_DEBUG

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
	spandsp/plc.o \
	audio_encode.o \
	audio_portaudio.o \
	audio_file.o \
	codec_gsm.o \
	codec_ulaw.o \
	codec_alaw.o \
	codec_speex.o \
	jitterbuf.o \
	iaxclient_lib.o 

OBJS_LIBSPEEX=\
        libspeex/nb_celp.o \
        libspeex/sb_celp.o \
        libspeex/lpc.o \
        libspeex/ltp.o \
        libspeex/lsp.o \
        libspeex/quant_lsp.o \
        libspeex/lsp_tables_nb.o \
        libspeex/gain_table.o \
        libspeex/gain_table_lbr.o \
        libspeex/cb_search.o \
        libspeex/filters.o \
        libspeex/bits.o \
        libspeex/modes.o \
        libspeex/speex.o \
        libspeex/vq.o \
        libspeex/high_lsp_tables.o \
        libspeex/vbr.o \
        libspeex/hexc_table.o \
        libspeex/exc_5_256_table.o \
        libspeex/exc_5_64_table.o \
        libspeex/exc_8_128_table.o \
        libspeex/exc_10_32_table.o \
        libspeex/exc_10_16_table.o \
        libspeex/exc_20_32_table.o \
        libspeex/hexc_10_32_table.o \
        libspeex/misc.o \
        libspeex/speex_header.o \
        libspeex/speex_callbacks.o \
        libspeex/math_approx.o \
        libspeex/stereo.o \
        libspeex/preprocess.o \
        libspeex/smallft.o \
        libspeex/lbr_48k_tables.o \
        libspeex/jitter.o \
        libspeex/mdf.o

OBJS_ILBC=\
	iLBC/anaFilter.o\
	iLBC/iCBSearch.o\
	iLBC/packing.o\
	iLBC/constants.o\
	iLBC/gainquant.o\
	iLBC/iLBC_decode.o\
	iLBC/StateConstructW.o \
	iLBC/createCB.o\
	iLBC/getCBvec.o\
	iLBC/iLBC_encode.o\
	iLBC/StateSearchW.o\
	iLBC/doCPLC.o\
	iLBC/helpfun.o\
	iLBC/syntFilter.o\
	iLBC/enhancer.o\
	iLBC/hpInput.o\
	iLBC/LPCdecode.o\
	iLBC/filter.o\
	iLBC/hpOutput.o\
	iLBC/LPCencode.o\
	iLBC/FrameClassify.o\
	iLBC/iCBConstruct.o\
	iLBC/lsf.o

OBJS_VIDEO=\
	video.o\
	video_portvideo.o\
	PortVideoSDL/common/cameraTool.o\
	PortVideoSDL/common/RingBuffer.o\
	PortVideoSDL/common/FrameInverter.o\
	codec_theora.o


OBJS:= $(OBJS) $(OBJS_LIBSPEEX) 

ifeq ($(CODEC_ILBC),1)
OBJS:= $(OBJS) $(OBJS_ILBC) codec_ilbc.o
CFLAGS:= $(CFLAGS) -DCODEC_ILBC
endif

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
CFLAGS:= -mno-cygwin $(CFLAGS)
DYNLDFLAGS= -mno-cygwin -lwinmm -lwsock32 -lcomdlg32 -luser32 -lgdi32 -lole32 -lcomctl32 -lctl3d32 \
		-lgcc -lstdc++ -lshell32 -loleaut32 -ladvapi32 -luuid -Wl,--out-implib,libiaxclient.a
DYNCFLAGS=-DBUILDING_DLL
DYNLIB=iaxclient.dll
endif

ifeq ($(OSTYPE),LINUX)
OBJS:= $(OBJS) $(OBJS_LINUX) 
DYNCFLAGS=-fPIC
DYNLIB=libiaxclient.so
# change below to 1 to include audio_alsa;
AUDIO_ALSA=0  
endif

ifeq ($(OSTYPE),SOLARIS)
OBJS:= $(OBJS) $(OBJS_SOLARIS) 
# only SPARC solaris machines need the alignment stuff
ifeq (sparc,$(findstring sparc,$(shell uname -a)))
CFLAGS:= $(CFLAGS) -DALIGN32 
endif
DYNCFLAGS=-fPIC
DYNLIB=libiaxclient.so
CC=gcc
endif

ifeq ($(OSTYPE),MACOSX)
OBJS:= $(OBJS) $(OBJS_MACOSX) 
RANLIB= ranlib
# comment these lines in to build against version  10.2.7.
#CC=MACOSX_DEPLOYMENT_TARGET=10.2 NEXT_ROOT=/Developer/SDKs/MacOSX10.2.7.sdk cc
#CFLAGS:= $(CFLAGS) -isystem /Developer/SDKs/MacOSX10.2.7.sdk/usr/include
DYNLIB=libiaxclient.dylib
endif

ifeq ($(USE_VIDEO),1)
	OBJS:= $(OBJS) $(OBJS_VIDEO) 
	CFLAGS:= $(CFLAGS) -DIAXC_VIDEO
	LIBS:= $(LIBS) -lavcodec -ltheora
  ifeq ($(OSTYPE),LINUX)
	OBJS:= $(OBJS) PortVideoSDL/linux/linuxfwCamera.o PortVideoSDL/linux/v4linuxCamera.o
  endif
  ifeq ($(OSTYPE),WIN32)
	CFLAGS:= $(CFLAGS) -IPortVideoSDL/win32/dslib/include -Ic:/DX90SDK/include-gcc -Ic:/DX90SDK/Samples/C++/DirectShow/BaseClasses
	OBJS:= $(OBJS) PortVideoSDL/win32/dslibCamera.cpp
  endif
  ifeq ($(OSTYPE),MACOSX)
        CFLAGS:= $(CFLAGS) -I../../theora/include -I/sw/include
        OBJS:= $(OBJS) PortVideoSDL/macosx/macvdCamera.o PortVideoSDL/macosx/vdigGrab.o
  endif
endif


ifeq ($(AUDIO_ALSA),1)
OBJS:= $(OBJS) audio_alsa.o
CFLAGS:= $(CFLAGS) -DAUDIO_ALSA
endif


OBJS:= $(OBJS) $(OBJS_IAX2)
#CFLAGS:= $(CFLAGS) -Ilibiax2/src -DIAXC_IAX2 -DLIBIAX -DDEBUG_SUPPORT 
CFLAGS:= $(CFLAGS) -Ilibiax2/src -DLIBIAX 

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

CXXFLAGS=$(CFLAGS)

$(MSLIB): $(OBJS)
#	rm -f $(MSLIB)
	cp $(GCCLIB) $(MSLIB)
	ar rs $(MSLIB) $(OBJS) 


shared: CFLAGS := $(CFLAGS) $(DYNCFLAGS)
#shared: clean libiaxclient.a
shared: libiaxclient.a
ifeq ($(OSTYPE),MACOSX)
	libtool -single_module -dynamic -o libiaxclient.dylib libiaxclient.a -lc -framework CoreAudio -framework AudioToolbox -lcc_dynamic
else
	$(CC) -shared -o $(DYNLIB) $(OBJS) $(DYNLDFLAGS)
endif



plat:
	echo Platform is $(OSTYPE)
	echo OBJS are $(OBJS)
	echo OBJS_LINUX are $(OBJS_LINUX)

clean:
	rm -f $(OBJS) $(LIB)
