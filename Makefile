##
##	@name Makefile	-	ALE OneHand Keyboard Makefile
##
##	Copyright (c) 2007,2009 by Lutz Sammer.  All Rights Reserved.
##
##	Contributor(s):
##
##	This file is part of ALE one-hand keyboard
##
##	This program is free software; you can redistribute it and/or modify
##	it under the terms of the GNU General Public License as published by
##	the Free Software Foundation; only version 2 of the License.
##
##	This program is distributed in the hope that it will be useful,
##	but WITHOUT ANY WARRANTY; without even the implied warranty of
##	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##	GNU General Public License for more details.
##
##	$Id$
############################################################################

CC=	gcc
#CFLAGS=	-g -pipe -W -Wall -W
CFLAGS=	-g -Os -pipe -W -Wall -W

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
	one-hand/q1.map one-hand/pc102leftside.map \
	one-hand/pc102numpad.map one-hand/pc102rotated.map \
	one-hand/o2.typ

indent:
	for i in $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done
clean:
	-rm *.o *~

clobber:	clean
	rm aohk-daemon

install:	all
	install -d /usr/local/bin
	install -d /usr/local/lib/aohk
	install -s aohk-daemon /usr/local/bin
	install *.map /usr/local/lib/aohk
