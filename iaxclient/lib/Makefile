
CFLAGS=-Igsm/inc -Ilibiax/src

OBJS=\
	libiax/src/md5.o \
	libiax/src/iax.o \
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
	iaxclient_lib.o 

OBJS_WIN32=\
	audio_win32.o \
	winfuncs.o \

OBJS_LINUX=
OBJS_MACOSX=

LIB=libiaxclient.a

all: $(LIB)

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

# Setup platform-specific stuff based on gleaning
ifeq ($(OSTYPE),WIN32)
OBJS:= $(OBJS) $(OBJS_WIN32) 
endif

ifeq ($(OSTYPE),LINUX)
OBJS:= $(OBJS) $(OBJS_LINUX) 
endif

ifeq ($(OSTYPE),MACOSX)
OBJS:= $(OBJS) $(OBJS_WIN32) 
endif

CFLAGS:= $(CFLAGS) -D$(OSTYPE)


$(LIB): $(OBJS)
	rm -f $(LIB)
	ar cru $(LIB) $(OBJS)

plat:
	echo Platform is $(OSTYPE)

clean:
	rm -f $(OBJS) $(LIB)
