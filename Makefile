#
#   This software is Copyright 2011 by Sean Groarke <sgroarke@gmail.com>
#   All rights reserved.
#
#   This file is part of npd6.
#
#   npd6 is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   npd6 is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with npd6.  If not, see <http://www.gnu.org/licenses/>.
#

# $Id$
# $HeadURL$

VERSION=1.1.0

CC=gcc
CFLAGS= -Os -I./ 
LDFLAGS=
SOURCES=main.c icmp6.c util.c ip6.c config.c expintf.c exparser.c global.c
OBJECTS=$(SOURCES:.c=.o)
HEADERS=includes.h npd6.h
EXECUTABLE=npd6
INSTALL_PREFIX=/usr
MAN_PREFIX=/usr/share/man
DEBIAN=debian/
TARGZ=npd6-$(VERSION)
DEV:= -D'BUILDREV="$(VERSION).$(shell git describe --always )"'

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(DEV) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

distclean:
	rm -rf $(OBJECTS) $(EXECUTABLE)
	rm -rf debian/etc/
	rm -rf debian/usr/
	rm -rf debian/DEBIAN/
	rm -rf debian/npd6/
	rm -rf debian/files
	rm -rf debian/npd6.*
	rm -f npd6*.deb

install: all
	mkdir -p $(DESTDIR)/etc/init.d/
	mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/bin/
	mkdir -p $(DESTDIR)$(MAN_PREFIX)/man5/
	mkdir -p $(DESTDIR)$(MAN_PREFIX)/man8/
	cp etc/npd6 $(DESTDIR)/etc/init.d/npd6
	cp etc/npd6.conf.sample $(DESTDIR)/etc/npd6.conf.sample
	cp npd6 $(DESTDIR)$(INSTALL_PREFIX)/bin/
	cp man/npd6.conf.5.gz $(DESTDIR)$(MAN_PREFIX)/man5/
	cp man/npd6.8.gz $(DESTDIR)$(MAN_PREFIX)/man8/

ubuntu: all
	mkdir $(DEBIAN)/DEBIAN/
	cp $(DEBIAN)/c* $(DEBIAN)/DEBIAN/
	mkdir -p $(DEBIAN)/etc/init.d/
	mkdir -p $(DEBIAN)$(INSTALL_PREFIX)/bin/
	mkdir -p $(DEBIAN)$(MAN_PREFIX)/man5/
	mkdir -p $(DEBIAN)$(MAN_PREFIX)/man8/
	cp etc/npd6 $(DEBIAN)/etc/init.d/npd6
	cp etc/npd6.conf.sample $(DEBIAN)/etc/npd6.conf.sample
	cp npd6 $(DEBIAN)$(INSTALL_PREFIX)/bin/
	cp man/npd6.conf.5.gz $(DEBIAN)$(MAN_PREFIX)/man5/
	cp man/npd6.8.gz $(DEBIAN)$(MAN_PREFIX)/man8/
	debuild -S -k93C35BB8


debian: all
	mkdir $(DEBIAN)/DEBIAN/
	cp $(DEBIAN)/c* $(DEBIAN)/DEBIAN/
	mkdir -p $(DEBIAN)/etc/init.d/
	mkdir -p $(DEBIAN)$(INSTALL_PREFIX)/bin/
	mkdir -p $(DEBIAN)$(MAN_PREFIX)/man5/
	mkdir -p $(DEBIAN)$(MAN_PREFIX)/man8/
	cp etc/npd6 $(DEBIAN)/etc/init.d/npd6
	cp etc/npd6.conf.sample $(DEBIAN)/etc/npd6.conf.sample
	cp npd6 $(DEBIAN)$(INSTALL_PREFIX)/bin/
	cp man/npd6.conf.5.gz $(DEBIAN)$(MAN_PREFIX)/man5/
	cp man/npd6.8.gz $(DEBIAN)$(MAN_PREFIX)/man8/
	debuild -I -us -uc 

debchange:
	dch -v $(VERSION) $(change)

debrelease:
	dch --release -M

targz:
	mkdir ../$(TARGZ)
	mkdir ../$(TARGZ)/man
	mkdir ../$(TARGZ)/etc
	mkdir ../$(TARGZ)/debian
	cp man/* ../$(TARGZ)/man
	cp etc/* ../$(TARGZ)/etc
	cp *.c ../$(TARGZ)/
	cp *.h ../$(TARGZ)/
	cp INSTALL ../$(TARGZ)
	cp debian/c* ../$(TARGZ)/debian
	cp debian/rules ../$(TARGZ)/debian
	cp Makefile ../$(TARGZ)/
	tar -cvf ../$(TARGZ).tar ../$(TARGZ)/*
	gzip ../$(TARGZ).tar
	rm -r ../$(TARGZ)/

