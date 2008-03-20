#    mhddfs - Multi HDD [FUSE] File System
#    Copyright (C) 2008 Dmitry E. Oboukhov <dimka@avanto.org>

#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.

#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.

#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

SRC	=	$(wildcard src/*.c)

OBJ	=	$(SRC:src/%.c=obj/%.o)

DEPS	=	$(OBJ:obj/%.o=obj/%.d)

TARGET	=	mhddfs

CFLAGS	=	-Wall $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=26 -MMD
LDFLAGS	=	$(shell pkg-config fuse --libs)

FORTAR	=	src COPYING LICENSE README Makefile README.ru.UTF-8 ChangeLog

VERSION	=	$(shell cat src/version.h  \
	|grep '^.define'|grep '[[:space:]]VERSION[[:space:]]' \
	|awk '{print $$3}'|sed 's/\"//g' )

all: $(TARGET)

tarball: mhddfs_$(VERSION).tar.gz
	@echo '>>>> mhddfs_$(VERSION).tar.gz created'

mhddfs_$(VERSION).tar.gz: $(FORTAR) $(wildcard src/*)
	mkdir mhddfs-$(VERSION)
	cp -r $(FORTAR) mhddfs-$(VERSION)
	tar --exclude=.svn -czvf $@ mhddfs-$(VERSION)
	rm -fr mhddfs-$(VERSION)


$(TARGET): obj/obj-stamp $(OBJ)
	gcc $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

obj/obj-stamp:
	mkdir -p obj
	touch $@

obj/%.o: src/%.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -fr obj $(TARGET)

open_project:
	screen -t vim vim Makefile src/* README* ChangeLog

.PHONY: all clean open_project tarball

include $(wildcard obj/*.d)
