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

GIT_REV =       "`git describe --always 2>/dev/null`"

CC=	gcc
#CFLAGS=	-g -pipe -W -Wall -W
CFLAGS=	-g -Os -pipe -W -Wall -W

OBJS=	daemon.o aohk.o
HDRS=	aohk.h

all:	aohkd

aohk.o daemon.o:	aohk.h

aohkd:	daemon.o aohk.o
	$(CC) -o $@ $^

#----------------------------------------------------------------------------
#	Developer tools

indent:
	for i in $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done
clean:
	-rm *.o *~

clobber:	clean
	rm aohkd

#----------------------------------------------------------------------------

.PHONY: doc

doc:	$(SRCS) $(HDRS) aohkd.doxygen
	(cat aohkd.doxygen;\
	echo "PROJECT_NUMBER=0.06 (GIT-$(GIT_REV))") | doxygen -

DISTDIR=	aohk-`date +%y%m%d`
FILES=	Makefile aohk-refcard.txt aohk.txt de.doc.txt doc.txt readme.txt \
	aohk-refcard.svgz aohk-refcard.png \
	us.default.map de.default.map pc102leftside.map pc102numpad.map \
	pc102rotated.map q1.map \
	o2.typ aohk.map.5 aohkd.1

dist:
	ln -s . $(DISTDIR); \
	tar -czf "aohkd-src-`date +%y%m%d`.tar.gz" \
	    $(addprefix $(DISTDIR)/, $(FILES) $(OBJS:.o=.c) $(HDRS)); \
	rm $(DISTDIR)

install:	all
	install -d /usr/local/bin
	install -d /usr/local/lib/aohk
	install -s aohkd /usr/local/bin
	install *.map /usr/local/lib/aohk
