.PHONY: all clean distclean

CC=gcc

MKBOOTIMAGE_NAME:=mkbootimage

VERSION_MAJOR:=1.0
VERSION_MINOR:=${shell git rev-parse --short HEAD}

VERSION:=${MKBOOTIMAGE_NAME} ${VERSION_MAJOR}-${VERSION_MINOR}

MKBOOTIMAGE_SRCS:=$(wildcard src/*.c) $(wildcard src/arch/*c)
MKBOOTIMAGE_OBJS:=${MKBOOTIMAGE_SRCS:.c=.o}

MKBOOTIMAGE_INCLUDE_DIRS:=src

CFLAGS += $(foreach includedir,$(MKBOOTIMAGE_INCLUDE_DIRS),-I$(includedir)) \
	-DMKBOOTIMAGE_VER="\"$(VERSION)\"" \
	-lpcre -lelf -Wall -Wextra

all: $(MKBOOTIMAGE_NAME)

$(MKBOOTIMAGE_NAME): $(MKBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(MKBOOTIMAGE_OBJS) -o $(MKBOOTIMAGE_NAME)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@- $(RM) $(MKBOOTIMAGE_NAME)
	@- $(RM) $(MKBOOTIMAGE_OBJS)

distclean: clean
