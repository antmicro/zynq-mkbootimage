.PHONY: all clean distclean

CC=gcc

MKBOOTIMAGE_NAME:=mkbootimage
EXBOOTIMAGE_NAME:=exbootimage

VERSION_MAJOR:=2.3
VERSION_MINOR:=$(shell git rev-parse --short HEAD)

VERSION:=$(MKBOOTIMAGE_NAME) $(VERSION_MAJOR)-$(VERSION_MINOR)

COMMON_SRCS:=src/bif.c src/bootrom.c src/common.c $(wildcard src/arch/*.c) $(wildcard src/file/*.c)

MKBOOTIMAGE_SRCS:=src/mkbootimage.c $(COMMON_SRCS)
MKBOOTIMAGE_OBJS:=$(MKBOOTIMAGE_SRCS:.c=.o)

EXBOOTIMAGE_SRCS:=src/exbootimage.c $(COMMON_SRCS)
EXBOOTIMAGE_OBJS:=$(EXBOOTIMAGE_SRCS:.c=.o)

INCLUDE_DIRS:=src

override CFLAGS += $(foreach includedir,$(INCLUDE_DIRS),-I$(includedir)) \
	-DMKBOOTIMAGE_VER="\"$(VERSION)\"" \
	-Wall -Wextra -Wpedantic \
	--std=c11

LDLIBS = -lelf

$(MKBOOTIMAGE_NAME): $(MKBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(MKBOOTIMAGE_OBJS) -o $(MKBOOTIMAGE_NAME) $(LDLIBS)

$(EXBOOTIMAGE_NAME): $(EXBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(EXBOOTIMAGE_OBJS) -o $(EXBOOTIMAGE_NAME) $(LDLIBS)

all: $(MKBOOTIMAGE_NAME) $(EXBOOTIMAGE_NAME)

clean:
	@- $(RM) $(MKBOOTIMAGE_NAME)
	@- $(RM) $(MKBOOTIMAGE_OBJS)
	@- $(RM) $(EXBOOTIMAGE_NAME)
	@- $(RM) $(EXBOOTIMAGE_OBJS)

distclean: clean

check:
	echo $(MKBOOTIMAGE_SRCS)
