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

VERSION = "0.90"
GIT_REV	= "`git describe --always 2>/dev/null`"

CC	= gcc
CFLAGS	= -g -O0 -pipe -W -Wall -W \
	-DVERSION=\"$(VERSION)\" -DGIT_REV=\"$(GIT_REV)\"

OBJS	= daemon.o aohk.o uinput.o
HDRS	= aohk.h uinput.h

all:	aohkd # btvhid xvaohk

$(OBJS):	$(HDRS) Makefile

aohkd:	$(OBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

#----------------------------------------------------------------------------

BTOBJS	= btvhid.o
BTHDRS	= btvhid.h
BTLIBS	= -lbluetooth

$(BTOBJS):	$(BTHDRS) Makefile

btvhid:	$(BTOBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(BTLIBS)

#----------------------------------------------------------------------------

XVOBJS	= xvaohk.o uinput.o aohk.o
XVHDRS	= xvaohk.h uinput.h aohk.h
XVLIBS	= `pkg-config --libs xcb-icccm xcb-shape xcb-image xcb`

$(XVOBJS):	$(XVHDRS) Makefile xvaohk.xpm

xvaohk:	$(XVOBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(XVLIBS)

#----------------------------------------------------------------------------
#	Developer tools

indent:
	for i in $(XVOBJS:.o=.c) $(BTOBJS:.o=.c) $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done
clean:
	-rm *.o *~

clobber:	clean
	rm aohkd btvhid xvaohk


#----------------------------------------------------------------------------

.PHONY: doc

doc:	$(SRCS) $(HDRS) aohkd.doxygen
	(cat aohkd.doxygen;\
	echo "PROJECT_NUMBER=${VERSION} (GIT-$(GIT_REV))") | doxygen -

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
