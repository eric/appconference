
EXEEXT=.exe

CFLAGS=-I../../lib -I../../lib/libiax/src -I../../lib/gsm/inc -Iinclude

OBJS=	simpleiax.o

LIBS=	../../lib/libiaxclient.a


all: simpleiax$(EXEEXT)

../../lib/libiaxclient.a:
	(cd ../../lib; make libiaxclient.a)

simpleiax.exe: $(OBJS) $(LIBS)
	gcc $(OBJS) $(LIBS) -lwinmm -lwsock32 -o simpleiax$(EXEEXT)

