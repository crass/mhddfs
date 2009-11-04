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

FORTAR	=	src COPYING LICENSE README Makefile \
		README.ru.UTF-8 ChangeLog mhddfs.1 \
		debian

VERSION	=	$(shell cat src/version.h  \
	|grep '^.define'|grep '[[:space:]]VERSION[[:space:]]' \
	|awk '{print $$3}'|sed 's/\"//g' )
RELEASE	=	0

SRCDIR	=	$(shell rpm --eval '%_sourcedir')

all: $(TARGET)

tarball: mhddfs_$(VERSION).tar.gz
	@echo '>>>> mhddfs_$(VERSION).tar.gz created'

mhddfs_$(VERSION).tar.gz: $(FORTAR) $(wildcard src/*) 
	mkdir mhddfs-$(VERSION)
	cp -r $(FORTAR) mhddfs-$(VERSION)
	tar --exclude=.svn -czvf $@ mhddfs-$(VERSION)
	rm -fr mhddfs-$(VERSION)

rpm: tarball
	mkdir -p $(SRCDIR)
	mv -f mhddfs_$(VERSION).tar.gz $(SRCDIR)
	rpmbuild -ba mhddfs.spec --define 'version $(VERSION)' --define 'release $(RELEASE)'
	cp $(shell rpm --eval '%_srcrpmdir')/mhddfs-$(VERSION)-$(RELEASE)* \
		$(shell rpm --eval '%_rpmdir')/*/mhddfs-*$(VERSION)-$(RELEASE)* .

$(TARGET): obj/obj-stamp $(OBJ)
	gcc $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

obj/obj-stamp:
	mkdir -p obj
	touch $@

obj/%.o: src/%.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -fr obj $(TARGET) pwrite_test statvfs

release_svn_thread:
	@echo current version $(VERSION)
	if ! svn ls http://svn.uvw.ru/mhddfs/tags| \
		grep -q release_$(VERSION); then \
		svn copy -m release-$(VERSION) \
			http://svn.uvw.ru/mhddfs/trunk \
			http://svn.uvw.ru/mhddfs/tags/release_$(VERSION); \
	fi

open_project:
	screen -t vim vim Makefile src/*.[ch] README* ChangeLog mhddfs.1

pwrite_test: src/test/pwrite.c
	gcc -o $@ $<

statvfs: src/test/statvfs.c
	gcc -o $@ $<

images-mount: test1.img test2.img
	mount|grep -q `pwd`/test1 || sudo mount -o loop test1.img test1
	mount|grep -q `pwd`/test2 || sudo mount -o loop test2.img test2
	sudo chown -R `whoami` test1 test2

test-mount:
	rm -f logfile.log
	mount|grep -q `pwd`/mnt && make test-umount || true
	./$(TARGET) test1 test2 mnt -o \
		logfile=logfile.log,mlimit=50m,nonempty,loglevel=0
	df -h test1 test2
	ls mnt

test-umount:
	-fusermount -u mnt

test1.img:
	dd if=/dev/zero of=$@ bs=1M count=100
	echo y|/sbin/mkfs.ext3 $@

test2.img:
	dd if=/dev/zero of=$@ bs=1M count=200
	echo y|/sbin/mkfs.ext3 $@

test:
	file_name=`mktemp -p mnt file_XXXXXXXXXX`; \
		echo $$file_name; \
		dd if=/dev/urandom of=$$file_name bs=1M count=1 2> /dev/null

btest:
	file_name=`mktemp -p mnt file_XXXXXXXXXX`; \
		echo $$file_name; \
		dd if=/dev/urandom bs=1M count=65 2> /dev/null \
			|tee $$file_name \
			|md5sum; \
		md5sum $$file_name
		

tests:
	while make test; do echo ok; echo; done

ptest:
	gcc -o $@ tests/plocks.c -l pthread
	-./$@
	rm -f $@

test-images: test1.img test2.img

.PHONY: all clean open_project tarball \
	release_svn_thread test-mount test-umount \
	images-mount test tests

include $(wildcard obj/*.d)
