.PHONY: all clean distclean

CC=gcc

FMT=clang-format

MKBOOTIMAGE_NAME:=mkbootimage
EXBOOTIMAGE_NAME:=exbootimage

VERSION_MAJOR:=2.3
VERSION_MINOR:=$(shell git rev-parse --short HEAD)

VERSION:=$(MKBOOTIMAGE_NAME) $(VERSION_MAJOR)-$(VERSION_MINOR)

COMMON_SRCS:=src/bif.c src/bootrom.c src/common.c \
	 $(wildcard src/arch/*.c) $(wildcard src/file/*.c)

COMMON_HDRS:=src/bif.h src/bootrom.h src/common.h \
	 $(wildcard src/arch/*.h) $(wildcard src/file/*.h)

MKBOOTIMAGE_SRCS:=$(COMMON_SRCS) src/mkbootimage.c
MKBOOTIMAGE_OBJS:=$(MKBOOTIMAGE_SRCS:.c=.o)

EXBOOTIMAGE_SRCS:=$(COMMON_SRCS) src/exbootimage.c
EXBOOTIMAGE_OBJS:=$(EXBOOTIMAGE_SRCS:.c=.o)

ALL_SRCS:=$(COMMON_SRCS) src/mkbootimage.c src/exbootimage.c
ALL_HDRS:=$(COMMON_HDRS)

INCLUDE_DIRS:=src

override CFLAGS += $(foreach includedir,$(INCLUDE_DIRS),-I$(includedir)) \
	-DMKBOOTIMAGE_VER="\"$(VERSION)\"" \
	-Wall -Wextra -Wpedantic \
	--std=c11

LDLIBS = -lelf

all: $(MKBOOTIMAGE_NAME) $(EXBOOTIMAGE_NAME)

$(MKBOOTIMAGE_NAME): $(MKBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(MKBOOTIMAGE_OBJS) -o $(MKBOOTIMAGE_NAME) $(LDLIBS)

$(EXBOOTIMAGE_NAME): $(EXBOOTIMAGE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(EXBOOTIMAGE_OBJS) -o $(EXBOOTIMAGE_NAME) $(LDLIBS)

format:
	$(FMT) -i $(ALL_SRCS) $(ALL_HDRS)

clean:
	@- $(RM) $(MKBOOTIMAGE_NAME)
	@- $(RM) $(MKBOOTIMAGE_OBJS)
	@- $(RM) $(EXBOOTIMAGE_NAME)
	@- $(RM) $(EXBOOTIMAGE_OBJS)

distclean: clean
