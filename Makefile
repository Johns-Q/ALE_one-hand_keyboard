
CC=	gcc
CFLAGS=	-Os -pipe -W -Wall

aohk.o daemon.o:	aohk.h

aoh-daemon:	daemon.o aohk.o
	$(CC) -o $@ $^


clean:
	rm *.o *~
