
CC=	gcc
CFLAGS=	-g -pipe -W -Wall -W
#CFLAGS=	-g -Os -pipe -W -Wall -W

OBJS=	daemon.o aohk.o
HDRS=	aohk.h

all:	aohk-daemon

aohk.o daemon.o:	aohk.h

aohk-daemon:	daemon.o aohk.o
	$(CC) -o $@ $^

dist:
	cd ..; \
	tar -czf "one-hand/aohk-src-`date +%y%m%d`.tar.gz" one-hand/*.c \
	one-hand/*.h one-hand/Makefile one-hand/*.txt \
	one-hand/aohk-refcard.svgz one-hand/aohk-refcard.png \
	one-hand/us.default.map one-hand/de.default.map \
	one-hand/pc102leftside.map \
	one-hand/pc102numpad.map one-hand/pc102rotated.map \
	one-hand/o2.typ

clean:
	-rm *.o *~

clobber:	clean
	rm aohk-daemon
