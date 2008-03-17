SRC	=	$(wildcard src/*.c)

OBJ	=	$(SRC:src/%.c=obj/%.o)

TARGET	=	mhddfs

CFLAGS	=	-Wall $(shell pkg-config fuse --cflags) -DFUSE_USE_VERSION=26
LDFLAGS	=	$(shell pkg-config fuse --libs)

all: $(TARGET)

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
	screen -t vim vim Makefile src/*

.PHONY: all clean open_project
