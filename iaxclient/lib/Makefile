
CFLAGS=-Igsm/inc -Ilibiax/src

OBJS=\
	winfuncs.o \
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
	audio_win32.o \
	iaxclient_lib.o 

all: libiaxclient.a

libiaxclient.a: $(OBJS)
	-rm libiaxclient.a
	ar cru libiaxclient.a $(OBJS)

